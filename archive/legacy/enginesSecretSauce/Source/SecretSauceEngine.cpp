#include "SecretSauceEngine.h"

// Embedded pole-shapes (subset) – A/B banks for one morph pair.
// These values mirror the uploaded JSON (48k ref) for "vowel_pair".
// A-bank (Ae)
static const char* kJSON_A = R"JSON(
{ "sampleRateRef": 48000, "shapes": [{
  "id": "vowel_pair",
  "poles": [
    { "r": 0.95,  "theta": 0.01047197551529928 },
    { "r": 0.96,  "theta": 0.01963495409118615 },
    { "r": 0.985, "theta": 0.03926990818237230 },
    { "r": 0.992, "theta": 0.11780972454711690 },
    { "r": 0.993, "theta": 0.32724923485310250 },
    { "r": 0.985, "theta": 0.45814892879434435 }
  ]}]}
)JSON";

// B-bank (Oo)
static const char* kJSON_B = R"JSON(
{ "sampleRateRef": 48000, "shapes": [{
  "id": "vowel_pair",
  "poles": [
    { "r": 0.96,  "theta": 0.00785398163647446 },
    { "r": 0.98,  "theta": 0.03141592614589800 },
    { "r": 0.985, "theta": 0.04450589600000000 },
    { "r": 0.992, "theta": 0.13089969394124100 },
    { "r": 0.99,  "theta": 0.28797932667073020 },
    { "r": 0.985, "theta": 0.39269908182372300 }
  ]}]}
)JSON";

void SecretSauceEngine::prepare (float sampleRate, int /*maxBlockSize*/)
{
    fs = juce::jlimit(8000.0f, 192000.0f, sampleRate);
    reset();

    // Initialize new EMU Z-plane engine with neutral defaults
    emuFilter.prepareToPlay(fs);
    emuFilter.setMorphPair(0);  // Use first morph pair
    emuFilter.setMorphPosition(0.5f);
    emuFilter.setIntensity(0.0f);  // Start neutral
    emuFilter.setDrive(0.0f);      // Start neutral
    emuFilter.setSectionSaturation(0.0f);
    emuFilter.setAutoMakeup(true); // Enable makeup gain

    // Legacy fallback setup
    loadEmbeddedShapes();
    setupDCBlocking();
    setupAntiAliasing();
    updateCoefficients();
}

void SecretSauceEngine::reset()
{
    emuFilter.reset();

    for (auto& s : leftChain)  s.clear();
    for (auto& s : rightChain) s.clear();
    leftDCBlock.clear();
    rightDCBlock.clear();
    leftAntiAlias.clear();
    rightAntiAlias.clear();
}

void SecretSauceEngine::setAmount (float amount01)
{
    amount = juce::jlimit(0.f, 1.f, amount01);

    // Map amount to EMU filter parameters with neutral defaults
    intensity = juce::jmap(amount, 0.0f, 1.0f, 0.0f, 0.75f); // Start from neutral
    const float driveDb = juce::jmap(amount, 0.0f, 1.0f, 0.0f, 4.0f); // 0-4dB range
    satAmt = juce::jmap(amount, 0.0f, 1.0f, 0.0f, 0.25f); // Conservative saturation

    // Update EMU filter with new parameters
    emuFilter.setIntensity(intensity);
    emuFilter.setDrive(driveDb);
    emuFilter.setSectionSaturation(satAmt);

    // Legacy parameters for fallback
    drive = std::pow(10.0f, driveDb / 20.0f);
    makeup = 1.0f / (1.0f + intensity * 0.5f);
    updateCoefficients();
}

void SecretSauceEngine::setSpeed (float speedHz)
{
    lfoSpeed = juce::jlimit(0.1f, 8.0f, speedHz);
    emuFilter.setLFORate(lfoSpeed);
}

void SecretSauceEngine::setDepth (float depth01)
{
    lfoDepth = juce::jlimit(0.0f, 1.0f, depth01);
    emuFilter.setLFODepth(lfoDepth);
}

void SecretSauceEngine::processStereo (float* left, float* right, int n)
{
    if (!left || !right || n <= 0) return;

    // Create a JUCE AudioBuffer from the input pointers for EMU processing
    juce::AudioBuffer<float> buffer(2, n);
    buffer.copyFrom(0, 0, left, n);
    buffer.copyFrom(1, 0, right, n);

    // Update morph position based on amount (subtle movement)
    const float baseMorph = 0.45f + 0.10f * amount;
    emuFilter.setMorphPosition(juce::jlimit(0.0f, 1.0f, baseMorph));

    // Process through the new EMU Z-plane engine
    emuFilter.process(buffer);

    // Copy processed data back to output pointers
    buffer.copyTo(0, 0, left, n);
    buffer.copyTo(1, 0, right, n);

    // Apply final safety limiting
    for (int i = 0; i < n; ++i) {
        left[i]  = juce::jlimit(-2.0f, 2.0f, left[i]);
        right[i] = juce::jlimit(-2.0f, 2.0f, right[i]);
    }
}

void SecretSauceEngine::loadEmbeddedShapes()
{
    auto parse = [](const char* json, std::array<Pole,6>& out){
        auto var = juce::JSON::parse(json);
        if (!var.isObject()) return;
        auto shapes = var.getProperty("shapes", juce::var());
        if (!shapes.isArray()) return;
        auto* arr = shapes.getArray();
        if (arr->isEmpty()) return;
        auto poles = (*arr)[0].getProperty("poles", juce::var());
        if (!poles.isArray()) return;
        auto* pArr = poles.getArray();
        for (int i = 0; i < 6 && i < pArr->size(); ++i)
        {
            auto pv = (*pArr)[i];
            out[(size_t)i] = Pole{ (float) pv.getProperty("r", 0.95f),
                                   (float) pv.getProperty("theta", 0.0f) };
        }
    };
    parse(kJSON_A, shapeA48k);
    parse(kJSON_B, shapeB48k);
}

void SecretSauceEngine::setupDCBlocking()
{
    // High-pass filter at ~5Hz to block DC and subsonic content
    const float fc = 5.0f / fs;
    const float w = juce::MathConstants<float>::twoPi * fc;
    const float cosw = std::cos(w);
    const float sinw = std::sin(w);
    const float alpha = sinw / (2.0f * 0.707f); // Q = 0.707

    // High-pass biquad coefficients
    const float norm = 1.0f + alpha;
    const float b0 = (1.0f + cosw) / (2.0f * norm);
    const float b1 = -(1.0f + cosw) / norm;
    const float b2 = b0;
    const float a1 = -2.0f * cosw / norm;
    const float a2 = (1.0f - alpha) / norm;

    leftDCBlock.b0 = b0; leftDCBlock.b1 = b1; leftDCBlock.b2 = b2;
    leftDCBlock.a1 = a1; leftDCBlock.a2 = a2;

    rightDCBlock.b0 = b0; rightDCBlock.b1 = b1; rightDCBlock.b2 = b2;
    rightDCBlock.a1 = a1; rightDCBlock.a2 = a2;
}

void SecretSauceEngine::setupAntiAliasing()
{
    // Low-pass filter at 18kHz to prevent aliasing
    const float fc = 18000.0f / fs;
    const float w = juce::MathConstants<float>::twoPi * fc;
    const float cosw = std::cos(w);
    const float sinw = std::sin(w);
    const float alpha = sinw / (2.0f * 0.707f); // Q = 0.707

    // Low-pass biquad coefficients
    const float norm = 1.0f + alpha;
    const float b0 = (1.0f - cosw) / (2.0f * norm);
    const float b1 = (1.0f - cosw) / norm;
    const float b2 = b0;
    const float a1 = -2.0f * cosw / norm;
    const float a2 = (1.0f - alpha) / norm;

    leftAntiAlias.b0 = b0; leftAntiAlias.b1 = b1; leftAntiAlias.b2 = b2;
    leftAntiAlias.a1 = a1; leftAntiAlias.a2 = a2;

    rightAntiAlias.b0 = b0; rightAntiAlias.b1 = b1; rightAntiAlias.b2 = b2;
    rightAntiAlias.a1 = a1; rightAntiAlias.a2 = a2;
}

static inline float wrapPi(float a)
{
    const float pi = juce::MathConstants<float>::pi;
    const float twoPi = juce::MathConstants<float>::twoPi;
    float x = std::fmod(a + pi, twoPi);
    if (x < 0) x += twoPi;
    return x - pi;
}

void SecretSauceEngine::updateCoefficients()
{
    // Scale 48k-referenced poles to current fs (r^ratio; theta*ratio, wrapped)
    const float fsRef = 48000.0f; //
    const float ratio = fsRef / (fs > 1.0f ? fs : fsRef);

    std::array<Pole, 6> aScaled{}, bScaled{};
    for (int i = 0; i < 6; ++i)
    {
        const Pole a = shapeA48k[(size_t)i];
        const Pole b = shapeB48k[(size_t)i];

        // Scale frequencies with safety clamp for high frequencies
        float scaledThetaA = wrapPi(a.theta * ratio);
        float scaledThetaB = wrapPi(b.theta * ratio);

        // Clamp very high frequencies that could cause aliasing
        const float maxTheta = juce::MathConstants<float>::pi * 0.85f; // ~85% of Nyquist
        scaledThetaA = juce::jlimit(-maxTheta, maxTheta, scaledThetaA);
        scaledThetaB = juce::jlimit(-maxTheta, maxTheta, scaledThetaB);

        aScaled[(size_t)i] = { std::pow(juce::jlimit(0.0f, 0.95f, a.r), ratio), scaledThetaA };
        bScaled[(size_t)i] = { std::pow(juce::jlimit(0.0f, 0.95f, b.r), ratio), scaledThetaB };
    }

    // Interpolate & set biquad for each section
    for (int i = 0; i < 6; ++i)
    {
        const Pole ip = interpPole(aScaled[(size_t)i], bScaled[(size_t)i], morph, intensity);
        poleToBiquad(ip, leftChain[(size_t)i],  intensity);
        poleToBiquad(ip, rightChain[(size_t)i], intensity);
    }
}

SecretSauceEngine::Pole SecretSauceEngine::interpPole (const Pole& a, const Pole& b, float t, float intensity01)
{
    // Clamp radii for stability, shortest-path theta blend
    float rA = juce::jlimit(0.1f, 0.99f, a.r);
    float rB = juce::jlimit(0.1f, 0.99f, b.r);

    float thetaA = a.theta, thetaB = b.theta;
    float d = thetaB - thetaA;
    while (d > juce::MathConstants<float>::pi)  d -= juce::MathConstants<float>::twoPi;
    while (d < -juce::MathConstants<float>::pi) d += juce::MathConstants<float>::twoPi;

    Pole out;
    out.r     = rA + t * (rB - rA);
    out.theta = thetaA + t * d;

    // Map intensity to radius scaling (more "Q")
    const float scale = 0.5f + juce::jlimit(0.f, 1.f, intensity01) * 0.49f;
    out.r *= scale;
    return out;
}

void SecretSauceEngine::poleToBiquad (const Pole& p, Biquad& s, float intensity01)
{
    // Convert conjugate pole pair to biquad with improved stability
    const float r = juce::jlimit(0.1f, 0.98f, p.r); // More conservative radius limit
    const float th = p.theta;

    const float re = r * std::cos(th);
    const float im = r * std::sin(th);

    // Denominator coefficients (poles)
    float a1 = -2.0f * re;
    float a2 = r * r;

    // More robust DC normalization - ensure we don't have DC gain issues
    float dcGain = 1.0f + a1 + a2;
    if (std::abs(dcGain) < 1.0e-3f) dcGain = 1.0e-3f; // Prevent near-zero division

    // Band-pass-like response: zero at DC and Nyquist
    float b0 = (1.0f - a2) / dcGain;
    float b1 = 0.0f;
    float b2 = -b0;

    // Apply Q scaling very conservatively to prevent resonant spikes
    const float qScale = 0.9f + juce::jlimit(0.f, 1.f, intensity01) * 0.05f; // Extremely conservative
    a1 *= qScale;
    a2 *= qScale;

    // Final stability clamps
    s.b0 = juce::jlimit(-2.0f, 2.0f, b0);
    s.b1 = b1;
    s.b2 = juce::jlimit(-2.0f, 2.0f, b2);
    s.a1 = juce::jlimit(-1.95f, 1.95f, a1);
    s.a2 = juce::jlimit(-0.95f, 0.95f, a2);
}