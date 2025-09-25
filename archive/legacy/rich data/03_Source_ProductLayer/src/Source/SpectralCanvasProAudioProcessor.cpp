#include "SpectralCanvasProAudioProcessor.h"
#include "SpectralCanvasProEditor.h"
#include "Core/DiagnosticLogger.h"
#include "Core/ColumnOps.h"
#include <chrono>

SpectralCanvasProAudioProcessor::SpectralCanvasProAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", Params::createParameterLayout())
{
    DBG("Processor ctor");
    // Set STFT latency immediately on construction for pluginval compatibility
    // This accounts for overlap-add reconstruction delay in STFT processing
    const int stftLatency = AtlasConfig::FFT_SIZE - AtlasConfig::HOP_SIZE; // 512-128 = 384
    updateReportedLatency(stftLatency);

    // Decay tau preset → SpectralPlayer via ParameterAttachment (JUCE ParameterAttachment)
    if (auto* p = apvts.getParameter(Params::ParameterIDs::decayTauPreset))
    {
        decayTauAttachment = std::make_unique<juce::ParameterAttachment>(*p,
            [this](float value) {
                const int idx = static_cast<int>(value);
                const float tau = (idx == 0 ? 120.0f : (idx == 2 ? 300.0f : 200.0f));
                spectralPlayer.setDecayTauMs(tau);
            });

        // Initialize tau from current value
        const int idx = static_cast<int>(p->getValue());
        const float tau = (idx == 0 ? 120.0f : (idx == 2 ? 300.0f : 200.0f));
        spectralPlayer.setDecayTauMs(tau);
    }
    // Register saturation parameter listeners
    apvts.addParameterListener(Params::ParameterIDs::saturationDrive, this);
    apvts.addParameterListener(Params::ParameterIDs::saturationMix, this);
}

SpectralCanvasProAudioProcessor::~SpectralCanvasProAudioProcessor()
{
#ifdef PHASE4_EXPERIMENT
    // Remove parameter listeners
    apvts.removeParameterListener(Params::ParameterIDs::useTestFeeder, this);
    apvts.removeParameterListener(Params::ParameterIDs::keyFilterEnabled, this);
    apvts.removeParameterListener(Params::ParameterIDs::oscGain, this);
    apvts.removeParameterListener(Params::ParameterIDs::scaleType, this);
    apvts.removeParameterListener(Params::ParameterIDs::rootNote, this);
    apvts.removeParameterListener(Params::ParameterIDs::useModernPaint, this);
    apvts.removeParameterListener(Params::ParameterIDs::mode, this);
    apvts.removeParameterListener(Params::ParameterIDs::blend, this);
    // respeed parameter removed for live insert mode
#endif
    apvts.removeParameterListener(Params::ParameterIDs::saturationDrive, this);
    apvts.removeParameterListener(Params::ParameterIDs::saturationMix, this);
}

void SpectralCanvasProAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    DBG("[reactivate] prepareToPlay sr=" << sampleRate << " spb=" << samplesPerBlock);
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    
    // set initial audio path from current params (message thread)
    setAudioPathFromParams();
    lastPath_ = currentPath_.load(std::memory_order_relaxed);
    
    // Initialize RT-safe sample loader
    sampleLoader.initialize(sampleRate);
    
    // Initialize RT-safe test feeder
    maskTestFeeder.initialize(sampleRate, AtlasConfig::NUM_BINS);
    
    // Initialize RT-safe spectral processing components
    spectralEngine = std::make_unique<SpectralEngine>();
    spectralEngine->initialize(sampleRate, samplesPerBlock);
    
    // Connect SampleLoader to SpectralEngine (RT-safe pointer sharing)
    spectralEngine->setSampleLoader(&sampleLoader);
    
    // Initialize modern JUCE DSP-based spectral painting processor
    spectralPaintProcessor = std::make_unique<SpectralPaintProcessor>();
    
    // === Initialize Paint-to-Audio Synthesis Components ===
    oscillatorBank.prepare(sampleRate, 128); // 128 oscillators
    oscillatorBank.setSmoothingTimesMs(5.0f, 15.0f); // 5ms gain, 15ms pan smoothing
    
    colorMapper.setTopN(16); // Use top 16 frequency bands
    colorMapper.setHueControlsPan(true); // Hue affects stereo position
    colorMapper.setBandCount(1024); // 1024 frequency bins
    
    dcBlocker.prepare(sampleRate, 2, 20.0); // 20Hz high-pass
    
    // Clear any existing paint events
    scp::PaintEvent dummy;
    while (paintEventQueue.pop(dummy)) {} // Drain queue
    
    // Initialize tiled atlas system
    initializeTiledAtlas();
    
    // Initialize Phase 4 (HEAR): RT-safe STFT masking
    hop_.prepare(sampleRate);
    hop_.setQueue(&maskDeltaQueue);
    hop_.setAtlas(tiledAtlas_.get(), tiledAtlas_ ? tiledAtlas_->allocateFreePage() : AtlasPageHandle{});
    hop_.setAtlasUpdateQueue(&atlasUpdateQueue);
    rt_.prepare(sampleRate, AtlasConfig::FFT_SIZE, AtlasConfig::HOP_SIZE);
    
    // Set STFT latency based on single-source atlas configuration
    // This accounts for overlap-add reconstruction delay in STFT processing
    const int stftLatency = AtlasConfig::FFT_SIZE - AtlasConfig::HOP_SIZE;
    updateReportedLatency(stftLatency);
    jassert(getLatencySamples() == stftLatency);
    
    // Prepare with JUCE DSP spec
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<uint32_t>(samplesPerBlock);
    spec.numChannels = static_cast<uint32_t>(std::max(getTotalNumInputChannels(), getTotalNumOutputChannels()));
    
    spectralPaintProcessor->prepare(spec);
    
    // Initialize Phase 2-3 validation infrastructure  
    processedSampleCount_.store(0, std::memory_order_relaxed);
    // Store epoch in nanoseconds for unified timebase (per spec)
    epochSteadyNanos_.store(std::chrono::steady_clock::now().time_since_epoch().count(), 
                            std::memory_order_relaxed);
    latencyTracker_.reset();
    
    // Clear any existing data in queues
    spectralDataQueue.clear();
    parameterQueue.clear();
    maskColumnQueue.clear();
    sampleQueue.clear();
    
    // RT-SAFE: processBlock setup - no logging in RT path
    // Queue diagnostics moved to UI thread for HUD display
    
#ifdef PHASE4_EXPERIMENT
    // Initialize Phase 4 experimental components
    const int fftSize = 512;
    const int numBins = fftSize / 2 + 1;  // 257
    const int channels = getTotalNumOutputChannels();
    
    spectralStub.prepare(sampleRate, fftSize, numBins, channels);
    keyFilter.prepare(fftSize, numBins, sampleRate);
    
    // Initialize with default scale (C major)
    keyFilter.rebuildAsync(0, dsp::ScaleType::Major);
    
    // Set up parameter listeners for Phase 4
    
    // Initialize parameter atomics from APVTS values
    useTestFeeder_.store(apvts.getParameterAsValue(Params::ParameterIDs::useTestFeeder).getValue(), std::memory_order_relaxed);
    keyFilterEnabled_.store(apvts.getParameterAsValue(Params::ParameterIDs::keyFilterEnabled).getValue(), std::memory_order_relaxed);
    oscGain_.store(apvts.getParameterAsValue(Params::ParameterIDs::oscGain).getValue(), std::memory_order_relaxed);
    scaleType_.store(static_cast<int>(apvts.getParameterAsValue(Params::ParameterIDs::scaleType).getValue()), std::memory_order_relaxed);
    rootNote_.store(static_cast<int>(apvts.getParameterAsValue(Params::ParameterIDs::rootNote).getValue()), std::memory_order_relaxed);
    useModernPaint_.store(apvts.getParameterAsValue(Params::ParameterIDs::useModernPaint).getValue(), std::memory_order_relaxed);
    // Initialize dual-mode params
    mode_.store(static_cast<int>(apvts.getParameterAsValue(Params::ParameterIDs::mode).getValue()), std::memory_order_relaxed);
    blend_.store(apvts.getParameterAsValue(Params::ParameterIDs::blend).getValue(), std::memory_order_relaxed);
    // respeed parameter removed for live insert mode
    
    // Add parameter listeners for real-time updates
    apvts.addParameterListener(Params::ParameterIDs::useTestFeeder, this);
    apvts.addParameterListener(Params::ParameterIDs::keyFilterEnabled, this);
    apvts.addParameterListener(Params::ParameterIDs::oscGain, this);
    apvts.addParameterListener(Params::ParameterIDs::scaleType, this);
    apvts.addParameterListener(Params::ParameterIDs::rootNote, this);
    apvts.addParameterListener(Params::ParameterIDs::useModernPaint, this);
    apvts.addParameterListener(Params::ParameterIDs::mode, this);
    apvts.addParameterListener(Params::ParameterIDs::blend, this);
    // respeed parameter removed for live insert mode
#endif
    
    // Initialize RT-safe parameter smoothers (50ms ramp time for smooth automation)
    oscGainSmoother_.reset(sampleRate, smoothingTimeSec_);
    brushSizeSmoother_.reset(sampleRate, smoothingTimeSec_);
    brushStrengthSmoother_.reset(sampleRate, smoothingTimeSec_);
    
    // Set initial values from current APVTS parameters
    oscGainSmoother_.setCurrentAndTargetValue(getParamFast(Params::ParameterIDs::oscGain));
    brushSizeSmoother_.setCurrentAndTargetValue(getParamFast(Params::ParameterIDs::brushSize));
    brushStrengthSmoother_.setCurrentAndTargetValue(getParamFast(Params::ParameterIDs::brushStrength));
    
#ifdef PHASE4_EXPERIMENT
    // Pre-calculate timestamp for RT safety (Phase4 only)
    using namespace std::chrono;
    auto now = steady_clock::now();
    rtTimestampUs_.store(static_cast<uint64_t>(duration_cast<microseconds>(now.time_since_epoch()).count()), std::memory_order_relaxed);
#endif
    
    // Initialize live insert spectral processor (always available)
    spectralPlayer.prepareLive(sampleRate, samplesPerBlock, /*fftOrder*/11, /*hop*/0);
    spectralPlayer.reset();
    spectralReady = true;

    // Preallocate hybrid buffer for mix mode (avoid RT allocations)
    const int numOutChans = std::max(getTotalNumInputChannels(), getTotalNumOutputChannels());
    hybridBuffer_.setSize(numOutChans, samplesPerBlock);
    
    // Initialize RT-safe latency delay line for non-STFT paths (FFT_SIZE - HOP_SIZE)
    const int latencyDelaySamples = AtlasConfig::FFT_SIZE - AtlasConfig::HOP_SIZE; // 384 samples
    latencyLine_.setSize(numOutChans, latencyDelaySamples);
    latencyLine_.clear();
    latencyWritePos_ = 0;

    // ===== MVP: Preallocate UI spectrogram buffers (no RT allocs) =====
    {
        int order = 9; // 512
        while ((1 << order) < samplesPerBlock && order < 11) ++order; // up to 2048
        uiFftOrder_ = juce::jlimit(9, 11, order);
        uiFftSize_  = 1 << uiFftOrder_;
        uiNumBins_  = uiFftSize_ / 2 + 1;
        uiHop_      = juce::jmax(128, uiFftSize_ / 4);
        uiFft_.reset(new juce::dsp::FFT(uiFftOrder_));
        uiFftBuffer_.assign((size_t)uiFftSize_, 0.0f);
        uiWindow_.assign((size_t)uiFftSize_, 0.0f);
        for (int i = 0; i < uiFftSize_; ++i)
            uiWindow_[(size_t)i] = 0.5f - 0.5f * std::cos(2.0f * juce::MathConstants<float>::pi * (float)i / (float)(uiFftSize_ - 1));
        uiMag_.assign((size_t)uiNumBins_, 0.0f);
        uiWorkMag_.assign((size_t)uiNumBins_, 0.0f);
        uiSeq_.store(0, std::memory_order_release);
    }

    // Prepare post TubeStage
    tubeStage_.prepare(sampleRate);
}

void SpectralCanvasProAudioProcessor::releaseResources()
{
    DBG("[reactivate] releaseResources - keeping latency at " << getLatencySamples());
    if (spectralEngine)
        spectralEngine->reset();
        
    if (spectralPaintProcessor)
        spectralPaintProcessor->reset();
        
    sampleLoader.shutdown();
    maskTestFeeder.reset();
    
    // Shutdown tiled atlas system
    shutdownTiledAtlas();
    
    // Keep latency intact - host needs consistent latency reporting
    // Do not reset latency to 0 here (causes pluginval failures)

    tubeStage_.reset();
}

bool SpectralCanvasProAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in.isDisabled() || out.isDisabled())
        return false;
    if (in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo()))
        return true;
    return false;
}

void SpectralCanvasProAudioProcessor::generateImmediateAudioFeedback()
{
    // No-op placeholder to satisfy linker for existing declarations
}

void SpectralCanvasProAudioProcessor::suspendProcessing(bool shouldBeSuspended)
{
    DBG("[reactivate] suspendProcessing=" << (shouldBeSuspended ? "true" : "false"));
    
    if (shouldBeSuspended) {
        // Reset SPSC queues to prevent stale data on reactivation
        maskDeltaQueue.clear();
        atlasUpdateQueue.clear();
        maskColumnQueue.clear();
        sampleQueue.clear();
    } else {
        // Verify latency maintained after unsuspend
        const int currentLatency = getLatencySamples();
        const int expectedLatency = AtlasConfig::FFT_SIZE - AtlasConfig::HOP_SIZE; // 384
        if (currentLatency != expectedLatency) {
            DBG("[reactivate] WARNING: latency drift after unsuspend: " << currentLatency << " != " << expectedLatency);
        }
    }
}

// Called from parameter listener / message thread whenever GUI toggles mode
void SpectralCanvasProAudioProcessor::setAudioPathFromParams()
{
    // Two-mode router only: Silent (Bypass) or Phase4Synth/SpectralResynthesis (Process)
    const auto* p = apvts.getRawParameterValue(Params::ParameterIDs::processEnabled);
    const bool enabled = (p != nullptr) && (p->load() > 0.5f);
    // Route to Phase4Synth path for paint-to-audio functionality with gain smoothing
    currentPath_.store(enabled ? AudioPath::Phase4Synth : AudioPath::Silent, std::memory_order_release);
}

#ifdef PHASE4_EXPERIMENT
void SpectralCanvasProAudioProcessor::rtResetPhase4_() noexcept
{
    spectralStub.reset(); // zero phases/magnitudes; RT-safe
}

void SpectralCanvasProAudioProcessor::rtResetTestFeeder_() noexcept
{
    // Reset test feeder state if needed
    // Static phase will be reset naturally due to scope
}

void SpectralCanvasProAudioProcessor::rtResetModernPaint_() noexcept
{
    if (spectralPaintProcessor)
        spectralPaintProcessor->reset();
}

int SpectralCanvasProAudioProcessor::getActiveBinCount() const noexcept
{
    return spectralStub.getActiveBinCount();
}

int SpectralCanvasProAudioProcessor::getNumBins() const noexcept
{
    return spectralStub.getNumBins();
}
#endif

void SpectralCanvasProAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, 
                                                   juce::MidiBuffer& midiMessages) noexcept
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;
    
    const int numSamples = buffer.getNumSamples();
    
    // Early exit for zero-length buffers to prevent crashes
    if (numSamples <= 0 || buffer.getNumChannels() <= 0) {
        return;
    }
    
    // RT-safe perf counters: no clocks on audio thread
    lastBlockSize_.store(numSamples, std::memory_order_relaxed);
    totalBlocksProcessed_.fetch_add(1, std::memory_order_relaxed);
    totalSamplesProcessed_.fetch_add(static_cast<uint64_t>(numSamples), std::memory_order_relaxed);
    
    // RT-safe XRun detection: check for unusual buffer conditions
    if (numSamples == 0 || (numSamples < 16 && currentBlockSize > 64)) {
        // Abnormally small buffer size may indicate XRun condition
        xrunCount_.fetch_add(1, std::memory_order_relaxed);
    }

    // RT path transition handling
    const auto path = currentPath_.load(std::memory_order_acquire);
    if (path != lastPath_)
    {
#ifdef PHASE4_EXPERIMENT
        if (path == AudioPath::Phase4Synth)   rtResetPhase4_();
        else if (path == AudioPath::TestFeeder) rtResetTestFeeder_();
        else if (path == AudioPath::ModernPaint) rtResetModernPaint_();
#endif
        lastPath_ = path;
    }

    
    // Initialize write detection for this block
    wroteAudioFlag_.store(false, std::memory_order_relaxed);

#if PHASE4_DEBUG_TAP
    // Record queue address on audio thread for integrity checking
    debugTap_.queueAddrOnAudio.store(reinterpret_cast<uintptr_t>(&maskColumnQueue), std::memory_order_relaxed);
#endif

    // Update parameter smoothers with current target values (RT-safe)
    oscGainSmoother_.setTargetValue(oscGain_.load(std::memory_order_relaxed));
    brushSizeSmoother_.setTargetValue(brushSize_.load(std::memory_order_relaxed));
    brushStrengthSmoother_.setTargetValue(brushStrength_.load(std::memory_order_relaxed));
    
    // Use smoothed params on the audio thread
    const float oscGain = juce::jlimit(1.0e-6f, 1.0f, oscGainSmoother_.getNextValue());
    const float brushSize = brushSizeSmoother_.getNextValue();
    const float brushStrength = brushStrengthSmoother_.getNextValue();
    
    // These are prepared for future paint integration
    juce::ignoreUnused(brushSize, brushStrength);
    
    // Check for invalid channel count before processing
    if (buffer.getNumChannels() <= 0) {
        publishCanvasSnapshot(); // Publish snapshot even on early exit
        return; // Early exit for invalid channel count
    }
    

    switch (path)
    {
        case AudioPath::Silent:
            // Convert any pending mask columns to deltas but don't apply them
            // This prevents queue buildup without processing through spectralStub
            {
                const uint64_t currentSamplePos = totalSamplesProcessed_.load(std::memory_order_acquire);
                convertMaskColumnsToDeltas(currentSamplePos);
            }
            // Publish spectrogram from input for UI even in bypass
            if (!uiFftBuffer_.empty() && !uiWindow_.empty() && uiFft_ && buffer.getNumChannels() > 0)
            {
                const float* in = buffer.getReadPointer(0);
                const int copyCount = juce::jmin(uiFftSize_, buffer.getNumSamples());
                std::fill(uiFftBuffer_.begin(), uiFftBuffer_.end(), 0.0f);
                std::memcpy(uiFftBuffer_.data(), in, (size_t)copyCount * sizeof(float));
                juce::FloatVectorOperations::multiply(uiFftBuffer_.data(), uiWindow_.data(), uiFftSize_);
                uiFft_->performRealOnlyForwardTransform(uiFftBuffer_.data(), true);
                for (int bin = 0; bin < uiNumBins_; ++bin)
                {
                    float re, im;
                    if (bin == 0)                { re = uiFftBuffer_[0]; im = 0.0f; }
                    else if (bin == uiNumBins_-1){ re = uiFftBuffer_[1]; im = 0.0f; }
                    else                         { re = uiFftBuffer_[bin*2]; im = uiFftBuffer_[bin*2 + 1]; }
                    uiWorkMag_[(size_t)bin] = std::sqrt(re*re + im*im) + 1.0e-12f;
                }
                publishUISpectrum(uiWorkMag_.data(), uiNumBins_);
            }
            // Nothing is rendered; output remains cleared.
            publishCanvasSnapshot(); // Publish snapshot for debug overlay
            return;

        case AudioPath::TestFeeder:
        {
#ifdef PHASE4_EXPERIMENT
            // Don't drain queue - preserve paint data for potential Phase4 switch
            // TestFeeder generates its own audio independently
#endif
            // *** Render ONLY the test feeder here ***
            static float testPhase = 0.0f;
            const float freq = 440.0f;
            const float incr = juce::MathConstants<float>::twoPi * freq / static_cast<float>(getSampleRate());
            
            for (int n = 0; n < numSamples; ++n)
            {
                testPhase += incr;
                if (testPhase >= juce::MathConstants<float>::twoPi) 
                    testPhase -= juce::MathConstants<float>::twoPi;
                
                const float sample = 0.2f * std::sin(testPhase);
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    buffer.getWritePointer(ch)[n] = sample;
            }
            
            // TestFeeder always writes (direct tone)
            wroteAudioFlag_.store(true, std::memory_order_relaxed);
            applyLatencyDelayIfNeeded(buffer); // align with reported latency
            publishCanvasSnapshot(); // Publish snapshot for debug overlay
            return;
        }

#ifdef PHASE4_EXPERIMENT
        case AudioPath::Phase4Synth:
        {
            // Drain paint queue to prevent buildup (Phase 4 foundation)
            const uint64_t currentSamplePos = totalSamplesProcessed_.load(std::memory_order_acquire);
            convertMaskColumnsToDeltas(currentSamplePos);
            
            // ===== MVP: Apply paint scalar gain with smoothing =====
            {
                const float sr = (float) getSampleRate();
                const float tauMs = 10.0f;
                const float blockSec = (float)buffer.getNumSamples() / juce::jmax(1.0f, sr);
                const float alpha = 1.0f - std::exp(-blockSec / (tauMs * 0.001f));
                const float target = paintGainTarget_.load(std::memory_order_acquire);
                smoothedPaintGain_ += alpha * (target - smoothedPaintGain_);
                const int nCh = buffer.getNumChannels();
                const int nSmps = buffer.getNumSamples();
                for (int c = 0; c < nCh; ++c)
                {
                    float* x = buffer.getWritePointer(c);
                    for (int n = 0; n < nSmps; ++n) x[n] *= smoothedPaintGain_;
                }
            }

            // Publish UI spectrum from processed audio
            if (!uiFftBuffer_.empty() && !uiWindow_.empty() && uiFft_ && buffer.getNumChannels() > 0)
            {
                const float* in = buffer.getReadPointer(0);
                const int copyCount = juce::jmin(uiFftSize_, buffer.getNumSamples());
                std::fill(uiFftBuffer_.begin(), uiFftBuffer_.end(), 0.0f);
                std::memcpy(uiFftBuffer_.data(), in, (size_t)copyCount * sizeof(float));
                juce::FloatVectorOperations::multiply(uiFftBuffer_.data(), uiWindow_.data(), uiFftSize_);
                uiFft_->performRealOnlyForwardTransform(uiFftBuffer_.data(), true);
                for (int bin = 0; bin < uiNumBins_; ++bin)
                {
                    float re, im;
                    if (bin == 0)                { re = uiFftBuffer_[0]; im = 0.0f; }
                    else if (bin == uiNumBins_-1){ re = uiFftBuffer_[1]; im = 0.0f; }
                    else                         { re = uiFftBuffer_[bin*2]; im = uiFftBuffer_[bin*2 + 1]; }
                    uiWorkMag_[(size_t)bin] = std::sqrt(re*re + im*im) + 1.0e-12f;
                }
                publishUISpectrum(uiWorkMag_.data(), uiNumBins_);
            }

            break;
        }
#endif
        
        case AudioPath::SpectralResynthesis:
        {
            // TEMP: Simple passthrough while fixing XRuns 
            // TODO: Re-enable spectralPlayer.process() with optimizations
            
            // Simple paint effect: apply smoothed gain based on paint activity
            const float paintGain = smoothedPaintGain_; // already smoothed in Phase4Synth
            if (paintGain < 0.99f) {
                // Apply gentle volume reduction when painting
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
                    buffer.applyGain(ch, 0, numSamples, paintGain);
                }
            }
            
            // Mark audio as processed
            const float rms = buffer.getNumChannels() > 0
                ? buffer.getRMSLevel(0, 0, numSamples) : 0.0f;
            if (rms > 1.0e-6f) {
                wroteAudioFlag_.store(true, std::memory_order_relaxed);
            }
            
            publishCanvasSnapshot(); // Publish snapshot for debug overlay
            return;
        }

    }

    // Tube post (safe RT). Map 0..10 → 0..1 drive.
    {
        const float drive01 = juce::jlimit(0.0f, 1.0f, saturationDrive_.load(std::memory_order_relaxed) * 0.1f);
        const float mix     = juce::jlimit(0.0f, 1.0f, saturationMix_.load(std::memory_order_relaxed));
        if (mix > 0.0f && drive01 > 0.0f)
        {
            const int nCh = buffer.getNumChannels();
            const int n   = buffer.getNumSamples();
            for (int ch = 0; ch < nCh; ++ch)
            {
                float* d = buffer.getWritePointer(ch);
                for (int i = 0; i < n; ++i)
                {
                    const float s   = d[i];
                    const float sat = tubeStage_.processSample(s, drive01);
                    d[i] = s + (sat - s) * mix;
                }
            }
        }
    }

    // Publish canvas snapshot for debug overlay (RT-safe)
    publishCanvasSnapshot();
}

// RT-safe fallback beep generator (220Hz A3 tone)
void SpectralCanvasProAudioProcessor::fallbackBeep(juce::AudioBuffer<float>& buffer) noexcept
{
    static float phase = 0.0f;
    const float phaseIncrement = 220.0f * juce::MathConstants<float>::twoPi / static_cast<float>(getSampleRate());
    const float amplitude = 0.05f; // Balanced volume
    const int numSamples = buffer.getNumSamples();
    
    for (int n = 0; n < numSamples; ++n) {
        const float sample = std::sin(phase) * amplitude;
        
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.getWritePointer(ch)[n] += sample;
            
        phase += phaseIncrement;
        if (phase >= juce::MathConstants<float>::twoPi)
            phase -= juce::MathConstants<float>::twoPi;
    }
}

// Compatibility wrapper for legacy interface
void SpectralCanvasProAudioProcessor::generateFallbackBeep(juce::AudioBuffer<float>& buffer, int numSamples) noexcept
{
    juce::ignoreUnused(numSamples); // buffer.getNumSamples() is authoritative
    fallbackBeep(buffer);
}

#ifdef PHASE4_EXPERIMENT
void SpectralCanvasProAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == Params::ParameterIDs::saturationDrive)
    {
        saturationDrive_.store(juce::jlimit(0.0f, 10.0f, newValue), std::memory_order_relaxed);
        return;
    }
    else if (parameterID == Params::ParameterIDs::saturationMix)
    {
        saturationMix_.store(juce::jlimit(0.0f, 1.0f, newValue), std::memory_order_relaxed);
        return;
    }
    if (parameterID == Params::ParameterIDs::useTestFeeder)
    {
        useTestFeeder_.store(newValue > 0.5f, std::memory_order_relaxed);
        setAudioPathFromParams(); // Update path atomically
    }
    else if (parameterID == Params::ParameterIDs::useModernPaint)
    {
        useModernPaint_.store(newValue > 0.5f, std::memory_order_relaxed);
        setAudioPathFromParams(); // Update path atomically
    }
    else if (parameterID == Params::ParameterIDs::keyFilterEnabled)
    {
        keyFilterEnabled_.store(newValue > 0.5f, std::memory_order_relaxed);
    }
    else if (parameterID == Params::ParameterIDs::oscGain)
    {
        oscGain_.store(newValue, std::memory_order_relaxed);
        // Smoother target will be set in processBlock - RT-safe
    }
    else if (parameterID == Params::ParameterIDs::brushSize)
    {
        brushSize_.store(newValue, std::memory_order_relaxed);
        // Smoother target will be set in processBlock - RT-safe
    }
    else if (parameterID == Params::ParameterIDs::brushStrength)
    {
        brushStrength_.store(newValue, std::memory_order_relaxed);
        // Smoother target will be set in processBlock - RT-safe
    }
    else if (parameterID == Params::ParameterIDs::maskingStrictness)
    {
        spectralPlayer.setMaskingStrictness(newValue);
    }
    else if (parameterID == Params::ParameterIDs::jndMarginDB)
    {
        spectralPlayer.setJndMarginDB(newValue);
    }
    // Z-plane parameters temporarily disabled
    /*
    else if (parameterID == Params::ParameterIDs::zplaneModelA || parameterID == Params::ParameterIDs::zplaneModelB)
    {
        // Main plugin does not use STFTProcessor; values are retained internally (no-ops here)
        juce::ignoreUnused(newValue);
    }
    else if (parameterID == Params::ParameterIDs::zplaneMorph)
    {
        // Main plugin no-op; stage is always-on and internal
        juce::ignoreUnused(newValue);
    }
    else if (parameterID == Params::ParameterIDs::zplaneQuality)
    {
        // Main plugin no-op; stage is always-on and internal
        juce::ignoreUnused(newValue);
    }
    */
    else if (parameterID == Params::ParameterIDs::scaleType)
    {
        const int scaleType = static_cast<int>(newValue);
        scaleType_.store(scaleType, std::memory_order_relaxed);
        
        // Queue rebuild request for audio thread processing
        const int root = rootNote_.load(std::memory_order_relaxed);
        KeyFilterRebuildRequest request{root, static_cast<dsp::ScaleType>(scaleType)};
        keyFilterRebuildQueue.push(request); // Non-blocking
    }
    else if (parameterID == Params::ParameterIDs::rootNote)
    {
        const int rootNote = static_cast<int>(newValue);
        rootNote_.store(rootNote, std::memory_order_relaxed);
        
        // Queue rebuild request for audio thread processing
        const int scaleType = scaleType_.load(std::memory_order_relaxed);
        KeyFilterRebuildRequest request{rootNote, static_cast<dsp::ScaleType>(scaleType)};
        keyFilterRebuildQueue.push(request); // Non-blocking
    }
    else if (parameterID == Params::ParameterIDs::mode)
    {
        mode_.store(static_cast<int>(newValue), std::memory_order_relaxed);
        setAudioPathFromParams();
    }
    else if (parameterID == Params::ParameterIDs::processEnabled)
    {
        setAudioPathFromParams();
    }
    else if (parameterID == Params::ParameterIDs::blend)
    {
        blend_.store(newValue, std::memory_order_relaxed);
    }
    // respeed parameter removed for live insert mode
}
#else
void SpectralCanvasProAudioProcessor::parameterChanged(const juce::String&, float)
{
    // No-op when Phase 4 is disabled
}
#endif

#ifdef PHASE4_EXPERIMENT
uint64_t SpectralCanvasProAudioProcessor::getCurrentTimeUs() noexcept
{
    // Simple fallback - use basic counter for now
    // This avoids RT violations while maintaining functionality
    static std::atomic<uint64_t> counter{0};
    return counter.fetch_add(1, std::memory_order_relaxed);
}
#else
uint64_t SpectralCanvasProAudioProcessor::getCurrentTimeUs() noexcept
{
    // Fallback when Phase4 is disabled - return 0 
    return 0;
}
#endif

juce::AudioProcessorEditor* SpectralCanvasProAudioProcessor::createEditor()
{
    return new SpectralCanvasProEditor(*this);
}

void SpectralCanvasProAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SpectralCanvasProAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    
    if (xmlState.get() != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

bool SpectralCanvasProAudioProcessor::pushMaskColumn(const MaskColumn& mask)
{
    // RT-SAFE: Track push attempts with atomic counter
    pushMaskAttempts_.fetch_add(1, std::memory_order_relaxed);
    
    // Allow paint events for Phase4Synth path (guard removed for paint-to-audio connection)
    
    
#if PHASE4_DEBUG_TAP
    // Create enhanced column with debug sequence
    static std::atomic<uint64_t> gSeq{1};
    MaskColumnEx colEx;
    
    // Copy base MaskColumn data
    colEx.frameIndex = mask.frameIndex;
    colEx.numBins = mask.numBins;
    colEx.timestampSamples = mask.timestampSamples;
    colEx.uiTimestampMicros = mask.uiTimestampMicros;
    colEx.sequenceNumber = mask.sequenceNumber;
    // Enforce NUM_BINS-sized copy at compile time
    atlas::copyColumn(colEx.values, mask.values);
    
    // Add debug sequence
    colEx.debugSeq = gSeq.fetch_add(1, std::memory_order_relaxed);
    
    // Attempt to push enhanced column to queue
    const bool success = maskColumnQueue.push(colEx);
    
    // Update debug tap
    debugTap_.queueAddrOnPush.store(reinterpret_cast<uintptr_t>(&maskColumnQueue), std::memory_order_relaxed);
    debugTap_.lastSeqPushed.store(colEx.debugSeq, std::memory_order_relaxed);
    
    if (success) {
        debugTap_.pushes.fetch_add(1, std::memory_order_relaxed);
        maskPushCount_.fetch_add(1, std::memory_order_relaxed);
    } else {
        debugTap_.pushFails.fetch_add(1, std::memory_order_relaxed);
        maskDropCount_.fetch_add(1, std::memory_order_relaxed);
    }
#else
    // Simple path without debug tap
    const bool success = maskColumnQueue.push(mask);
    
    if (success) {
        maskPushCount_.fetch_add(1, std::memory_order_relaxed);
    } else {
        maskDropCount_.fetch_add(1, std::memory_order_relaxed);
    }
#endif
    
    return success;
}

// Phase 2-3 getter methods for debug overlay
int SpectralCanvasProAudioProcessor::getBlockSize() const
{
    return currentBlockSize;
}

double SpectralCanvasProAudioProcessor::getSampleRate() const
{
    return currentSampleRate;
}

SpectralCanvasProAudioProcessor::PerformanceMetrics SpectralCanvasProAudioProcessor::getPerformanceMetrics() const
{
    PerformanceMetrics metrics;
    
    // Latency statistics from RT-safe tracker
    metrics.medianLatencyMs = latencyTracker_.getMedianLatencyMs();
    metrics.p95LatencyMs = latencyTracker_.getP95LatencyMs();
    
    // Queue statistics
    metrics.queueDepth = maskColumnQueue.getApproxQueueDepth();
    metrics.dropCount = maskColumnQueue.getDropCount();
    
    // Audio processing stats
    metrics.processedSamples = processedSampleCount_.load(std::memory_order_relaxed);
    
    // XRun detection from RT-safe buffer monitoring
    metrics.xrunCount = xrunCount_.load(std::memory_order_relaxed);
    
    return metrics;
}

void SpectralCanvasProAudioProcessor::updateReportedLatency(int samples) noexcept
{
    latencySamples_.store(samples, std::memory_order_release);
    juce::AudioProcessor::setLatencySamples(samples); // <- ensure wrapper sees it
    DBG("updateReportedLatency(" << samples << ")");
}


bool SpectralCanvasProAudioProcessor::getCanvasSnapshot(CanvasSnapshot& out) const
{
    return snapshotBus_.tryLoad(out);
}

void SpectralCanvasProAudioProcessor::publishCanvasSnapshot() const
{
    CanvasSnapshot snapshot;
    
    // Populate with current processor state
    snapshot.timestampMs = juce::Time::getMillisecondCounterHiRes();
    snapshot.metrics = getPerformanceMetrics();
    snapshot.currentPath = getCurrentPath();
    snapshot.wroteAudioFlag = getWroteAudioFlag();
    snapshot.sampleRate = getSampleRate();
    snapshot.blockSize = getBlockSize();
    
    #ifdef PHASE4_EXPERIMENT
    snapshot.activeBins = getActiveBinCount();
    snapshot.totalBins = getNumBins();
    snapshot.maskPushCount = getMaskPushCount();
    snapshot.maskDropCount = getMaskDropCount();
    snapshot.maxMagnitude = getMaxMagnitude();
    snapshot.phase4Blocks = getPhase4Blocks();
    #endif
    
    // Publish via double-buffered bus - no shared_ptr, no heap allocation
    snapshotBus_.publish(snapshot);
}

bool SpectralCanvasProAudioProcessor::getUISpectrum(const float*& ptr, int& numBins, uint32_t& seq) const noexcept
{
    seq     = uiSeq_.load(std::memory_order_acquire);
    ptr     = uiMag_.empty() ? nullptr : uiMag_.data();
    numBins = (int) uiMag_.size();
    return ptr != nullptr && numBins > 0;
}

bool SpectralCanvasProAudioProcessor::pushPaintEvent(float y, float intensity, uint32_t timestampMs) noexcept
{
    // UPDATE PAINT GAIN TARGET FOR IMMEDIATE AUDIO FEEDBACK
    // When painting (intensity > 0), reduce gain to 0.3, when not painting return to 1.0
    const float targetGain = (intensity > 0.01f) ? 0.3f : 1.0f;
    paintGainTarget_.store(targetGain, std::memory_order_release);
    
    // Generate timestamp if not provided
    double timestamp = (timestampMs == 0) ? juce::Time::getMillisecondCounterHiRes() : static_cast<double>(timestampMs);
    
    // 1) Continue publishing ModernPaint event for legacy/experimental paths
    {
        scp::PaintEvent ev;
        ev.pos = juce::Point<float>(0.5f, y); // x=0.5 (center), y from param
        ev.colour = juce::Colour::fromHSV(y, 0.8f, 0.9f, 1.0f);
        ev.pressure = intensity;
        ev.strokeStart = (intensity > 0.0f);
        ev.strokeEnd = (intensity == 0.0f);
        ev.timestampMs = timestamp;
        paintEventQueue.push(ev); // best-effort
    }

    // 2) Map paint event → SpectralPlayer brush command for live insert path
    {
        // Convert Y (0=top) to bin index (top=high freq)
        const int K = spectralPlayer.numBins();
        if (K > 0)
        {
            const float yClamped = juce::jlimit(0.0f, 1.0f, y);
            const int centerBin = juce::jlimit(0, K - 1, (int) std::round((1.0f - yClamped) * (float)(K - 1)));
            // Half-octave radius mapping: radius ≈ 0.353553390593 × centerBin
            const float halfOctaveRadiusF = 0.353553390593f * (float) centerBin;
            const int radiusBins = juce::jlimit(1, K / 2, (int) std::lround(halfOctaveRadiusF));
            const float strength = juce::jlimit(0.0f, 1.0f, brushStrength_.load(std::memory_order_relaxed));
            // Map intensity (0..1) × strength to attenuation gain in [0..1]
            const float attenuation = juce::jlimit(0.0f, 1.0f, intensity * strength);
            const float gain = juce::jlimit(0.0f, 2.0f, 1.0f - attenuation);
            BrushCommand cmd;
            cmd.binCenter = (BinIndex) centerBin;
            cmd.radiusBins = radiusBins;
            cmd.gain = gain;
            cmd.hardness = 0.6f;
            spectralPlayer.pushBrushCommand(cmd);
        }
    }

    return true;
}

// Tiled atlas system implementation methods

void SpectralCanvasProAudioProcessor::initializeTiledAtlas() noexcept {
    // Initialize tiled atlas
    tiledAtlas_ = std::make_shared<TiledAtlas>();
    if (!tiledAtlas_->initialize()) {
        tiledAtlas_.reset();
        return;
    }
    
    // Initialize offline analyzer
    offlineAnalyzer_ = std::make_unique<OfflineStftAnalyzer>();
    if (!offlineAnalyzer_->initialize(currentSampleRate, maskDeltaQueue)) {
        offlineAnalyzer_.reset();
        return;
    }
    
    // Phase 4 hop scheduler is initialized in prepareToPlay()
}

void SpectralCanvasProAudioProcessor::shutdownTiledAtlas() noexcept {
    if (offlineAnalyzer_) {
        offlineAnalyzer_->shutdown();
        offlineAnalyzer_.reset();
    }
    
    // Phase 4 hop scheduler is managed directly as hop_ member
    
    if (tiledAtlas_) {
        tiledAtlas_->shutdown();
        tiledAtlas_.reset();
    }
}

void SpectralCanvasProAudioProcessor::processTiledAtlasMessages() noexcept {
    // Phase 4: Using hop_ member instead of hopScheduler_
    
    // Get current audio position
    const uint64_t currentSamplePos = totalSamplesProcessed_.load(std::memory_order_acquire);
    
    // Convert MaskColumn → MaskColumnDelta and feed to HopScheduler
    convertMaskColumnsToDeltas(currentSamplePos);
    
    // Phase 4 mask delta processing is handled directly in processBlock()
    
    // Process any atlas page management messages
    AtlasPageMessage pageMsg;
    while (pageManagementQueue.pop(pageMsg)) {
        if (!tiledAtlas_) continue;
        
        switch (pageMsg.type) {
            case AtlasPageMessage::Activate:
                if (pageMsg.handle.isValid()) {
                    tiledAtlas_->activatePage(pageMsg.handle);
                }
                break;
                
            case AtlasPageMessage::Release:
                if (pageMsg.handle.isValid()) {
                    tiledAtlas_->releasePage(pageMsg.handle);
                }
                break;
                
            default:
                break; // Other message types handled by background thread
        }
    }
}

void SpectralCanvasProAudioProcessor::convertMaskColumnsToDeltas(uint64_t currentSamplePos) noexcept {
    MaskColumn maskCol;
    static constexpr size_t MAX_CONVERSIONS_PER_BLOCK = 16; // RT-safety: bounded processing
    size_t conversionsThisBlock = 0;
    
    // Drain painting system output with bounded processing
    while (maskColumnQueue.pop(maskCol) && 
           maskDeltaQueue.hasSpaceAvailable() && 
           conversionsThisBlock < MAX_CONVERSIONS_PER_BLOCK) {
        
        // CRITICAL: Runtime bounds checking to prevent heap corruption
        if (maskCol.numBins != AtlasConfig::NUM_BINS) {
            badBinSkips_.fetch_add(1, std::memory_order_relaxed);
            jassertfalse; // Debug: catch now rather than crash later
            continue;     // Drop the malformed delta safely
        }
        
        // Create MaskColumnDelta
        MaskColumnDelta delta;
        
        // Map UI column to atlas position
        // FINAL SPEC: column = floor((playheadSamples - startOffsetSamples) / HOP)
        // For now, use maskCol.x directly as column index
        const uint32_t column = static_cast<uint32_t>(maskCol.x);
        const uint32_t tileId = column / AtlasConfig::TILE_WIDTH;
        const uint32_t colInTile = column % AtlasConfig::TILE_WIDTH;
        
        // CRITICAL: Additional bounds checking for column position  
        if (colInTile >= AtlasConfig::TILE_WIDTH) {
            badColSkips_.fetch_add(1, std::memory_order_relaxed);
            continue; // Skip out-of-bounds column to prevent corruption
        }
        
        // Set atlas position
        delta.position.tileX = static_cast<uint16_t>(tileId);
        delta.position.tileY = 0; // Single row for now
        delta.position.columnInTile = static_cast<uint16_t>(colInTile);
        delta.position.binStart = static_cast<uint16_t>(maskCol.binStart);
        
        // Copy mask values with runtime size validation
        static_assert(sizeof(delta.values) == sizeof(maskCol.values), 
                     "MaskColumn and MaskColumnDelta must have same values array size");
        
        for (size_t i = 0; i < AtlasConfig::NUM_BINS; ++i) {
            const float scaledValue = maskCol.values[i] * maskCol.intensity;
            delta.values[i] = scaledValue;
            
            // Debug-only finite value check to catch NaN/infinity corruption
            #if JUCE_DEBUG
            jassertquiet(std::isfinite(scaledValue));
            #endif
        }
        
        // Set timing metadata
        delta.metadata.samplePosition = maskCol.timestampSamples > 0 ? 
                                       maskCol.timestampSamples : currentSamplePos;
        delta.metadata.timeSeconds = static_cast<float>(delta.metadata.samplePosition / currentSampleRate);
        delta.metadata.sequenceNumber = maskCol.sequenceNumber;
        delta.metadata.fundamentalHz = 0.0f; // Will be filled by analysis later
        delta.metadata.spectralCentroid = 0.0f; // Will be filled by analysis later
        
        // Push to HopScheduler input queue (RT-safe, non-blocking)
        if (maskDeltaQueue.push(delta)) {
            conversionsThisBlock++;
        } else {
            // Queue full - could increment drop counter
            break;
        }
    }
    
    // ===== MVP: Derive scalar paint gain target from last processed column =====
    // Average valid bins [binStart, binEnd) or all bins if unspecified. Clamp [0..2].
    // Note: This uses the last maskCol seen in the bounded loop above.
    {
        const int totalBins = (int)AtlasConfig::NUM_BINS;
        int b0 = 0, b1 = totalBins;
        if (maskCol.binEnd > 0 || maskCol.binStart > 0)
        {
            b0 = juce::jlimit(0, totalBins, (int)maskCol.binStart);
            b1 = juce::jlimit(0, totalBins, (int)(maskCol.binEnd > 0 ? maskCol.binEnd : totalBins));
        }
        float sum = 0.0f; int count = 0;
        for (int b = b0; b < b1; ++b) { sum += maskCol.values[b]; ++count; }
        if (count > 0)
        {
            const float avg = sum / (float)count;
            const float target = juce::jlimit(0.0f, 2.0f, avg);
            paintGainTarget_.store(target, std::memory_order_release);
        }
    }
}

// RT-safe latency delay application for non-STFT paths
void SpectralCanvasProAudioProcessor::applyLatencyDelayIfNeeded(juce::AudioBuffer<float>& buffer) noexcept
{
    // Only apply delay if we have a configured latency line
    if (latencyLine_.getNumSamples() == 0 || latencyLine_.getNumChannels() == 0) {
        return; // No delay line configured
    }
    
    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(buffer.getNumChannels(), latencyLine_.getNumChannels());
    const int delaySize = latencyLine_.getNumSamples(); // Should be 384 samples
    
    // Process each channel independently
    for (int ch = 0; ch < numChannels; ++ch) {
        const float* input = buffer.getReadPointer(ch);
        float* output = buffer.getWritePointer(ch);
        float* delayBuffer = latencyLine_.getWritePointer(ch);
        
        // Apply circular buffer delay (RT-safe, no allocations)
        int writePos = latencyWritePos_;
        for (int n = 0; n < numSamples; ++n) {
            // Read delayed sample from circular buffer
            const float delayedSample = delayBuffer[writePos];
            
            // Write current input to delay buffer
            delayBuffer[writePos] = input[n];
            
            // Output the delayed sample
            output[n] = std::isfinite(delayedSample) ? delayedSample : 0.0f;
            
            // Advance write position (circular)
            writePos = (writePos + 1) % delaySize;
        }
    }
    
    // Update shared write position after processing all channels
    latencyWritePos_ = (latencyWritePos_ + numSamples) % delaySize;
    
    // Clear any extra channels that don't have delay processing
    for (int ch = numChannels; ch < buffer.getNumChannels(); ++ch) {
        buffer.clear(ch, 0, numSamples);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpectralCanvasProAudioProcessor();
}
