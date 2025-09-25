    #pragma once
    #include <JuceHeader.h>

    class CanvasProcessor
    {
    public:
        CanvasProcessor();
        ~CanvasProcessor();

        void prepareToPlay(double sampleRate, int samplesPerBlock);
        void processBlock(juce::AudioBuffer<float>& buffer);
        void updateFromImage(const juce::Image& image);

        // --- Control Methods ---
        void setActive(bool shouldBeActive) { isActive = shouldBeActive; }
        void setPlayheadPosition(float normalisedPosition);
        void setFrequencyRange(float minHz, float maxHz);
        void setMasterGain(float gain) { masterGain.setTargetValue(gain); }
        void setAmplitudeScale(float scale) { amplitudeScale = scale; }
        void setUsePanning(bool shouldUsePanning) { usePanning = shouldUsePanning; }

    private:
        // Nested struct for a single sine wave partial
        struct Partial
        {
            float frequency = 0.0f;
            float phase = 0.0f;
            float amplitude = 0.0f;
            float targetAmplitude = 0.0f;
            float pan = 0.5f; // 0.0 = left, 0.5 = center, 1.0 = right

            float getSample() const
            {
                return std::sin(phase * juce::MathConstants<float>::twoPi);
            }

            void updatePhase(float phaseIncrement)
            {
                phase += phaseIncrement;
                if (phase >= 1.0f)
                    phase -= 1.0f;
            }
        };

        // Main DSP methods
        void updateOscillatorsFromColumn(int x);
        float pixelYToFrequency(int y) const;

        // Member Variables
        juce::Image currentImage;
        std::vector<Partial> oscillators;

        // Parameters
        float sampleRate = 44100.0f;
        float playheadPos = 0.0f;
        int imageWidth = 0;
        int imageHeight = 0;

        bool isActive = false;
        bool usePanning = true;

        // Configurable settings
        int maxPartials = 512;
        float minFreq = 20.0f;
        float maxFreq = 20000.0f;
        float amplitudeScale = 1.0f; // Final scaling factor for amplitude

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> masterGain;
    };  