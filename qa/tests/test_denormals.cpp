#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_core/juce_core.h>
#include <cstdio>
#include <cmath>

#include "libs/pitchengine_dsp/include/AuthenticEMUZPlane.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    AuthenticEMUZPlane plane;
    plane.prepareToPlay(48000.0);
    plane.setIntensity(1.0f);
    plane.setSectionSaturation(0.1f);

    juce::AudioBuffer<float> buffer(2, 256);
    buffer.clear();

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            buffer.setSample(ch, i, std::pow(0.25f, static_cast<float>(i + 1)));

    plane.process(buffer);

    bool ok = true;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float sample = buffer.getSample(ch, i);
            if (! std::isfinite(sample) || std::abs(sample) > 5.0f)
            {
                ok = false;
                break;
            }
        }

    std::printf("denormal test result = %s\n", ok ? "ok" : "fail");
    return ok ? 0 : 1;
}
