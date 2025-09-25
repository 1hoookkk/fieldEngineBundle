#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <memory>

// Forward declaration
class SpectralMask;

class ForgeVoice
{
public:
    ForgeVoice() = default;

    void prepare(double sampleRate, int blockSize);
    void setSample(juce::AudioBuffer<float>&& newBuffer, double originalBPM = 120.0);
    void process(juce::AudioBuffer<float>& output, int startSample, int numSamples);

    // Control
    void start();
    void stop();
    void reset();
    bool isActive() const { return isPlaying; }

    // Parameters
    void setPitch(float semitones);
    void setSpeed(float speed);
    void setSyncMode(bool sync);
    void setHostBPM(double bpm);
    void setVolume(float vol) { volume = vol; }
    void setDrive(float drv) { drive = juce::jlimit(1.0f, 10.0f, drv); }
    void setCrush(float bits) { crushBits = juce::jlimit(1.0f, 16.0f, bits); }

    // Info
    juce::String getSampleName() const { return sampleName; }
    bool hasSample() const { return buffer.getNumSamples() > 0; }
    float getProgress() const { return buffer.getNumSamples() > 0 ? position / (double)buffer.getNumSamples() : 0.0f; }
    
    // Spectral masking
    void enableSpectralMask(bool enable);
    bool isSpectralMaskEnabled() const { return spectralMaskEnabled; }
    class SpectralMask* getSpectralMask() { return spectralMask.get(); }
    const juce::AudioBuffer<float>& getSampleBuffer() const { return buffer; }

private:
    // Audio data
    juce::AudioBuffer<float> buffer;
    juce::AudioBuffer<float> processBuffer;
    juce::String sampleName;

    // Playback state
    double position = 0.0;
    double playbackRate = 1.0;
    bool isPlaying = false;

    // Parameters
    float volume = 0.7f;
    float pitch = 0.0f;      // in semitones
    float speed = 1.0f;      // playback speed multiplier
    float drive = 1.0f;      // distortion amount
    float crushBits = 16.0f; // bit crushing

    // Sync
    bool syncEnabled = false;
    double hostBPM = 120.0;
    double originalBPM = 120.0;
    double sampleRate = 44100.0;

    // DSP
    juce::dsp::Oversampling<float> oversampling{ 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> pitchSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> volumeSmooth;
    
    // Spectral masking
    std::unique_ptr<SpectralMask> spectralMask;
    bool spectralMaskEnabled = false;

    // Helpers
    void updatePlaybackRate();
    float processSample(float input);
    // Disallow copying and assignment:
    ForgeVoice(const ForgeVoice&) = delete;
    ForgeVoice& operator= (const ForgeVoice&) = delete;

};