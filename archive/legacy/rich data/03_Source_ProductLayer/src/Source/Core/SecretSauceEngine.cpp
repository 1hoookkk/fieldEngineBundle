#include "SecretSauceEngine.h"
#include <cmath>

//==============================================================================
SecretSauceEngine::SecretSauceEngine()
{
    // Initialize the secret components
    for (auto& filter : emuFilters)
    {
        filter.reset();
    }
    
    for (auto& amp : tubeAmps)
    {
        // Initialize tube characteristics to sweet spot values
        amp.glow_factor = 0.3f;
        amp.sag_amount = 0.2f;
        amp.air_presence = 0.15f;
        amp.harmonic_content = 0.25f;
    }
    
    // Set up mastering processor with optimal settings
    auto& bands = masteringProcessor.bands;
    bands[0].frequency = 60.0f;    // Sub bass
    bands[1].frequency = 250.0f;   // Low mids
    bands[2].frequency = 2000.0f;  // High mids 
    bands[3].frequency = 8000.0f;  // Highs
    
    // Set optimal secret sauce settings
    settings.overall_intensity = 0.7f; // Sweet spot - noticeable but not obvious
    settings.adaptive_processing = true;
    settings.preserve_dynamics = true;
    settings.gentle_processing = true;
}

SecretSauceEngine::~SecretSauceEngine()
{
    releaseResources();
}

//==============================================================================
// Core Audio Processing

void SecretSauceEngine::prepareToPlay(double sampleRate, int samplesPerBlock, int numChannels)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    
    // Initialize all filters and processors for the sample rate
    for (auto& filter : emuFilters)
    {
        filter.setParameters(1000.0f, 0.0f, 1.0f, sampleRate);
    }
    
    // Update tube amp characteristics for sample rate
    for (auto& amp : tubeAmps)
    {
        amp.updateTubeCharacteristics(amp.glow_factor, amp.sag_amount, amp.air_presence);
    }
}

void SecretSauceEngine::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (bypassMode.load() || !isEnabled.load())
        return;
    
    applySecretSauce(buffer, settings.overall_intensity);
}

void SecretSauceEngine::releaseResources()
{
    for (auto& filter : emuFilters)
    {
        filter.reset();
    }
}

void SecretSauceEngine::applySecretSauce(juce::AudioBuffer<float>& buffer, float intensity)
{
    if (intensity <= 0.0f) return;
    
    // Analyze the audio content for intelligent processing
    audioAnalyzer.analyzeBuffer(buffer);
    audioAnalyzer.updateProcessingHints();
    
    // Update settings based on content analysis if adaptive processing is enabled
    if (settings.adaptive_processing)
    {
        updateIntelligentSettings();
        adaptToAudioContent(buffer);
    }
    
    // Apply the secret sauce in optimal order for best sound quality
    applyEMUFiltering(buffer);          // Vintage EMU character first
    applyTubeAmplification(buffer);     // Tube warmth and harmonics
    applyAnalogCharacter(buffer);       // Analog coloration and tape saturation
    applyPsychoacousticEnhancement(buffer); // Spatial and psychoacoustic enhancement
    applyMasteringGrade(buffer);        // Final mastering polish
}

//==============================================================================
// EMU Filter Magic Implementation

void SecretSauceEngine::VintageEMUFilter::setParameters(float newCutoff, float newResonance, float newDrive, double sampleRate)
{
    cutoff = newCutoff;
    resonance = newResonance;
    drive = newDrive;
    sample_rate_factor = static_cast<float>(44100.0 / sampleRate);
    
    // Calculate vintage drift amount (simulates analog component drift)
    vintage_drift = juce::Random::getSystemRandom().nextFloat() * 0.02f - 0.01f;
    vintage_nonlinearity = 0.05f + juce::Random::getSystemRandom().nextFloat() * 0.03f;
}

float SecretSauceEngine::VintageEMUFilter::process(float input, EMUFilterType type)
{
    // Apply input drive with vintage nonlinearity
    float driven_input = input * drive;
    driven_input = applyVintageNonlinearity(driven_input);
    
    // Apply analog drift simulation
    driven_input = simulateAnalogDrift(driven_input);
    
    // EMU-style 4-pole filter processing (based on reverse-engineered EMU characteristics)
    float frequency_factor = cutoff / (22050.0f * sample_rate_factor);
    frequency_factor = juce::jlimit(0.01f, 0.99f, frequency_factor);
    
    // Classic EMU filter algorithm with our enhancements
    float f = 2.0f * std::sin(juce::MathConstants<float>::pi * frequency_factor);
    float q = resonance * 0.95f + 0.05f; // Subtle minimum Q for character
    float fb = q + q / (1.0f - f);
    
    // 4-pole cascade with EMU characteristics
    lp1 += f * (driven_input - lp1 + fb * (lp1 - lp4));
    lp2 += f * (lp1 - lp2);
    lp3 += f * (lp2 - lp3);
    lp4 += f * (lp3 - lp4);
    
    return applyEMUCharacteristics(lp4, type);
}

float SecretSauceEngine::VintageEMUFilter::applyEMUCharacteristics(float input, EMUFilterType type)
{
    switch (type)
    {
        case EMUFilterType::EMU_Classic:
            // Classic EMU sampler character - slightly darker, warmer
            return input * 0.95f + std::tanh(input * 0.1f) * 0.05f;
            
        case EMUFilterType::EMU_Vintage:
            // SP-1200 style - more aggressive saturation
            return std::tanh(input * 1.2f) * 0.85f;
            
        case EMUFilterType::EMU_Modern:
            // Modern EMU - cleaner but with subtle harmonic content
            return input * 0.98f + std::sin(input * juce::MathConstants<float>::pi) * 0.02f;
            
        case EMUFilterType::EMU_Resonant:
            // High resonance with controlled feedback
            return input + std::tanh(resonance * input * 0.3f) * 0.1f;
            
        case EMUFilterType::EMU_Smooth:
            // Smooth musical filtering
            return input * (0.95f + 0.05f * std::sin(input * 2.0f));
            
        default:
            return input;
    }
}

float SecretSauceEngine::VintageEMUFilter::simulateAnalogDrift(float input)
{
    // Simulate slow analog component drift
    vintage_drift += (juce::Random::getSystemRandom().nextFloat() - 0.5f) * 0.0001f;
    vintage_drift = juce::jlimit(-0.02f, 0.02f, vintage_drift);
    
    return input * (1.0f + vintage_drift);
}

float SecretSauceEngine::VintageEMUFilter::applyVintageNonlinearity(float input)
{
    // Subtle nonlinearity that adds character without being obvious
    return input + std::tanh(input * vintage_nonlinearity) * 0.05f;
}

void SecretSauceEngine::VintageEMUFilter::reset()
{
    lp1 = lp2 = lp3 = lp4 = 0.0f;
    bp1 = bp2 = 0.0f;
    hp1 = 0.0f;
    delay1 = delay2 = delay3 = delay4 = 0.0f;
    vintage_drift = 0.0f;
}

//==============================================================================
// Tube Amplifier Magic Implementation

float SecretSauceEngine::TubeAmplifierModel::process(float input, double sampleRate)
{
    // Update tube state based on input
    float abs_input = std::abs(input);
    envelope_follower = envelope_follower * 0.999f + abs_input * 0.001f;
    
    // Simulate tube saturation
    float saturated = simulateTubeSaturation(input);
    
    // Add tube harmonics
    float with_harmonics = applyTubeHarmonics(saturated);
    
    // Apply power supply sag
    float with_sag = simulatePowerSupplySag(with_harmonics);
    
    // Apply tube frequency response
    float with_frequency_response = applyTubeFrequencyResponse(with_sag);
    
    // Add thermal drift
    float final_output = simulateThermalDrift(with_frequency_response);
    
    return final_output;
}

float SecretSauceEngine::TubeAmplifierModel::simulateTubeSaturation(float input)
{
    // Classic tube saturation curve - smooth compression and harmonics
    float drive_factor = 1.0f + glow_factor * 2.0f;
    float driven = input * drive_factor;
    
    // Asymmetric tube saturation (tubes saturate differently on positive/negative swings)
    if (driven > 0.0f)
    {
        return std::tanh(driven * 0.7f) * 1.1f;
    }
    else
    {
        return std::tanh(driven * 0.8f) * 0.9f;
    }
}

float SecretSauceEngine::TubeAmplifierModel::applyTubeHarmonics(float input)
{
    // Generate subtle even harmonics (2nd, 4th) characteristic of tubes
    harmonic_generator_phase += std::abs(input) * 0.1f;
    if (harmonic_generator_phase > juce::MathConstants<float>::twoPi)
        harmonic_generator_phase -= juce::MathConstants<float>::twoPi;
    
    float second_harmonic = std::sin(harmonic_generator_phase * 2.0f) * harmonic_content * 0.1f;
    float fourth_harmonic = std::sin(harmonic_generator_phase * 4.0f) * harmonic_content * 0.05f;
    
    return input + (second_harmonic + fourth_harmonic) * std::abs(input);
}

float SecretSauceEngine::TubeAmplifierModel::simulatePowerSupplySag(float input)
{
    // Simulate power supply sag under load
    float load = envelope_follower;
    sag_envelope = sag_envelope * 0.995f + load * 0.005f;
    
    float sag_reduction = 1.0f - (sag_envelope * sag_amount * 0.3f);
    return input * sag_reduction;
}

float SecretSauceEngine::TubeAmplifierModel::applyTubeFrequencyResponse(float input)
{
    // Tube amps have characteristic frequency response - slight high-end rolloff and mid boost
    // Simplified single-pole filter simulation
    eq_state[0] = eq_state[0] * 0.95f + input * 0.05f; // High-frequency rolloff
    eq_state[1] = eq_state[1] * 0.98f + input * 0.02f; // Mid-frequency boost
    
    return input * 0.7f + eq_state[0] * 0.2f + eq_state[1] * 0.1f;
}

float SecretSauceEngine::TubeAmplifierModel::simulateThermalDrift(float input)
{
    // Simulate thermal drift - very subtle changes over time
    thermal_drift += (juce::Random::getSystemRandom().nextFloat() - 0.5f) * 0.00001f;
    thermal_drift = juce::jlimit(-0.005f, 0.005f, thermal_drift);
    
    return input * (1.0f + thermal_drift);
}

void SecretSauceEngine::TubeAmplifierModel::updateTubeCharacteristics(float glow, float sag, float air)
{
    glow_factor = juce::jlimit(0.0f, 1.0f, glow);
    sag_amount = juce::jlimit(0.0f, 1.0f, sag);
    air_presence = juce::jlimit(0.0f, 1.0f, air);
}

//==============================================================================
// Analog Character Implementation

float SecretSauceEngine::AnalogCharacterProcessor::process(float input, double sampleRate)
{
    float processed = input;
    
    // Apply tape saturation
    processed = applyTapeSaturation(processed);
    
    // Apply console coloration
    processed = applyConsoleColoration(processed);
    
    // Apply vintage compression
    processed = applyVintageCompression(processed);
    
    // Add subtle analog noise floor
    processed = addAnalogNoise(processed);
    
    return processed;
}

float SecretSauceEngine::AnalogCharacterProcessor::applyTapeSaturation(float input)
{
    // Tape saturation with hysteresis
    float driven = input * (1.0f + tape_saturation);
    
    // Hysteresis effect
    tape_hysteresis = tape_hysteresis * 0.9f + driven * 0.1f;
    float hysteresis_effect = (driven - tape_hysteresis) * 0.1f;
    
    return std::tanh(driven + hysteresis_effect) * 0.8f;
}

float SecretSauceEngine::AnalogCharacterProcessor::applyConsoleColoration(float input)
{
    // Console-style harmonic coloration
    console_harmonic_phase += std::abs(input) * 0.2f;
    if (console_harmonic_phase > juce::MathConstants<float>::twoPi)
        console_harmonic_phase -= juce::MathConstants<float>::twoPi;
    
    float harmonic = std::sin(console_harmonic_phase * 3.0f) * console_coloration * 0.05f;
    return input + harmonic * std::abs(input);
}

float SecretSauceEngine::AnalogCharacterProcessor::applyVintageCompression(float input)
{
    // Gentle vintage-style compression
    float abs_input = std::abs(input);
    compressor_envelope = compressor_envelope * 0.999f + abs_input * 0.001f;
    
    float threshold = 0.7f;
    if (compressor_envelope > threshold)
    {
        float over_threshold = compressor_envelope - threshold;
        float compression_amount = over_threshold * vintage_compression;
        return input * (1.0f - compression_amount);
    }
    
    return input;
}

float SecretSauceEngine::AnalogCharacterProcessor::addAnalogNoise(float input)
{
    // Very subtle analog noise floor
    noise_generator_state = noise_generator_state * 1664525 + 1013904223; // Linear congruential generator
    float noise = (static_cast<float>(noise_generator_state) / 4294967296.0f - 0.5f) * 2.0f;
    
    float noise_level = std::pow(10.0f, analog_noise_floor / 20.0f);
    return input + noise * noise_level * 0.001f;
}

//==============================================================================
// Psychoacoustic Enhancement Implementation

void SecretSauceEngine::PsychoacousticEnhancer::processStereo(float& left, float& right, double sampleRate)
{
    enhanceStereoWidth(left, right);
    enhanceDepth(left, right);
    enhancePresence(left, right);
    enhanceClarity(left, right);
}

void SecretSauceEngine::PsychoacousticEnhancer::enhanceStereoWidth(float& left, float& right)
{
    if (stereo_width == 1.0f) return;
    
    float mid = (left + right) * 0.5f;
    float side = (left - right) * 0.5f;
    
    side *= stereo_width;
    
    left = mid + side;
    right = mid - side;
}

void SecretSauceEngine::PsychoacousticEnhancer::enhanceDepth(float& left, float& right)
{
    // Use Haas effect for depth enhancement
    delay_buffer_left[delay_index] = left;
    delay_buffer_right[delay_index] = right;
    
    int delayed_index = (delay_index - 32) & 63; // 32-sample delay (~0.7ms at 44.1kHz)
    
    haas_delay_left = delay_buffer_left[delayed_index];
    haas_delay_right = delay_buffer_right[delayed_index];
    
    left += haas_delay_right * depth_enhancement * 0.3f;
    right += haas_delay_left * depth_enhancement * 0.3f;
    
    delay_index = (delay_index + 1) & 63;
}

void SecretSauceEngine::PsychoacousticEnhancer::enhancePresence(float& left, float& right)
{
    // Subtle high-frequency enhancement for presence
    static float hf_state_left = 0.0f, hf_state_right = 0.0f;
    
    // High-pass filter to isolate high frequencies
    float hf_left = left - hf_state_left;
    float hf_right = right - hf_state_right;
    
    hf_state_left = hf_state_left * 0.98f + left * 0.02f;
    hf_state_right = hf_state_right * 0.98f + right * 0.02f;
    
    // Add enhanced high frequencies back
    left += hf_left * presence_boost * 0.2f;
    right += hf_right * presence_boost * 0.2f;
}

void SecretSauceEngine::PsychoacousticEnhancer::enhanceClarity(float& left, float& right)
{
    // Transient enhancement for clarity
    static float prev_left = 0.0f, prev_right = 0.0f;
    
    float transient_left = left - prev_left;
    float transient_right = right - prev_right;
    
    if (std::abs(transient_left) > 0.1f)
        left += transient_left * clarity_factor * 0.15f;
    
    if (std::abs(transient_right) > 0.1f)
        right += transient_right * clarity_factor * 0.15f;
    
    prev_left = left * 0.9f + prev_left * 0.1f;
    prev_right = right * 0.9f + prev_right * 0.1f;
}

//==============================================================================
// Mastering Processor Implementation

void SecretSauceEngine::MasteringProcessor::processMastering(juce::AudioBuffer<float>& buffer, double sampleRate)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float input = channelData[sample];
            
            // Apply multiband processing
            applyMultibandProcessing(input, channel);
            
            // Apply harmonic excitement
            applyHarmonicExcitement(input);
            
            // Apply gentle limiting
            applyGentleLimiting(input);
            
            channelData[sample] = input;
        }
    }
}

void SecretSauceEngine::MasteringProcessor::applyMultibandProcessing(float& sample, int channel)
{
    // Simplified 4-band processing
    for (auto& band : bands)
    {
        // Simple filtering (would be more sophisticated in real implementation)
        if (band.frequency < 200.0f)
        {
            // Low band processing
            float low_content = sample * 0.3f;
            sample += low_content * (band.gain - 1.0f) * subtle_eq_adjustment;
        }
        else if (band.frequency < 2000.0f)
        {
            // Mid band processing
            float mid_content = sample * 0.4f;
            sample += mid_content * (band.gain - 1.0f) * subtle_eq_adjustment;
        }
        else
        {
            // High band processing
            float high_content = sample * 0.3f;
            sample += high_content * (band.gain - 1.0f) * subtle_eq_adjustment;
        }
    }
}

void SecretSauceEngine::MasteringProcessor::applyGentleLimiting(float& sample)
{
    float abs_sample = std::abs(sample);
    
    if (abs_sample > 0.95f)
    {
        limiter_envelope = limiter_envelope * 0.99f + abs_sample * 0.01f;
        float reduction = 0.95f / limiter_envelope;
        sample *= juce::jlimit(0.5f, 1.0f, reduction);
    }
    else
    {
        limiter_envelope *= 0.999f;
    }
}

void SecretSauceEngine::MasteringProcessor::applyHarmonicExcitement(float& sample)
{
    // Subtle harmonic excitement
    float harmonic = std::tanh(sample * 3.0f) * harmonic_excitement * 0.1f;
    sample = sample * 0.95f + harmonic * 0.05f;
}

//==============================================================================
// Audio Analysis Implementation

void SecretSauceEngine::AudioAnalyzer::analyzeBuffer(const juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumSamples() == 0) return;
    
    // Calculate RMS and peak levels
    rms_level = 0.0f;
    peak_level = 0.0f;
    
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* channelData = buffer.getReadPointer(channel);
        
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float abs_sample = std::abs(channelData[sample]);
            rms_level += channelData[sample] * channelData[sample];
            peak_level = juce::jmax(peak_level, abs_sample);
        }
    }
    
    rms_level = std::sqrt(rms_level / (buffer.getNumSamples() * buffer.getNumChannels()));
    dynamic_range = peak_level - rms_level;
    
    // Perform spectral analysis (simplified)
    performSpectralAnalysis(buffer);
    classifyAudioContent();
}

void SecretSauceEngine::AudioAnalyzer::performSpectralAnalysis(const juce::AudioBuffer<float>& buffer)
{
    // Simplified spectral analysis - would use FFT in real implementation
    const float* channelData = buffer.getReadPointer(0);
    int numSamples = juce::jmin(buffer.getNumSamples(), 256);
    
    // Calculate spectral centroid (simplified)
    float weighted_sum = 0.0f;
    float magnitude_sum = 0.0f;
    
    for (int i = 0; i < numSamples; ++i)
    {
        float magnitude = std::abs(channelData[i]);
        magnitude_spectrum[i] = magnitude;
        weighted_sum += magnitude * i;
        magnitude_sum += magnitude;
    }
    
    spectral_centroid = (magnitude_sum > 0.0f) ? (weighted_sum / magnitude_sum) : 0.0f;
}

void SecretSauceEngine::AudioAnalyzer::classifyAudioContent()
{
    // Simple content classification
    is_percussive = (peak_level > 0.5f && dynamic_range > 0.3f);
    is_harmonic = (spectral_centroid > 50.0f && spectral_centroid < 150.0f);
    is_vocal = (spectral_centroid > 80.0f && spectral_centroid < 120.0f && rms_level > 0.1f);
    needs_warmth = (spectral_centroid > 150.0f);
    needs_brightness = (spectral_centroid < 50.0f);
}

void SecretSauceEngine::AudioAnalyzer::updateProcessingHints()
{
    // This would update processing parameters based on content analysis
    // Implementation depends on specific processing goals
}

//==============================================================================
// Core Processing Pipeline

void SecretSauceEngine::applyEMUFiltering(juce::AudioBuffer<float>& buffer)
{
    if (settings.emu_filter_intensity <= 0.0f) return;
    
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        auto& filter = emuFilters[channel % 2]; // Use stereo pair
        
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float input = channelData[sample];
            float filtered = filter.process(input, EMUFilterType::EMU_Classic);
            
            // Blend with original signal
            channelData[sample] = input * (1.0f - settings.emu_filter_intensity) + 
                                 filtered * settings.emu_filter_intensity;
        }
    }
}

void SecretSauceEngine::applyTubeAmplification(juce::AudioBuffer<float>& buffer)
{
    if (settings.tube_amp_intensity <= 0.0f) return;
    
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        auto& amp = tubeAmps[channel % 2]; // Use stereo pair
        
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float input = channelData[sample];
            float processed = amp.process(input, currentSampleRate);
            
            // Blend with original signal
            channelData[sample] = input * (1.0f - settings.tube_amp_intensity) + 
                                 processed * settings.tube_amp_intensity;
        }
    }
}

void SecretSauceEngine::applyAnalogCharacter(juce::AudioBuffer<float>& buffer)
{
    if (settings.analog_character_intensity <= 0.0f) return;
    
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        auto& processor = analogProcessors[channel % 2]; // Use stereo pair
        
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float input = channelData[sample];
            float processed = processor.process(input, currentSampleRate);
            
            // Blend with original signal
            channelData[sample] = input * (1.0f - settings.analog_character_intensity) + 
                                 processed * settings.analog_character_intensity;
        }
    }
}

void SecretSauceEngine::applyPsychoacousticEnhancement(juce::AudioBuffer<float>& buffer)
{
    if (settings.psychoacoustic_intensity <= 0.0f || buffer.getNumChannels() < 2) return;
    
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float left = buffer.getSample(0, sample);
        float right = buffer.getSample(1, sample);
        
        psychoacousticEnhancer.processStereo(left, right, currentSampleRate);
        
        buffer.setSample(0, sample, left);
        buffer.setSample(1, sample, right);
    }
}

void SecretSauceEngine::applyMasteringGrade(juce::AudioBuffer<float>& buffer)
{
    if (settings.mastering_intensity <= 0.0f) return;
    
    masteringProcessor.processMastering(buffer, currentSampleRate);
}

//==============================================================================
// Dynamic Brush Control Integration

void SecretSauceEngine::updateTubeCharacteristicsFromBrush(float pressure, float velocity, juce::Colour brushColor)
{
    // Update brush state
    brushState.currentPressure = juce::jlimit(0.0f, 1.0f, pressure);
    brushState.currentVelocity = velocity;
    brushState.currentColor = brushColor;
    
    // Determine pressure direction for hysteresis
    brushState.pressureIncreasing = (pressure > brushState.smoothedPressure);
    
    // Apply smoothing to prevent audio artifacts
    brushState.updateSmoothing(pressure, velocity);
    
    // Get pressure with hysteresis for organic feel
    float effectivePressure = brushState.getPressureWithHysteresis();
    
    //==============================================================================
    // Pressure → Tube Saturation Mapping (Following Gemini's recommendations)
    
    // Option 1: Exponential mapping (subtle at low pressures, dramatic at high)
    float exponentialSaturation = std::pow(effectivePressure, brushState.exponentialExponent);
    
    // Option 2: Sigmoid mapping (creates a "sweet spot" breakup point)
    float sigmoidSaturation = 1.0f / (1.0f + std::exp(-brushState.sigmoidSlope * (effectivePressure - brushState.sigmoidThreshold)));
    
    // Blend between mapping curves based on velocity (fast strokes = more exponential)
    float velocityFactor = juce::jlimit(0.0f, 1.0f, std::abs(velocity) * 2.0f);
    float blendedSaturation = exponentialSaturation * velocityFactor + sigmoidSaturation * (1.0f - velocityFactor);
    
    //==============================================================================
    // Apply Pressure to Tube Parameters (Multi-faceted approach per Gemini)
    
    for (auto& amp : tubeAmps)
    {
        // 1. Drive: Direct pressure → input gain
        float driveAmount = 1.0f + blendedSaturation * 3.0f; // 1.0 to 4.0x drive
        
        // 2. Glow Factor: Controls tube warmth and even harmonics
        float glowFactor = 0.2f + blendedSaturation * 0.6f; // 0.2 to 0.8
        
        // 3. Bias Voltage: Affects saturation symmetry
        float biasOffset = blendedSaturation * 0.5f - 0.25f; // -0.25V to +0.25V offset
        amp.bias_voltage = -2.0f + biasOffset;
        
        // 4. Power Supply Sag: More sag with higher pressure
        float sagAmount = 0.1f + blendedSaturation * 0.4f; // 0.1 to 0.5
        
        // Update tube characteristics
        amp.updateTubeCharacteristics(glowFactor, sagAmount, amp.air_presence);
        
        // Store drive amount for use in processing
        amp.glow_factor = glowFactor; // Repurpose for drive storage
    }
    
    //==============================================================================
    // Brush Color → Tube Tone Mapping (Creative enhancement per Gemini)
    
    float hue = brushColor.getHue();
    float saturation = brushColor.getSaturation();
    float brightness = brushColor.getBrightness();
    
    for (auto& amp : tubeAmps)
    {
        // Hue → Harmonic Content (warmer colors = more even harmonics)
        float harmonicRatio = 0.5f + (hue < 0.5f ? hue : (1.0f - hue)) * 0.5f;
        amp.harmonic_content = 0.15f + harmonicRatio * 0.3f; // 0.15 to 0.45
        
        // Saturation → Tube Type Character (vivid = aggressive, muted = clean)
        float tubeAggressiveness = saturation;
        amp.plate_voltage = 200.0f + tubeAggressiveness * 100.0f; // 200V to 300V
        
        // Brightness → Air Presence (brighter colors = more high-frequency content)
        amp.air_presence = 0.1f + brightness * 0.3f; // 0.1 to 0.4
    }
    
    //==============================================================================
    // Update Processing Intensities Based on Gesture
    
    // More aggressive processing for higher pressure
    settings.tube_amp_intensity = 0.4f + effectivePressure * 0.4f; // 0.4 to 0.8
    
    // Velocity affects EMU filter intensity (fast strokes = more filtering)
    float velocityIntensity = juce::jlimit(0.0f, 0.5f, std::abs(velocity) * 0.5f);
    settings.emu_filter_intensity = 0.6f + velocityIntensity; // 0.6 to 1.1
    
    // Color saturation affects analog character
    settings.analog_character_intensity = 0.3f + saturation * 0.3f; // 0.3 to 0.6
}

//==============================================================================
// Intelligent Adaptation

void SecretSauceEngine::updateIntelligentSettings()
{
    // Adapt processing based on audio content
    if (audioAnalyzer.is_percussive)
    {
        settings.emu_filter_intensity *= settings.percussive_emphasis;
        settings.tube_amp_intensity *= 0.9f; // Less tube saturation on drums
    }
    
    if (audioAnalyzer.is_harmonic)
    {
        settings.analog_character_intensity *= settings.harmonic_enhancement;
        settings.psychoacoustic_intensity *= 1.1f;
    }
    
    if (audioAnalyzer.is_vocal)
    {
        settings.psychoacoustic_intensity *= settings.vocal_presence;
        settings.mastering_intensity *= 1.15f;
    }
    
    if (audioAnalyzer.needs_warmth)
    {
        settings.tube_amp_intensity *= 1.2f;
        settings.analog_character_intensity *= 1.1f;
    }
    
    if (audioAnalyzer.needs_brightness)
    {
        settings.psychoacoustic_intensity *= 1.1f;
        psychoacousticEnhancer.presence_boost *= 1.2f;
    }
}

void SecretSauceEngine::adaptToAudioContent(const juce::AudioBuffer<float>& buffer)
{
    // Real-time adaptation based on content
    float energy = audioAnalyzer.rms_level;
    
    // Adapt intensity based on signal energy
    if (energy > 0.7f)
    {
        // Loud signal - reduce processing to avoid over-processing
        settings.overall_intensity *= 0.8f;
    }
    else if (energy < 0.1f)
    {
        // Quiet signal - increase processing for enhancement
        settings.overall_intensity *= 1.2f;
    }
    
    // Limit intensity to reasonable range
    settings.overall_intensity = juce::jlimit(0.3f, 1.0f, settings.overall_intensity);
}

float SecretSauceEngine::calculateQualityMetric(const juce::AudioBuffer<float>& buffer)
{
    // Calculate a quality metric to monitor processing effectiveness
    float metric = 1.0f;
    
    // Check for clipping
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* channelData = buffer.getReadPointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            if (std::abs(channelData[sample]) > 0.99f)
            {
                metric *= 0.9f; // Penalize clipping
            }
        }
    }
    
    qualityMetric.store(metric);
    return metric;
}