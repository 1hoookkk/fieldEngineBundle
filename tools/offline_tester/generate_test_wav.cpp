// Simple test WAV generator - creates a sine wave with pitch variations
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>

int main() {
    juce::MessageManager::getInstance();

    const double sampleRate = 44100.0;
    const int numChannels = 1;
    const int numSamples = 44100 * 3; // 3 seconds

    // Create audio buffer
    juce::AudioBuffer<float> buffer(numChannels, numSamples);
    buffer.clear();

    // Generate a sine wave that slides from 440Hz to 415Hz (slightly flat)
    auto* data = buffer.getWritePointer(0);
    double phase = 0.0;

    for (int i = 0; i < numSamples; ++i) {
        // Frequency slides from A4 (440Hz) to slightly flat
        double t = (double)i / sampleRate;
        double freq = 440.0 - (25.0 * t / 3.0); // Slide down 25Hz over 3 seconds

        data[i] = 0.5f * std::sin(phase);
        phase += 2.0 * M_PI * freq / sampleRate;

        // Keep phase in reasonable range
        while (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;
    }

    // Write to WAV file
    juce::File outputFile("C:\\fieldEngineBundle\\build\\tester\\test_input.wav");
    outputFile.deleteFile();

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> outputStream(outputFile.createOutputStream());

    if (outputStream) {
        std::unique_ptr<juce::AudioFormatWriter> writer(
            wavFormat.createWriterFor(outputStream.get(), sampleRate, numChannels, 16, {}, 0)
        );

        if (writer) {
            outputStream.release(); // Writer takes ownership
            writer->writeFromAudioSampleBuffer(buffer, 0, numSamples);
            writer.reset();

            juce::Logger::writeToLog("Created test WAV: " + outputFile.getFullPathName());
        }
    }

    return 0;
}