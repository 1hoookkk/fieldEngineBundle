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

// Schur triangle stability projection (per DSP research)
static inline void projectToStableRegion(double& a1, double& a2)
{
    // Stability triangle: |a2| < 1, |a1| < 1 + a2
    constexpr double eps = 2e-6; // Small margin for numerical safety

    // Project a2 first
    a2 = std::min(a2, 1.0 - eps);
    a2 = std::max(a2, -1.0 + eps);

    // Then project a1 based on a2
    const double a1max = (1.0 + a2) - eps;
    const double a1min = -a1max;
    a1 = std::min(a1max, std::max(a1min, a1));
}

static inline void biquadFromPole (double r, double theta, double (&b)[3], double (&a)[3])
{
    // All‑pole section: H(z) = 1 / (1 - 2 r cos(theta) z^-1 + r^2 z^-2)
    b[0] = 1.0; b[1] = 0.0; b[2] = 0.0;
    a[0] = 1.0; a[1] = -2.0 * r * std::cos (theta); a[2] = r * r;

    // Apply stability projection (character-preserving safety)
    projectToStableRegion(a[1], a[2]);
}

void ZPlaneStyle::setCoefficientsFor (float tNorm, bool updateBackground)
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

    // Build SOS from interpolated poles, ordered by Q (low to high for cascade stability)
    const auto& s0 = steps[i0];
    const auto& s1 = steps[i1];

    // Collect interpolated poles for sorting
    struct InterpolatedPole {
        double r, theta;
        int originalIndex;
    };
    std::vector<InterpolatedPole> poles;

    for (int k = 0; k < numSections; ++k)
    {
        const auto& p0 = s0.poles[k];
        const auto& p1 = s1.poles[k];

        double r0 = p0.r, r1 = p1.r;
        double r = std::exp ((1.0 - f) * std::log (std::max (1e-6, r0)) + f * std::log (std::max (1e-6, r1)));
        double thRef = p0.thetaRef + f * juce::jlimit (-juce::MathConstants<double>::pi, juce::MathConstants<double>::pi,
                         std::remainder (p1.thetaRef - p0.thetaRef, juce::MathConstants<double>::twoPi));

        // CRITICAL FIX: Corrected θ sample-rate scaling (was inverted) with overflow protection
        double theta = thRef * (48000.0 / fsHost);

        // Prevent theta overflow at extreme sample rates per DSP research
        theta = std::fmod(theta, juce::MathConstants<double>::twoPi);
        if (theta < 0.0) theta += juce::MathConstants<double>::twoPi;

        // Implement matched-Z sample rate conversion for r (preserve bandwidth in Hz)
        // Base radius was computed for 48kHz, adjust for current sample rate
        const double fsRef = 48000.0;
        r = std::pow(r, fsRef / fsHost);

        // Pole radius limits per DSP research (0.996-0.997 at 44.1kHz)
        // Apply soft-limit to prevent pathological Q explosions
        const double rMinBase44k = 0.996; // Min at 44.1kHz
        const double rMaxBase44k = 0.997; // Max at 44.1kHz
        const double rMin = std::pow(rMinBase44k, 44100.0 / fsHost); // Scale for current Fs
        const double rMax = std::pow(rMaxBase44k, 44100.0 / fsHost); // Scale for current Fs

        // Enforce safe radius bounds with soft limiting
        if (r > rMax) {
            const double K = 4e-4; // Soft-limit parameter
            const double delta = 1.0 - r;
            r = 1.0 - (delta / (1.0 + delta / K));
            r = std::min(r, rMax);
        }
        r = std::max(r, rMin); // Ensure minimum stability

        poles.push_back({r, theta, k});
    }

    // Sort poles by Q factor (r closer to 1 = higher Q) for cascade stability
    std::sort(poles.begin(), poles.end(), [](const InterpolatedPole& a, const InterpolatedPole& b) {
        return a.r < b.r; // Lower Q first
    });

    // Build coefficients in sorted order with per-section normalization
    double cascadeGain = 1.0;
    for (int k = 0; k < numSections; ++k)
    {
        double r = poles[k].r;
        double theta = poles[k].theta;

        double b[3], a[3];
        biquadFromPole (r, theta, b, a);

        // Per-section unity normalization (preserve EMU character while preventing overload)
        // Compute peak gain at resonant frequency (simple approximation for bandpass)
        double sectionPeakGain = 1.0 / (1.0 - r); // Approximation for high-Q resonator
        double sectionScale = 1.0 / std::sqrt(sectionPeakGain); // Scale for ~18dB headroom

        // Apply section scaling to numerator
        b[0] *= sectionScale;
        b[1] *= sectionScale;
        b[2] *= sectionScale;

        cascadeGain *= sectionScale;

        // Secret mode: coefficient quantization for "fixed-grid math feel"
        if (secret)
        {
            auto qfix = [](double x){ const double s = std::ldexp(1.0, 20); return std::round(x*s)/s; };
            a[1] = qfix(a[1]);
            a[2] = qfix(a[2]);
        }

        // CRITICAL FIX: Use compatible coefficients construction
        auto coeff = std::make_shared<juce::dsp::IIR::Coefficients<float>> (b[0], b[1], b[2], a[0], a[1], a[2]);

        // Apply to main or background filters based on updateBackground flag
        if (updateBackground)
        {
            *sosL_bg[k].coefficients = *coeff;
            *sosR_bg[k].coefficients = *coeff;
        }
        else
        {
            *sosL[k].coefficients = *coeff;
            *sosR[k].coefficients = *coeff;
        }
    }

    // Cascade-wide normalization (maintain reasonable output level)
    const double globalScale = 1.05; // Slight passivity margin per research
    const double finalScale = globalScale / std::max(1e-6, std::abs(cascadeGain));

    // Apply final scaling to first section to avoid recomputing all coefficients
    if (numSections > 0)
    {
        if (updateBackground)
        {
            auto firstCoeff = sosL_bg[0].coefficients.get();
            if (firstCoeff != nullptr)
            {
                auto scaledCoeff = std::make_shared<juce::dsp::IIR::Coefficients<float>>(
                    firstCoeff->coefficients[0] * finalScale,
                    firstCoeff->coefficients[1] * finalScale,
                    firstCoeff->coefficients[2] * finalScale,
                    firstCoeff->coefficients[3],
                    firstCoeff->coefficients[4],
                    firstCoeff->coefficients[5]);

                *sosL_bg[0].coefficients = *scaledCoeff;
                *sosR_bg[0].coefficients = *scaledCoeff;
            }
        }
        else
        {
            auto firstCoeff = sosL[0].coefficients.get();
            if (firstCoeff != nullptr)
            {
                auto scaledCoeff = std::make_shared<juce::dsp::IIR::Coefficients<float>>(
                    firstCoeff->coefficients[0] * finalScale,
                    firstCoeff->coefficients[1] * finalScale,
                    firstCoeff->coefficients[2] * finalScale,
                    firstCoeff->coefficients[3],
                    firstCoeff->coefficients[4],
                    firstCoeff->coefficients[5]);

                *sosL[0].coefficients = *scaledCoeff;
                *sosR[0].coefficients = *scaledCoeff;
            }
        }
    }
}

void ZPlaneStyle::process (juce::AudioBuffer<float>& buf, float style)
{
    // Critical: Enable denormal protection (FTZ/DAZ) per DSP research
    juce::ScopedNoDenormals noDenormals;

    const int C = juce::jmin (2, buf.getNumChannels());
    const int N = buf.getNumSamples();

    // CRITICAL FIX: Member flag instead of static init
    if (!hasCoeffs)
    {
        for (int k = 0; k < numSections; ++k)
        {
            sosL[k].reset();
            sosR[k].reset();
            sosL_bg[k].reset(); // Also reset background filters
            sosR_bg[k].reset();
        }
        setCoefficientsFor (style);
        hasCoeffs = true;
    }

    // Per-sample parameter smoothing for snappy morphs (0.3ms time constant)
    float target = style;
    const float tau = 0.0003f; // 0.3ms time constant per DSP research
    const float alpha = std::exp(-1.0f / (float(fsHost) * tau));

    // Always use smoothing for stability (even in non-secret mode)
    morphState = alpha * morphState + (1.0f - alpha) * target;

    // Detect large parameter jumps for crossfading (per DSP research)
    const float morphChange = std::abs(morphState - prevMorphState);
    if (morphChange > kLargeJumpThreshold && !needsCrossfade)
    {
        needsCrossfade = true;
        crossfadeSamples = kCrossfadeLength;
        crossfadeAmount = 0.0f;

        // Update background filters with new coefficients
        setCoefficientsFor(morphState, true); // true = update background filters
    }
    else if (!needsCrossfade)
    {
        // Normal coefficient updates for small changes
        setCoefficientsFor(morphState, false); // false = update main filters
    }

    prevMorphState = morphState;

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

    // Dual-path processing with crossfading for smooth parameter changes
    juce::dsp::AudioBlock<float> block (buf);

    // Handle crossfading for smooth parameter transitions
    if (needsCrossfade && crossfadeSamples > 0)
    {
        // Create temporary buffers for background filter output
        juce::AudioBuffer<float> bgBuffer;
        bgBuffer.setSize(C, N, false, false, true);
        bgBuffer.clear();

        // Process main filters (current output)
        for (int ch = 0; ch < C; ++ch)
        {
            auto channelBlock = block.getSingleChannelBlock(ch);
            juce::dsp::ProcessContextReplacing<float> ctx(channelBlock);

            auto* mainFilters = (ch == 0) ? sosL : sosR;
            for (int k = 0; k < numSections; ++k)
                mainFilters[k].process(ctx);
        }

        // Process background filters (new coefficients)
        for (int ch = 0; ch < C; ++ch)
        {
            // Copy input to background buffer
            bgBuffer.copyFrom(ch, 0, buf, ch, 0, N);

            juce::dsp::AudioBlock<float> bgBlock(bgBuffer);
            auto channelBlock = bgBlock.getSingleChannelBlock(ch);
            juce::dsp::ProcessContextReplacing<float> ctx(channelBlock);

            auto* bgFilters = (ch == 0) ? sosL_bg : sosR_bg;
            for (int k = 0; k < numSections; ++k)
                bgFilters[k].process(ctx);
        }

        // Crossfade between main and background outputs
        const int samplesToProcess = juce::jmin(N, crossfadeSamples);
        for (int ch = 0; ch < C; ++ch)
        {
            auto* mainData = buf.getWritePointer(ch);
            auto* bgData = bgBuffer.getReadPointer(ch);

            for (int n = 0; n < samplesToProcess; ++n)
            {
                const float progress = (float)(kCrossfadeLength - crossfadeSamples + n) / (float)kCrossfadeLength;
                const float fadeCurve = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * progress));

                mainData[n] = (1.0f - fadeCurve) * mainData[n] + fadeCurve * bgData[n];
            }
        }

        crossfadeSamples -= samplesToProcess;

        // Complete crossfade: swap filters and reset state
        if (crossfadeSamples <= 0)
        {
            // Swap main and background filters
            for (int k = 0; k < numSections; ++k)
            {
                std::swap(sosL[k], sosL_bg[k]);
                std::swap(sosR[k], sosR_bg[k]);
            }

            needsCrossfade = false;
            crossfadeAmount = 0.0f;
        }
    }
    else
    {
        // Normal processing (no crossfade)
        for (int ch = 0; ch < C; ++ch)
        {
            auto channelBlock = block.getSingleChannelBlock(ch);
            juce::dsp::ProcessContextReplacing<float> ctx(channelBlock);

            // Apply cascade with energy monitoring
            auto* filters = (ch == 0) ? sosL : sosR;
            auto* energyArray = (ch == 0) ? outputEnergyL : outputEnergyR;

            for (int k = 0; k < numSections; ++k)
            {
                filters[k].process(ctx);

                // Lightweight energy monitoring (last-resort safety per DSP research)
                auto* data = channelBlock.getChannelPointer(0);
                float maxLevel = 0.0f;
                for (size_t n = 0; n < channelBlock.getNumSamples(); ++n)
                    maxLevel = std::max(maxLevel, std::abs(data[n]));

                // Exponential peak tracking
                energyArray[k] = kEnergyDecay * energyArray[k] + (1.0f - kEnergyDecay) * (maxLevel * maxLevel);

                // Apply gentle limiting if energy exceeds threshold (rare)
                if (energyArray[k] > kMaxEnergy)
                {
                    float scale = std::sqrt(kMaxEnergy / energyArray[k]);
                    for (size_t n = 0; n < channelBlock.getNumSamples(); ++n)
                        data[n] *= scale;
                    energyArray[k] = kMaxEnergy; // Reset tracker
                }
            }
        }
    }
}