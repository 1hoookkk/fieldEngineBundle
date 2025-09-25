#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "AuthenticEMUZPlane.h"







namespace {



    constexpr float kRefFs = 48000.0f;



    constexpr float kStereoPhaseOffset = juce::MathConstants<float>::pi / 720.0f;



    constexpr float kStereoRadiusVariance = 0.002f;



    constexpr float kHighFreqDamping = 0.12f;



    constexpr float kHighFreqGainTaper = 0.35f;
    constexpr float kCoefficientSmoothFactor = 0.05f;  // Minimal smoothing - preserve EMU snap

    constexpr float kPoleRadiusMin = AuthenticEMUZPlane::minPoleRadius;
    constexpr float kPoleRadiusMax = AuthenticEMUZPlane::maxPoleRadius;
    constexpr float kPoleStabilityMargin = AuthenticEMUZPlane::stabilityMargin;



    // Bilinear transform: z@fs1 -> s -> z@fs2



    // Keeps filter shapes consistent across sample rates



    inline std::complex<float> remapZ(std::complex<float> zAtRef, float fsNew) {



        const std::complex<float> one(1.0f, 0.0f);



        const auto s   = 2.0f * kRefFs * (zAtRef - one) / (zAtRef + one);



        const auto zfs = (2.0f * fsNew + s) / (2.0f * fsNew - s);



        return zfs;

    }

    // sanitiseSection moved to static member function below

    inline float smoothTowards(float previous, float target, float intensity = 1.0f)
    {
        if (! std::isfinite(previous))
            return target;
        // Less smoothing at high intensity = more EMU character
        const float smoothFactor = kCoefficientSmoothFactor * (1.0f - intensity * 0.8f);
        return previous + smoothFactor * (target - previous);
    }

}

namespace {
    inline void validateReferenceTables()
    {
        static bool validated = false;
        if (validated)
            return;
        validated = true;

        juce::StringArray issues;

        for (int shape = 0; shape < AUTHENTIC_EMU_NUM_SHAPES; ++shape)
        {
            for (int idx = 0; idx < 12; idx += 2)
            {
                const float r = AUTHENTIC_EMU_SHAPES[shape][idx];
                const float theta = AUTHENTIC_EMU_SHAPES[shape][idx + 1];
                if (! std::isfinite(r) || r < kPoleRadiusMin || r > 1.2f)
                    issues.add("Shape " + juce::String(shape) + ": radius out of range @ index " + juce::String(idx));
                if (! std::isfinite(theta))
                    issues.add("Shape " + juce::String(shape) + ": theta not finite @ index " + juce::String(idx + 1));
            }
        }

        juce::StringArray pairKeys;
        for (int pair = 0; pair < AUTHENTIC_EMU_NUM_PAIRS; ++pair)
        {
            const int a = MORPH_PAIRS[pair][0];
            const int b = MORPH_PAIRS[pair][1];
            if (a == b)
                issues.add("Morph pair " + juce::String(pair) + " references identical shapes");
            if (a < 0 || a >= AUTHENTIC_EMU_NUM_SHAPES || b < 0 || b >= AUTHENTIC_EMU_NUM_SHAPES)
                issues.add("Morph pair " + juce::String(pair) + " references out-of-range shape index");
            const juce::String key = juce::String(a) + "->" + juce::String(b);
            if (pairKeys.contains(key))
                issues.add("Duplicate morph pair entry: " + key);
            else
                pairKeys.add(key);
        }

        if (! issues.isEmpty())
            juce::Logger::writeToLog("AuthenticEMUZPlane data validation issues:\n" + issues.joinIntoString("\n"));
    }
} // namespace







AuthenticEMUZPlane::AuthenticEMUZPlane() {



    // Neutral defaults for production reliability



    setMorphPair(0);



    setMorphPosition(0.5f);



    setIntensity(0.0f);        // Transparent by default



    setDrive(0.0f);            // Unity gain by default



    setSectionSaturation(0.0f); // Clean by default



    setAutoMakeup(false);



    setLFORate(0.0f);          // No modulation by default



    setLFODepth(0.0f);



}







void AuthenticEMUZPlane::prepareToPlay (double sampleRate) {



    fs = (float) sampleRate;
    validateReferenceTables();
    juce::FloatVectorOperations::disableDenormalisedNumberSupport();







    // 20ms smoothing for zipper-free parameter changes



    morphSm.reset(fs, 0.020f);



    intenSm.reset(fs, 0.020f);



    morphSm.setCurrentAndTargetValue(currentMorph);



    intenSm.setCurrentAndTargetValue(currentIntensity);







    reset();



    updateCoefficientsBlock(); // Prime the coefficients



}







void AuthenticEMUZPlane::reset() {



    for (auto& s : sectionsL) s.reset();



    for (auto& s : sectionsR) s.reset();



    lfoPhase = 0.0f;



}







// --- Parameter setters ---



void AuthenticEMUZPlane::setMorphPair(MorphPair p)          { currentPair = p; }



void AuthenticEMUZPlane::setMorphPosition(float v)          { currentMorph     = juce::jlimit(0.0f, 1.0f, v); }



void AuthenticEMUZPlane::setIntensity(float v)              { currentIntensity = juce::jlimit(0.0f, 1.0f, v); }



void AuthenticEMUZPlane::setDrive(float db)                 { driveLin = juce::Decibels::decibelsToGain(db); }



void AuthenticEMUZPlane::setSectionSaturation(float v)      { sectionSaturation= juce::jlimit(0.0f, 1.0f, v); }



void AuthenticEMUZPlane::setAutoMakeup(bool e)              { autoMakeup = e; }



void AuthenticEMUZPlane::setLFORate(float hz)               { lfoRate   = juce::jlimit(0.02f, 8.0f, hz); }



void AuthenticEMUZPlane::setLFODepth(float d)               { lfoDepth  = juce::jlimit(0.0f, 1.0f, d); }







void AuthenticEMUZPlane::process(juce::AudioBuffer<float>& buffer) {
    juce::ScopedNoDenormals noDenormals;



    const int nCh = buffer.getNumChannels();



    const int nSm = buffer.getNumSamples();







    // Update targets and coefficients before assessing transparency



    updateCoefficientsBlock();







    const float intensityNow = intenSm.getCurrentValue();



    const bool driveNeutral  = std::abs (driveLin - 1.0f) < 1.0e-6f;



    const bool satNeutral    = sectionSaturation <= 1.0e-6f;



    const bool lfoNeutral    = lfoDepth <= 1.0e-6f;



    if (intensityNow <= 1.0e-4f && driveNeutral && satNeutral && lfoNeutral)



        return;







    if (! driveNeutral)



        for (int ch = 0; ch < nCh; ++ch)



        {



            auto* x = buffer.getWritePointer (ch);



            for (int i = 0; i < nSm; ++i)



                x[i] *= driveLin;



        }







    for (int ch = 0; ch < nCh; ++ch)



    {



        auto* x = buffer.getWritePointer (ch);



        auto* S = (ch == 0 ? sectionsL : sectionsR);



        for (int i = 0; i < nSm; ++i)



        {



            float y = x[i];



            for (int s = 0; s < 6; ++s)



                y = S[s].processSample (y, sectionSaturation);



            x[i] = y;



        }



    }







    // Auto-makeup gain REMOVED per DSP research findings
    // - Masks authentic EMU character ("glass-clear, thin, precise, resonant")
    // - EMU Audity-2000 never had automatic gain compensation
    // - Users should control output level manually for authentic experience
    if (false && autoMakeup) // DISABLED



    {
        const float makeup = 1.0f / (1.0f + 0.5f * intensityNow);



        for (int ch = 0; ch < nCh; ++ch)



        {



            auto* x = buffer.getWritePointer (ch);



            for (int i = 0; i < nSm; ++i)



                x[i] *= makeup;



        }



    }



}



void AuthenticEMUZPlane::updateCoefficientsBlock(int blockSamples) {



    // Control-rate LFO advance (approximate one block)



    if (lfoRate > 0.0f) {



        const float inc = juce::MathConstants<float>::twoPi * (lfoRate / fs);



        lfoPhase += inc * 64.0f; // assume ~64-sample blocks



        if (lfoPhase >= juce::MathConstants<float>::twoPi)



            lfoPhase -= juce::MathConstants<float>::twoPi;



    }







    // LFO modulates morph position



    const float lfoUnipolar = 0.5f * (1.0f + std::sin(lfoPhase)) * lfoDepth;







    // Update smoother targets



    morphSm.setTargetValue(juce::jlimit(0.0f, 1.0f, currentMorph + lfoUnipolar));



    intenSm.setTargetValue(currentIntensity);







    const int samplesToAdvance = blockSamples > 0 ? blockSamples : 64;
    morphSm.skip(samplesToAdvance);
    intenSm.skip(samplesToAdvance);

    const float morph = morphSm.getCurrentValue();
    const float morphEased = morph * morph * (3.0f - 2.0f * morph);

    const float intensity = intenSm.getCurrentValue();







    // Pull shape data from tables

    const int pairIndex = juce::jlimit(0, AUTHENTIC_EMU_NUM_PAIRS - 1, static_cast<int>(currentPair));
    const int* pair = MORPH_PAIRS[pairIndex];

    if (useDynamicPack && dynNumPairs > 0)
    {
        const int dynPairIndex = juce::jlimit(0, dynNumPairs - 1, static_cast<int>(currentPair));
        pair = dynPairs[(size_t) dynPairIndex].data();
    }

    const float* shapeA = AUTHENTIC_EMU_SHAPES[juce::jlimit(0, AUTHENTIC_EMU_NUM_SHAPES - 1, pair[0])];
    const float* shapeB = AUTHENTIC_EMU_SHAPES[juce::jlimit(0, AUTHENTIC_EMU_NUM_SHAPES - 1, pair[1])];

    if (useDynamicPack && dynNumShapes > 0)
    {
        const int dynShapeA = juce::jlimit(0, dynNumShapes - 1, pair[0]);
        const int dynShapeB = juce::jlimit(0, dynNumShapes - 1, pair[1]);
        shapeA = dynShapes[(size_t) dynShapeA].data();
        shapeB = dynShapes[(size_t) dynShapeB].data();
    }



    // Interpolate poles at 48k reference (shortest-path theta, linear radius)



    for (int i = 0; i < 6; ++i) {



        const int ri = i*2, ti = i*2+1;



        float rA = juce::jlimit(kPoleRadiusMin, kPoleRadiusMax, shapeA[ri]);



        float rB = juce::jlimit(kPoleRadiusMin, kPoleRadiusMax, shapeB[ri]);



        float tA = shapeA[ti];



        float tB = shapeB[ti];







        // Shortest path for theta interpolation (handles wrap-around)



        float d = tB - tA;



        while (d >  juce::MathConstants<float>::pi)  d -= juce::MathConstants<float>::twoPi;



        while (d < -juce::MathConstants<float>::pi)  d += juce::MathConstants<float>::twoPi;







        // Log-radius interpolation for stability per DSP research
        float r = std::exp((1.0f - morphEased) * std::log(std::max(1e-6f, rA)) +
                          morphEased * std::log(std::max(1e-6f, rB)));



        float the = tA + morphEased * d;







        // Intensity interpolates between a softer neutral and the authentic radius

        const float neutralR = juce::jlimit(kPoleRadiusMin, kPoleRadiusMax, r * 0.85f);

        const float intensityBlend = juce::jlimit(0.0f, 1.0f, intensity);
        r = juce::jlimit(kPoleRadiusMin, kPoleRadiusMax, juce::jmap(intensityBlend, neutralR, r));



        polesRef48[i] = { r, the };



    }







    // Remap z(48k) -> z(fs) and compute biquad sections



    for (int i = 0; i < 6; ++i) {



        const auto zRef = std::polar(polesRef48[i].r, polesRef48[i].theta);
        const auto zfs   = (fs == kRefFs) ? zRef : remapZ(zRef, fs);

        float rfs = std::abs(zfs);
        if (!std::isfinite(rfs))
            rfs = kPoleRadiusMin;

        const float thf = std::arg(zfs);
        const float normFreq = juce::jlimit(0.0f, 1.0f, std::abs(thf) / juce::MathConstants<float>::pi);
        const float damping = juce::jlimit(0.0f, 1.0f, 1.0f - kHighFreqDamping * normFreq * normFreq);

        rfs = juce::jlimit(kPoleRadiusMin, kPoleRadiusMax, rfs * damping);
        polesFs[i] = { rfs, thf };

        const float hfGain = juce::jlimit(0.5f, 1.0f, 1.0f - kHighFreqGainTaper * normFreq * normFreq);

        float a1L, a2L, b0L, b1L, b2L;
        zpairToBiquad(polesFs[i], a1L, a2L, b0L, b1L, b2L);
        b0L *= hfGain;
        b2L = -b0L;

        const float phaseOffset  = (i % 2 == 0 ? kStereoPhaseOffset : -kStereoPhaseOffset);
        const float radiusOffset = 1.0f + ((i % 2 == 0) ? kStereoRadiusVariance : -kStereoRadiusVariance);
        PolePair rightPair {
            juce::jlimit(kPoleRadiusMin, kPoleRadiusMax, rfs * radiusOffset),
            thf + phaseOffset
        };

        float a1R, a2R, b0R, b1R, b2R;
        zpairToBiquad(rightPair, a1R, a2R, b0R, b1R, b2R);
        b0R *= hfGain;
        b2R = -b0R;

        const float prevA1L = sectionsL[i].a1;
        const float prevA2L = sectionsL[i].a2;
        const float prevB0L = sectionsL[i].b0;
        const float prevB1L = sectionsL[i].b1;
        const float prevB2L = sectionsL[i].b2;
        const float prevA1R = sectionsR[i].a1;
        const float prevA2R = sectionsR[i].a2;
        const float prevB0R = sectionsR[i].b0;
        const float prevB1R = sectionsR[i].b1;
        const float prevB2R = sectionsR[i].b2;

        sectionsL[i].a1 = smoothTowards(prevA1L, a1L, intensity);
        sectionsL[i].a2 = smoothTowards(prevA2L, a2L, intensity);
        sectionsL[i].b0 = smoothTowards(prevB0L, b0L, intensity);
        sectionsL[i].b1 = smoothTowards(prevB1L, b1L, intensity);
        sectionsL[i].b2 = smoothTowards(prevB2L, b2L, intensity);

        sectionsR[i].a1 = smoothTowards(prevA1R, a1R, intensity);
        sectionsR[i].a2 = smoothTowards(prevA2R, a2R, intensity);
        sectionsR[i].b0 = smoothTowards(prevB0R, b0R, intensity);
        sectionsR[i].b1 = smoothTowards(prevB1R, b1R, intensity);
        sectionsR[i].b2 = smoothTowards(prevB2R, b2R, intensity);

        AuthenticEMUZPlane::sanitiseSection(sectionsL[i]);
        AuthenticEMUZPlane::sanitiseSection(sectionsR[i]);
}




    // Passivity clamp: coarse grid sweep and normalise numerator gain if needed
    auto sectionMag = [] (float b0,float b1,float b2,float a1,float a2, float zRe, float zIm) -> float {
        std::complex<float> z1(zRe, -zIm);
        std::complex<float> z2 = z1 * z1;
        std::complex<float> num = std::complex<float>(b0,0) + std::complex<float>(b1,0)*z1 + std::complex<float>(b2,0)*z2;
        std::complex<float> den = std::complex<float>(1,0) + std::complex<float>(a1,0)*z1 + std::complex<float>(a2,0)*z2;
        return std::abs(num/den);
    };

    float maxMag = 0.0f;
    for (int k=1; k<=12; ++k) {
        const float w = juce::MathConstants<float>::pi * (float)k / 13.0f;
        const float zRe = std::cos(w), zIm = std::sin(w);
        float magL = 1.0f;
        for (int i=0;i<6;++i) {
            magL *= sectionMag(sectionsL[i].b0, sectionsL[i].b1, sectionsL[i].b2,
                               sectionsL[i].a1, sectionsL[i].a2, zRe, zIm);
        }
        if (magL > maxMag) maxMag = magL;
    }
    if (maxMag > 1.05f) {
        const float sN = 1.05f / maxMag;
        for (int i=0;i<6;++i) { sectionsL[i].b0 *= sN; sectionsL[i].b2 *= sN; sectionsR[i].b0 *= sN; sectionsR[i].b2 *= sN; }
    }
}







void AuthenticEMUZPlane::zpairToBiquad(const PolePair& p, float& a1, float& a2, float& b0, float& b1, float& b2) {
    // Denominator from complex pole pair
    a1 = -2.0f * p.r * std::cos(p.theta);
    a2 =  p.r * p.r;

    // Bandpass numerator (zeros at DC & Nyquist) with softened gain profile
    const float softGain = 0.5f * ((1.0f - p.r) + (1.0f - p.r * p.r));
    b0 = softGain;
    b1 = 0.0f;
    b2 = -softGain;

    // Numerical stability clamps
    a1 = juce::jlimit(-1.999f, 1.999f, a1);
    a2 = juce::jlimit(-0.999f, 0.999f, a2);
}

void AuthenticEMUZPlane::sanitiseSection(BiquadSection& section) {
    // Ensure all coefficients are finite and within reasonable bounds
    if (!std::isfinite(section.a1)) section.a1 = 0.0f;
    if (!std::isfinite(section.a2)) section.a2 = 0.0f;
    if (!std::isfinite(section.b0)) section.b0 = 1.0f;
    if (!std::isfinite(section.b1)) section.b1 = 0.0f;
    if (!std::isfinite(section.b2)) section.b2 = 0.0f;

    // Clamp to stability bounds
    section.a1 = juce::jlimit(-1.999f, 1.999f, section.a1);
    section.a2 = juce::jlimit(-0.999f, 0.999f, section.a2);

    // Sanitize state variables
    if (!std::isfinite(section.z1)) section.z1 = 0.0f;
    if (!std::isfinite(section.z2)) section.z2 = 0.0f;
}


void AuthenticEMUZPlane::setLFOPhase(float radians) {
    lfoPhase = radians;
}


void AuthenticEMUZPlane::setModelPack(const float* shapes12, int numShapes, const int* pairs2, int numPairs)
{
    const int maxShapes = (int) dynShapes.size();
    dynNumShapes = juce::jlimit(0, maxShapes, numShapes);
    for (int i=0;i<dynNumShapes;++i) {
        for (int k=0;k<12;++k) dynShapes[(size_t)i][(size_t)k] = shapes12[i*12 + k];
    }
    const int maxPairs = (int) dynPairs.size();
    dynNumPairs = juce::jlimit(0, maxPairs, numPairs);
    for (int i=0;i<dynNumPairs;++i) {
        dynPairs[(size_t)i][0] = pairs2[i*2 + 0];
        dynPairs[(size_t)i][1] = pairs2[i*2 + 1];
    }
    useDynamicPack = (dynNumShapes > 0 && dynNumPairs > 0);
}
