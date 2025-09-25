#include "ZPlaneStyle.h"

static juce::var parseEmbeddedJSON()
{
    auto* data = BinaryData::pitchEngine_zLUT_ref48k_json;
    auto  size = BinaryData::pitchEngine_zLUT_ref48k_jsonSize;
    juce::MemoryInputStream mis (data, size, false);
    juce::String json = mis.readEntireStreamAsString();
    juce::var v = juce::JSON::parse (json);
    jassert (! v.isVoid());
    return v;
}

void ZPlaneStyle::buildFromEmbeddedLUT()
{
    steps.clear();
    auto v = parseEmbeddedJSON();
    auto* obj = v.getDynamicObject();
    jassert (obj != nullptr);

    auto pairs = obj->getProperty ("pairs");
    jassert (pairs.isArray());

    // pick first pair (vowel_morph)
    auto arr = *pairs.getArray();
    jassert (arr.size() > 0);
    auto first = arr[0];
    auto* pobj = first.getDynamicObject();
    auto s = pobj->getProperty ("steps");
    jassert (s.isArray());

    for (const auto& stepVar : *s.getArray())
    {
        Step st;
        auto* sobj = stepVar.getDynamicObject();
        st.t = (float) sobj->getProperty ("t");
        auto poles = sobj->getProperty ("poles");
        jassert (poles.isArray());
        for (const auto& pv : *poles.getArray())
        {
            auto* pObj = pv.getDynamicObject();
            Pole p;
            p.r = (double) pObj->getProperty ("r");
            p.thetaRef = (double) pObj->getProperty ("theta_ref");
            st.poles.add (p);
        }
        steps.add (st);
    }

    numSections = juce::jmin (6, steps[0].poles.size());
}

static inline void biquadFromPole (double r, double theta, double (&b)[3], double (&a)[3])
{
    // All‑pole section: H(z) = 1 / (1 - 2 r cos(theta) z^-1 + r^2 z^-2)
    b[0] = 1.0; b[1] = 0.0; b[2] = 0.0;
    a[0] = 1.0; a[1] = -2.0 * r * std::cos (theta); a[2] = r * r;
}

void ZPlaneStyle::setCoefficientsFor (float tNorm)
{
    if (steps.isEmpty()) return;

    // Smooth map with safety cap (keep top 15% for future advanced mode)
    tNorm = juce::jlimit (0.0f, 0.85f, tNorm);
    auto ease = [](float x){ return x*x*(3.0f - 2.0f*x); }; // smoothstep
    float t = ease (tNorm);

    // Locate bracketing LUT steps
    const int N = steps.size();
    float pos = t * (float) (N - 1);
    int i0 = (int) std::floor (pos);
    int i1 = juce::jmin (N - 1, i0 + 1);
    float f  = pos - (float) i0;

    // Build SOS from interpolated poles, with gentle Q trim
    const auto& s0 = steps[i0];
    const auto& s1 = steps[i1];

    for (int k = 0; k < numSections; ++k)
    {
        const auto& p0 = s0.poles[k];
        const auto& p1 = s1.poles[k];

        double r0 = p0.r, r1 = p1.r;
        double r = std::exp ((1.0 - f) * std::log (std::max (1e-6, r0)) + f * std::log (std::max (1e-6, r1)));
        double thRef = p0.thetaRef + f * juce::jlimit (-juce::MathConstants<double>::pi, juce::MathConstants<double>::pi,
                         std::remainder (p1.thetaRef - p0.thetaRef, juce::MathConstants<double>::twoPi));

        // CRITICAL FIX: Corrected θ sample-rate scaling (was inverted)
        double theta = thRef * (48000.0 / fsHost);

        // Q trim vs t (reduce resonance slightly as style increases)
        double qTrim = juce::jmap ((double)t, 0.0, 0.85, 1.0, 0.9);
        r = 1.0 - (1.0 - r) * qTrim;

        // Absolute pole safety cap
        r = juce::jmin (r, 0.9995);

        double b[3], a[3];
        biquadFromPole (r, theta, b, a);

        // Secret mode: coefficient quantization for "fixed-grid math feel"
        if (secret)
        {
            auto qfix = [](double x){ const double s = std::ldexp(1.0, 20); return std::round(x*s)/s; };
            a[1] = qfix(a[1]);
            a[2] = qfix(a[2]);
        }

        // CRITICAL FIX: Use compatible coefficients construction without allocating per block
        auto coeffStack = juce::dsp::IIR::Coefficients<float> ( (float)b[0], (float)b[1], (float)b[2], (float)a[0], (float)a[1], (float)a[2] );
        *sosL[k].coefficients = coeffStack;
        *sosR[k].coefficients = coeffStack;
    }
}

void ZPlaneStyle::process (juce::AudioBuffer<float>& buf, float style)
{
    const int C = juce::jmin (2, buf.getNumChannels());
    const int N = buf.getNumSamples();

    // CRITICAL FIX: Member flag instead of static init
    if (!hasCoeffs)
    {
        for (int k = 0; k < numSections; ++k)
        {
            sosL[k].reset();
            sosR[k].reset();
        }
        setCoefficientsFor (style);
        hasCoeffs = true;
    }

    // Secret mode: morph slew (~6 ms)
    float target = style;
    if (secret)
    {
        const float alpha = std::exp(-1.0f / (float(fsHost) * 0.006f));
        morphState = alpha * morphState + (1.0f - alpha) * target;
        setCoefficientsFor (morphState);
    }
    else
    {
        morphState = target;
        setCoefficientsFor (style);
    }

    // Secret mode: tiny dither to avoid sterile silence/denormals
    if (secret)
    {
        for (int ch = 0; ch < C; ++ch)
        {
            auto* data = buf.getWritePointer (ch);
            for (int n = 0; n < N; ++n)
            {
                data[n] += (rng.nextFloat() * 2.0f - 1.0f) * 0.000004f; // ≈ −88 dBFS
            }
        }
    }

    // CRITICAL FIX: Proper per-channel processing
    juce::dsp::AudioBlock<float> block (buf);

    if (C >= 1)
    {
        auto left = block.getSingleChannelBlock (0);
        juce::dsp::ProcessContextReplacing<float> ctxL (left);

        // cascade biquads on left channel
        for (int k = 0; k < numSections; ++k)
            sosL[k].process (ctxL);
    }

    if (C >= 2)
    {
        auto right = block.getSingleChannelBlock (1);
        juce::dsp::ProcessContextReplacing<float> ctxR (right);

        // cascade biquads on right channel
        for (int k = 0; k < numSections; ++k)
            sosR[k].process (ctxR);
    }
}