#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_core/juce_core.h>
#include <cstdio>
#include <cmath>

#include "qa/tests/PlaneTestProbe.h"

namespace
{
constexpr float kMaxCoefficientDelta = 0.45f;
}

static float maxSectionDelta(const std::array<AuthenticEMUZPlane::BiquadSection, 6>& a,
                             const std::array<AuthenticEMUZPlane::BiquadSection, 6>& b)
{
    float maxDelta = 0.0f;
    for (size_t i = 0; i < a.size(); ++i)
    {
        const auto& sa = a[i];
        const auto& sb = b[i];
        maxDelta = juce::jmax(maxDelta, std::abs(sa.a1 - sb.a1));
        maxDelta = juce::jmax(maxDelta, std::abs(sa.a2 - sb.a2));
        maxDelta = juce::jmax(maxDelta, std::abs(sa.b0 - sb.b0));
        maxDelta = juce::jmax(maxDelta, std::abs(sa.b1 - sb.b1));
        maxDelta = juce::jmax(maxDelta, std::abs(sa.b2 - sb.b2));
    }
    return maxDelta;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    AuthenticEMUZPlane plane;
    plane.prepareToPlay(48000.0);
    plane.setIntensity(1.0f);

    juce::AudioBuffer<float> buffer(2, 128);
    buffer.clear();

    bool ok = true;
    bool firstCapture = true;
    std::array<AuthenticEMUZPlane::BiquadSection, 6> prevL{};
    std::array<AuthenticEMUZPlane::BiquadSection, 6> prevR{};

    for (int pair = 0; pair < AUTHENTIC_EMU_NUM_PAIRS && ok; ++pair)
    {
        plane.setMorphPair(pair);

        for (int step = 0; step <= 20 && ok; ++step)
        {
            const float morph = step / 20.0f;
            plane.setMorphPosition(morph);
            buffer.clear();
            plane.process(buffer);

            auto currentL = AuthenticEMUZPlaneTestProbe::sectionsL(plane);
            auto currentR = AuthenticEMUZPlaneTestProbe::sectionsR(plane);

            if (! firstCapture)
            {
                const float deltaL = maxSectionDelta(prevL, currentL);
                const float deltaR = maxSectionDelta(prevR, currentR);
                if (deltaL > kMaxCoefficientDelta || deltaR > kMaxCoefficientDelta)
                {
                    ok = false;
                    break;
                }
            }
            else
            {
                firstCapture = false;
            }

            prevL = currentL;
            prevR = currentR;
        }
    }

    std::printf("coefficient continuity = %s\n", ok ? "ok" : "fail");
    return ok ? 0 : 4;
}
