#pragma once

#include <JuceHeader.h>
#include <vector>
#include <array>

/**
 * SpectralMask - MetaSynth-style spectral masking using drum samples
 * 
 * This class analyzes the spectral content of loaded samples (like hi-hats)
 * and uses that spectral information as a mask or filter for the audio synthesis.
 * 
 * Key Features:
 * - Real-time FFT analysis of drum samples
 * - Spectral envelope extraction
 * - Time-varying spectral masks
 * - Integration with ForgeVoice samples
 */
class SpectralMask
{
public:
    //==============================================================================
    // Constants
    static constexpr int FFT_ORDER = 10;  // 1024-point FFT
    static constexpr int FFT_SIZE = 1 << FFT_ORDER;
    static constexpr int SPECTRUM_BINS = FFT_SIZE / 2;
    static constexpr int MAX_ANALYSIS_LENGTH = 88200; // 2 seconds at 44.1kHz
    
    //==============================================================================
    // Mask Types
    enum class MaskType
    {
        Off = 0,           // No masking
        SpectralGate,      // Use spectrum as gate (pass/block frequencies)
        SpectralFilter,    // Use spectrum as filter (attenuate frequencies)
        RhythmicGate,      // Use envelope for rhythmic gating
        SpectralMorph      // Morphing spectral characteristics
    };
    
    //==============================================================================
    // Spectral Frame - represents spectrum at one time slice
    struct SpectralFrame
    {
        std::array<float, SPECTRUM_BINS> magnitudes;
        std::array<float, SPECTRUM_BINS> phases;
        float overallEnergy = 0.0f;
        float centroid = 0.0f;        // Spectral centroid
        float brightness = 0.0f;      // High-frequency content
        
        SpectralFrame()
        {
            magnitudes.fill(0.0f);
            phases.fill(0.0f);
        }
    };
    
    //==============================================================================
    // Main Interface
    
    SpectralMask();
    ~SpectralMask();
    
    // Audio processing
    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void processBlock(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>& maskSource);
    void releaseResources();
    
    // Sample analysis
    void analyzeSample(const juce::AudioBuffer<float>& sampleBuffer, int channel = 0);
    void clearAnalysis();
    
    // Mask control
    void setMaskType(MaskType type) { maskType = type; }
    MaskType getMaskType() const { return maskType; }
    void setMaskStrength(float strength) { maskStrength = juce::jlimit(0.0f, 1.0f, strength); }
    float getMaskStrength() const { return maskStrength; }
    void setTimeStretch(float stretch) { timeStretch = juce::jmax(0.1f, stretch); }
    float getTimeStretch() const { return timeStretch; }
    
    // Frequency analysis controls
    void setFrequencyRange(float minHz, float maxHz);
    void setSensitivity(float sensitivity) { this->sensitivity = juce::jlimit(0.0f, 1.0f, sensitivity); }
    void setSmoothing(float smoothing) { this->smoothing = juce::jlimit(0.0f, 0.99f, smoothing); }
    
    // Real-time info
    bool hasAnalyzedSample() const { return spectralFrames.size() > 0; }
    int getNumFrames() const { return static_cast<int>(spectralFrames.size()); }
    float getCurrentMaskPosition() const { return maskPosition; }
    const SpectralFrame* getCurrentFrame() const;
    
    // Visualization
    void getSpectralDisplay(std::vector<float>& magnitudes, int displayBins = 128) const;
    float getInstantaneousEnergy() const { return currentEnergy; }
    
private:
    //==============================================================================
    // Internal Processing
    
    void performFFT(const float* timeData, SpectralFrame& frame);
    void applySpectralGate(juce::AudioBuffer<float>& buffer, const SpectralFrame& maskFrame);
    void applySpectralFilter(juce::AudioBuffer<float>& buffer, const SpectralFrame& maskFrame);
    void applyRhythmicGate(juce::AudioBuffer<float>& buffer, const SpectralFrame& maskFrame);
    void updateMaskPosition();
    
    // Utility functions
    float binToFrequency(int bin) const;
    int frequencyToBin(float frequency) const;
    void smoothSpectralFrame(SpectralFrame& frame, const SpectralFrame& previousFrame);
    void calculateSpectralFeatures(SpectralFrame& frame);
    
    //==============================================================================
    // Member Variables
    
    // Audio processing state
    double sampleRate = 44100.0;
    int samplesPerBlock = 512;
    
    // FFT processing
    std::unique_ptr<juce::dsp::FFT> fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
    std::vector<float> fftData;
    std::vector<float> tempAudioBuffer;
    
    // Spectral analysis data
    std::vector<SpectralFrame> spectralFrames;
    int frameSize = 512;           // Hop size between frames
    int analysisPosition = 0;      // Current position in analysis
    
    // Mask playback state
    MaskType maskType = MaskType::Off;
    float maskPosition = 0.0f;     // 0.0-1.0 position through analyzed sample
    float maskStrength = 0.7f;     // 0.0-1.0 mask intensity
    float timeStretch = 1.0f;      // Time stretching factor
    
    // Frequency analysis parameters
    float minFrequency = 20.0f;
    float maxFrequency = 20000.0f;
    float sensitivity = 0.5f;      // How sensitive the mask is to spectral changes
    float smoothing = 0.3f;        // Temporal smoothing between frames
    
    // Real-time processing state
    SpectralFrame currentMaskFrame;
    SpectralFrame previousFrame;
    float currentEnergy = 0.0f;
    int frameCounter = 0;
    
    // Visualization state
    mutable std::vector<float> displayMagnitudes;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralMask)
};