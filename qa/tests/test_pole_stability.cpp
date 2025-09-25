#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_core/juce_core.h>
#include <cstdio>

#include "qa/tests/PlaneTestProbe.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    AuthenticEMUZPlane plane;
    bool ok = true;

    const double sampleRates[] = { 44100.0, 48000.0, 96000.0 };
    const float morphPositions[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };

    juce::AudioBuffer<float> buffer(2, 64);
    buffer.clear();

    for (double sr : sampleRates)
    {
        plane.prepareToPlay(sr);
        plane.setIntensity(1.0f);

        for (int pair = 0; pair < AUTHENTIC_EMU_NUM_PAIRS; ++pair)
        {
            plane.setMorphPair(pair);

            for (float morph : morphPositions)
            {
                plane.setMorphPosition(morph);
                buffer.clear();
                plane.process(buffer);

                auto poles = AuthenticEMUZPlaneTestProbe::poles(plane);
                for (const auto& p : poles)
                {
                    if (! std::isfinite(p.r) || ! std::isfinite(p.theta))
                    {
                        ok = false;
                        break;
                    }

                    if (p.r < AuthenticEMUZPlane::minPoleRadius - 1.0e-4f
                        || p.r > AuthenticEMUZPlane::maxPoleRadius - AuthenticEMUZPlane::stabilityMargin)
                    {
                        ok = false;
                        break;
                    }
                }

                if (! ok)
                    break;
            }

            if (! ok)
                break;
        }

        if (! ok)
            break;
    }

    std::printf("pole stability = %s\n", ok ? "ok" : "fail");
    return ok ? 0 : 3;
}
