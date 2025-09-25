// Core/ForgeVoice.cpp
#include "ForgeVoice.h"
#include "SpectralMask.h"

void ForgeVoice::prepare(double sr, int blockSize)
{
    sampleRate = sr;
    processBuffer.setSize(2, blockSize);

    // Initialize DSP
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sr;
    spec.maximumBlockSize = static_cast<juce::uint32>(blockSize);
    spec.numChannels = 2;

    oversampling.initProcessing(static_cast<size_t>(blockSize));
    oversampling.reset();

    pitchSmooth.reset(sr, 0.02); // 20ms smoothing
    volumeSmooth.reset(sr, 0.01); // 10ms smoothing
}

void ForgeVoice::setSample(juce::AudioBuffer<float>&& newBuffer, double originalBPM)
{
    buffer = std::move(newBuffer);
    this->originalBPM = originalBPM;
    sampleName = "Sample " + juce::String(juce::Random::getSystemRandom().nextInt(1000));
    reset();
    
    // Analyze sample for spectral masking if enabled
    if (spectralMaskEnabled && spectralMask && buffer.getNumSamples() > 0)
    {
        spectralMask->analyzeSample(buffer, 0); // Analyze left channel
        DBG("ForgeVoice: Auto-analyzed sample for spectral masking: " << sampleName);
    }
}

void ForgeVoice::process(juce::AudioBuffer<float>& output, int startSample, int numSamples)
{
    // AUDIO DEBUG: Log voice activity (occasionally)
    static int voiceDebugCounter = 0;
    if (++voiceDebugCounter % 10000 == 0 && isPlaying)  // Log every ~4 minutes at 44.1kHz
    {
        DBG("AUDIO DEBUG: ForgeVoice processing - playing=" << (isPlaying ? "YES" : "NO")
            << " bufferSamples=" << buffer.getNumSamples() 
            << " position=" << position);
    }
    
    if (!isPlaying || buffer.getNumSamples() == 0)
        return;

    processBuffer.clear();

    // Update smoothed values
    pitchSmooth.setTargetValue(pitch);
    volumeSmooth.setTargetValue(volume);

    const int numChannels = juce::jmin(output.getNumChannels(), buffer.getNumChannels());

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Update playback rate for this sample
        updatePlaybackRate();

        // Get interpolated sample
        const int pos = static_cast<int>(position);
        const float frac = static_cast<float>(position - pos);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (pos < buffer.getNumSamples() - 1)
            {
                const float* channelData = buffer.getReadPointer(ch % buffer.getNumChannels());
                float sampleValue = channelData[pos] * (1.0f - frac) + channelData[pos + 1] * frac;

                // Apply processing
                sampleValue = processSample(sampleValue);

                // Apply volume with smoothing
                sampleValue *= volumeSmooth.getNextValue();

                // Write to output
                output.addSample(ch, startSample + sample, sampleValue);
            }
        }

        // Advance position
        position += playbackRate * pitchSmooth.getNextValue();

        // Handle loop/stop
        if (position >= buffer.getNumSamples())
        {
            position = 0.0;
            // For now, just loop. Later we can add one-shot mode
        }
    }
}

void ForgeVoice::start()
{
    isPlaying = true;
}

void ForgeVoice::stop()
{
    isPlaying = false;
}

void ForgeVoice::reset()
{
    position = 0.0;
    updatePlaybackRate();
}

void ForgeVoice::setPitch(float semitones)
{
    pitch = std::pow(2.0f, semitones / 12.0f);
}

void ForgeVoice::setSpeed(float spd)
{
    speed = juce::jlimit(0.1f, 4.0f, spd);
    updatePlaybackRate();
}

void ForgeVoice::setSyncMode(bool sync)
{
    syncEnabled = sync;
    updatePlaybackRate();
}

void ForgeVoice::setHostBPM(double bpm)
{
    hostBPM = bpm;
    updatePlaybackRate();
}

void ForgeVoice::updatePlaybackRate()
{
    playbackRate = speed;

    if (syncEnabled && hostBPM > 0 && originalBPM > 0)
    {
        playbackRate *= (hostBPM / originalBPM);
    }
}

float ForgeVoice::processSample(float input)
{
    float output = input;

    // Apply drive (simple tanh distortion)
    if (drive > 1.0f)
    {
        output = std::tanh(output * drive) / drive;
    }

    // Apply bit crushing
    if (crushBits < 16.0f)
    {
        const float scale = std::pow(2.0f, crushBits - 1.0f);
        output = std::round(output * scale) / scale;
    }

    return output;
}

//==============================================================================
// Spectral Masking Implementation

void ForgeVoice::enableSpectralMask(bool enable)
{
    spectralMaskEnabled = enable;
    
    if (enable && !spectralMask)
    {
        // Create and initialize spectral mask
        spectralMask = std::make_unique<SpectralMask>();
        spectralMask->prepareToPlay(sampleRate, 512); // Default block size
        
        // Analyze current sample if one is loaded
        if (hasSample())
        {
            spectralMask->analyzeSample(buffer, 0); // Analyze left channel
            DBG("ForgeVoice: Spectral mask analysis complete for " << sampleName);
        }
    }
    else if (!enable)
    {
        spectralMask.reset();
    }
}