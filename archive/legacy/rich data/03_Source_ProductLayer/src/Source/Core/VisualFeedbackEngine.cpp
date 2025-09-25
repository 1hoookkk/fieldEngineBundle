#include "VisualFeedbackEngine.h"
#include <cmath>

//==============================================================================
VisualFeedbackEngine::VisualFeedbackEngine()
{
    // Initialize default color theme
    setColorScheme(ColorScheme::Modern);
    
    // Initialize frequency bands for linear drumming visualization
    auto& drumBands = frequencyVisualization.drumFrequencyBands;
    
    // Setup frequency bands matching LinearTrackerEngine
    drumBands[0] = {20.0f, 80.0f, 0.0f, 0.0f, juce::Colours::red, "Kick"};
    drumBands[1] = {40.0f, 80.0f, 0.0f, 0.0f, juce::Colours::darkred, "Tom 3"};
    drumBands[2] = {60.0f, 100.0f, 0.0f, 0.0f, juce::Colours::orange, "Tom 2"};
    drumBands[3] = {80.0f, 120.0f, 0.0f, 0.0f, juce::Colours::darkorange, "Tom 1"};
    drumBands[4] = {150.0f, 250.0f, 0.0f, 0.0f, juce::Colours::yellow, "Snare"};
    drumBands[5] = {1000.0f, 3000.0f, 0.0f, 0.0f, juce::Colours::pink, "Clap"};
    drumBands[6] = {2000.0f, 5000.0f, 0.0f, 0.0f, juce::Colours::hotpink, "Rim"};
    drumBands[7] = {3000.0f, 8000.0f, 0.0f, 0.0f, juce::Colours::lightblue, "Crash"};
    drumBands[8] = {4000.0f, 10000.0f, 0.0f, 0.0f, juce::Colours::cyan, "Ride"};
    drumBands[9] = {6000.0f, 12000.0f, 0.0f, 0.0f, juce::Colours::lightyellow, "Open Hat"};
    drumBands[10] = {8000.0f, 15000.0f, 0.0f, 0.0f, juce::Colours::white, "Closed Hat"};
    drumBands[11] = {10000.0f, 16000.0f, 0.0f, 0.0f, juce::Colours::lightgrey, "Shaker"};
    
    // Initialize tracker colors
    auto& trackColors = trackerVisualization.trackColors;
    for (int i = 0; i < TrackerVisualization::MAX_TRACKS; ++i)
    {
        if (i < 12)
        {
            trackColors[i] = drumBands[i].bandColor;
        }
        else
        {
            trackColors[i] = juce::Colour::fromHSV(i * 0.07f, 0.8f, 0.9f, 1.0f);
        }
    }
    
    lastFrameTime = juce::Time::getCurrentTime();
}

VisualFeedbackEngine::~VisualFeedbackEngine()
{
    shutdown();
}

//==============================================================================
// Core Visual System

void VisualFeedbackEngine::initialize(juce::OpenGLContext* glContext)
{
    if (glContext != nullptr)
    {
        useOpenGL = true;
        // TODO: Initialize OpenGL renderer when implemented
    }
    else
    {
        useOpenGL = false;
        // TODO: Initialize software renderer when implemented
    }
    
    applyQualitySettings();
}

void VisualFeedbackEngine::shutdown()
{
    activePaintTrails.clear();
    activeParticles.clear();
    // TODO: Reset renderer pointers when implemented
    // openGLRenderer.reset();
    // softwareRenderer.reset();
}

void VisualFeedbackEngine::renderFrame(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Update timing
    auto currentFrameTime = juce::Time::getCurrentTime();
    deltaTime = static_cast<float>((currentFrameTime - lastFrameTime).inMilliseconds()) / 1000.0f;
    deltaTime = juce::jlimit(0.001f, 0.1f, deltaTime); // Clamp for stability
    lastFrameTime = currentFrameTime;
    currentTime += deltaTime;
    
    // Update animations and effects
    updateAnimation(deltaTime);
    updateEffects(deltaTime);
    
    // Apply screen shake if active
    auto transform = juce::AffineTransform::identity;
    if (screenShakeEnabled.load() && screenShake.isActive)
    {
        auto offset = screenShake.getCurrentOffset();
        transform = transform.translated(offset.x, offset.y);
    }
    
    // Clear background
    g.setColour(currentColorTheme.backgroundColor);
    g.fillRect(bounds);
    
    // Apply transform for screen effects
    juce::Graphics::ScopedSaveState saveState(g);
    g.addTransform(transform);
    
    // Render main visualization based on mode
    switch (getVisualizationMode())
    {
        case VisualizationMode::Spectrum2D:
            renderSpectrum2D(g, bounds);
            break;
        case VisualizationMode::Spectrum3D:
            renderSpectrum3D(g, bounds);
            break;
        case VisualizationMode::Waveform3D:
            renderWaveform3D(g, bounds);
            break;
        case VisualizationMode::FrequencyBars:
            renderFrequencyBars(g, bounds);
            break;
        case VisualizationMode::SpectrumSphere:
            renderSpectrumSphere(g, bounds);
            break;
        case VisualizationMode::ParticleField:
            renderParticleField(g, bounds);
            break;
        default:
            renderSpectrum2D(g, bounds);
            break;
    }
    
    // Render paint trails
    renderPaintTrails(g);
    
    // Render particles
    renderParticles(g);
    
    // Render tracker pattern if visible
    if (getQualityLevel() >= QualityLevel::Balanced)
    {
        auto trackerBounds = bounds.removeFromBottom(200);
        trackerVisualization.renderPattern(g, trackerBounds);
    }
    
    // Render screen effects (flash, chromatic aberration, etc.)
    renderScreenEffects(g, bounds);
    
    // Update performance metrics
    updatePerformanceMetrics();
}

//==============================================================================
// Audio Data Updates

void VisualFeedbackEngine::updateAudioData(const juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumSamples() == 0) return;
    
    // Update frequency visualization with basic spectrum analysis
    const int fftSize = 512;
    std::vector<float> spectrum(fftSize / 2);
    
    // Simple magnitude calculation (in real implementation would use proper FFT)
    const float* channelData = buffer.getReadPointer(0);
    const int numSamples = juce::jmin(buffer.getNumSamples(), fftSize);
    
    for (int i = 0; i < spectrum.size(); ++i)
    {
        float magnitude = 0.0f;
        if (i < numSamples)
        {
            magnitude = std::abs(channelData[i]);
        }
        spectrum[i] = magnitude;
    }
    
    updateSpectrumData(spectrum);
    
    // Create particles based on audio energy
    if (particleEffectsEnabled.load() && getQualityLevel() >= QualityLevel::Balanced)
    {
        float energy = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            energy += std::abs(channelData[i]);
        }
        energy /= numSamples;
        
        if (energy > 0.1f) // Threshold for particle creation
        {
            createParticleSystem({400.0f, 300.0f}, currentColorTheme.accentColor, 
                               static_cast<int>(energy * 50));
        }
    }
}

void VisualFeedbackEngine::updateSpectrumData(const std::vector<float>& magnitudeSpectrum)
{
    frequencyVisualization.updateFromSpectrum(magnitudeSpectrum, 44100.0f);
}

void VisualFeedbackEngine::updateTrackerData(const std::vector<std::vector<int>>& patternData)
{
    trackerVisualization.updateFromPattern(patternData);
}

//==============================================================================
// Paint Stroke Visualization

void VisualFeedbackEngine::addPaintStroke(const juce::Path& path, juce::Colour color, float intensity)
{
    PaintTrail trail;
    trail.strokePath = path;
    trail.color = color;
    trail.intensity = intensity;
    trail.age = 0.0f;
    trail.maxAge = paintTrailLength.load();
    trail.glowRadius = intensity * 10.0f;
    trail.strokeWidth = intensity * 5.0f;
    trail.hasParticles = paintParticlesEnabled.load();
    
    // Generate particles along the path if enabled
    if (trail.hasParticles && getQualityLevel() >= QualityLevel::Quality)
    {
        const float pathLength = path.getLength();
        const int particleCount = static_cast<int>(pathLength / 10.0f);
        
        for (int i = 0; i < particleCount; ++i)
        {
            const float t = static_cast<float>(i) / particleCount;
            juce::Point<float> point;
            path.getNearestPoint(path.getPointAlongPath(pathLength * t), point);
            trail.particles.push_back(point);
        }
    }
    
    activePaintTrails.push_back(trail);
    
    // Limit number of active trails for performance
    const int maxTrails = (getQualityLevel() == QualityLevel::Performance) ? 10 : 50;
    if (activePaintTrails.size() > maxTrails)
    {
        activePaintTrails.erase(activePaintTrails.begin());
    }
}

void VisualFeedbackEngine::clearPaintStrokes()
{
    activePaintTrails.clear();
}

void VisualFeedbackEngine::PaintTrail::update(float deltaTime)
{
    age += deltaTime;
    isActive = (age < maxAge);
    
    // Update particle positions
    for (auto& particle : particles)
    {
        // Simple particle animation
        particle.x += (juce::Random::getSystemRandom().nextFloat() - 0.5f) * 2.0f;
        particle.y += (juce::Random::getSystemRandom().nextFloat() - 0.5f) * 2.0f;
    }
}

void VisualFeedbackEngine::PaintTrail::render(juce::Graphics& g, const juce::AffineTransform& transform) const
{
    if (!isActive) return;
    
    const float alpha = 1.0f - (age / maxAge);
    const juce::Colour drawColor = color.withAlpha(alpha * intensity);
    
    // Render glow effect
    if (glowRadius > 0.0f && alpha > 0.1f)
    {
        for (float r = glowRadius; r > 0; r -= 1.0f)
        {
            const float glowAlpha = (1.0f - r / glowRadius) * alpha * 0.1f;
            g.setColour(drawColor.withAlpha(glowAlpha));
            g.strokePath(strokePath, juce::PathStrokeType(strokeWidth + r * 2.0f), transform);
        }
    }
    
    // Render main stroke
    g.setColour(drawColor);
    g.strokePath(strokePath, juce::PathStrokeType(strokeWidth), transform);
    
    // Render particles
    if (hasParticles)
    {
        g.setColour(drawColor.withAlpha(alpha * 0.8f));
        for (const auto& particle : particles)
        {
            g.fillEllipse(particle.x - 1, particle.y - 1, 2, 2);
        }
    }
}

//==============================================================================
// Real-Time Visual Effects

void VisualFeedbackEngine::triggerScreenShake(float intensity, float duration)
{
    if (!screenShakeEnabled.load()) return;
    
    screenShake.isActive = true;
    screenShake.intensity = intensity;
    screenShake.duration = duration;
    screenShake.timeRemaining = duration;
}

void VisualFeedbackEngine::triggerFlash(juce::Colour color, float intensity, float duration)
{
    if (!flashEffectsEnabled.load()) return;
    
    flashEffect.isActive = true;
    flashEffect.color = color;
    flashEffect.intensity = intensity;
    flashEffect.duration = duration;
    flashEffect.timeRemaining = duration;
}

void VisualFeedbackEngine::createParticleSystem(juce::Point<float> origin, juce::Colour color, int count)
{
    if (!particleEffectsEnabled.load()) return;
    
    for (int i = 0; i < count; ++i)
    {
        Particle particle;
        particle.position = origin;
        particle.velocity = {
            (juce::Random::getSystemRandom().nextFloat() - 0.5f) * 200.0f,
            (juce::Random::getSystemRandom().nextFloat() - 0.5f) * 200.0f
        };
        particle.color = color;
        particle.life = particle.maxLife = 1.0f + juce::Random::getSystemRandom().nextFloat();
        particle.size = 1.0f + juce::Random::getSystemRandom().nextFloat() * 3.0f;
        
        activeParticles.push_back(particle);
    }
    
    // Limit particle count for performance
    const int maxParticles = (getQualityLevel() == QualityLevel::Performance) ? 100 : 500;
    if (activeParticles.size() > maxParticles)
    {
        activeParticles.erase(activeParticles.begin(), 
                             activeParticles.begin() + (activeParticles.size() - maxParticles));
    }
}

//==============================================================================
// Animation and Effect Updates

void VisualFeedbackEngine::updateAnimation(float deltaTime)
{
    // Update global animation parameters
    rotationAngle += deltaTime * visualization3DParams.rotationSpeed;
    if (rotationAngle > juce::MathConstants<float>::twoPi)
        rotationAngle -= juce::MathConstants<float>::twoPi;
    
    pulsePhase += deltaTime * 2.0f;
    if (pulsePhase > juce::MathConstants<float>::twoPi)
        pulsePhase -= juce::MathConstants<float>::twoPi;
    
    wavePhase += deltaTime * 4.0f;
    if (wavePhase > juce::MathConstants<float>::twoPi)
        wavePhase -= juce::MathConstants<float>::twoPi;
}

void VisualFeedbackEngine::updateEffects(float deltaTime)
{
    // Update paint trails
    for (auto& trail : activePaintTrails)
    {
        trail.update(deltaTime);
    }
    
    // Remove inactive trails
    activePaintTrails.erase(
        std::remove_if(activePaintTrails.begin(), activePaintTrails.end(),
            [](const PaintTrail& trail) { return !trail.isActive; }),
        activePaintTrails.end()
    );
    
    // Update particles
    for (auto& particle : activeParticles)
    {
        particle.update(deltaTime);
    }
    
    // Remove dead particles
    activeParticles.erase(
        std::remove_if(activeParticles.begin(), activeParticles.end(),
            [](const Particle& particle) { return !particle.isAlive(); }),
        activeParticles.end()
    );
    
    // Update screen shake
    screenShake.update(deltaTime);
    
    // Update flash effect
    flashEffect.update(deltaTime);
    
    // Update frequency visualization peaks
    frequencyVisualization.updatePeaks(deltaTime);
}

//==============================================================================
// Rendering Methods

void VisualFeedbackEngine::renderSpectrum2D(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto& spectrum = frequencyVisualization.magnitudes;
    const int numBands = spectrum.size();
    const float bandWidth = static_cast<float>(bounds.getWidth()) / numBands;
    
    for (int i = 0; i < numBands; ++i)
    {
        const float magnitude = spectrum[i];
        const float barHeight = magnitude * bounds.getHeight() * 0.8f;
        
        // Color based on frequency
        const float frequency = (static_cast<float>(i) / numBands) * 22050.0f;
        const juce::Colour barColor = getSpectrumColor(frequency, magnitude);
        
        g.setColour(barColor);
        g.fillRect(i * bandWidth, bounds.getBottom() - barHeight, bandWidth - 1, barHeight);
        
        // Peak indicators
        const float peakHeight = frequencyVisualization.peakValues[i] * bounds.getHeight() * 0.8f;
        g.setColour(barColor.brighter(0.5f));
        g.fillRect(i * bandWidth, bounds.getBottom() - peakHeight, bandWidth - 1, 2.0f);
    }
}

void VisualFeedbackEngine::renderSpectrum3D(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Simplified 3D spectrum rendering using 2D transforms
    const auto& spectrum = frequencyVisualization.magnitudes;
    const int numBands = spectrum.size();
    const float centerX = bounds.getCentreX();
    const float centerY = bounds.getCentreY();
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.3f;
    
    for (int i = 0; i < numBands; ++i)
    {
        const float angle = (static_cast<float>(i) / numBands) * juce::MathConstants<float>::twoPi + rotationAngle;
        const float magnitude = spectrum[i];
        const float barLength = magnitude * radius;
        
        // Calculate 3D-like position
        const float x1 = centerX + std::cos(angle) * radius * 0.5f;
        const float y1 = centerY + std::sin(angle) * radius * 0.5f;
        const float x2 = centerX + std::cos(angle) * (radius * 0.5f + barLength);
        const float y2 = centerY + std::sin(angle) * (radius * 0.5f + barLength);
        
        // Color and transparency for depth effect
        const float depth = (std::cos(angle + rotationAngle) + 1.0f) * 0.5f;
        const juce::Colour barColor = getSpectrumColor(i * 22050.0f / numBands, magnitude)
                                     .withAlpha(0.3f + depth * 0.7f);
        
        g.setColour(barColor);
        g.drawLine(x1, y1, x2, y2, 2.0f + magnitude * 3.0f);
    }
}

void VisualFeedbackEngine::renderFrequencyBars(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Render linear drumming frequency visualization
    frequencyVisualization.renderBands(g, bounds);
}

void VisualFeedbackEngine::renderSpectrumSphere(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Spherical spectrum visualization
    const auto& spectrum = frequencyVisualization.magnitudes;
    const int numBands = spectrum.size();
    const float centerX = bounds.getCentreX();
    const float centerY = bounds.getCentreY();
    const float baseRadius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.2f;
    
    // Draw multiple rings for 3D sphere effect
    const int numRings = 8;
    for (int ring = 0; ring < numRings; ++ring)
    {
        const float ringAngle = (static_cast<float>(ring) / numRings) * juce::MathConstants<float>::pi;
        const float ringRadius = baseRadius * std::sin(ringAngle);
        const float ringY = centerY + baseRadius * std::cos(ringAngle) * 0.5f;
        
        const int bandsPerRing = numBands / numRings;
        for (int i = 0; i < bandsPerRing; ++i)
        {
            const int bandIndex = ring * bandsPerRing + i;
            if (bandIndex >= numBands) break;
            
            const float angle = (static_cast<float>(i) / bandsPerRing) * juce::MathConstants<float>::twoPi + rotationAngle;
            const float magnitude = spectrum[bandIndex];
            const float x = centerX + std::cos(angle) * ringRadius;
            const float y = ringY;
            
            const juce::Colour pointColor = getSpectrumColor(bandIndex * 22050.0f / numBands, magnitude);
            g.setColour(pointColor);
            
            const float pointSize = 2.0f + magnitude * 8.0f;
            g.fillEllipse(x - pointSize * 0.5f, y - pointSize * 0.5f, pointSize, pointSize);
        }
    }
}

void VisualFeedbackEngine::renderParticleField(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Audio-reactive particle field
    renderParticles(g);
    
    // Add grid for reference
    if (visualization3DParams.showGrid)
    {
        renderGrid(g, bounds);
    }
}

void VisualFeedbackEngine::renderPaintTrails(juce::Graphics& g)
{
    if (!paintGlowEnabled.load() && getQualityLevel() == QualityLevel::Performance)
    {
        // Simple rendering for performance
        for (const auto& trail : activePaintTrails)
        {
            if (trail.isActive)
            {
                const float alpha = 1.0f - (trail.age / trail.maxAge);
                g.setColour(trail.color.withAlpha(alpha * trail.intensity));
                g.strokePath(trail.strokePath, juce::PathStrokeType(trail.strokeWidth));
            }
        }
    }
    else
    {
        // Full quality rendering with effects
        for (const auto& trail : activePaintTrails)
        {
            trail.render(g, juce::AffineTransform::identity);
        }
    }
}

void VisualFeedbackEngine::renderParticles(juce::Graphics& g)
{
    for (const auto& particle : activeParticles)
    {
        if (particle.isAlive())
        {
            const float alpha = particle.life / particle.maxLife;
            g.setColour(particle.color.withAlpha(alpha));
            g.fillEllipse(particle.position.x - particle.size * 0.5f,
                         particle.position.y - particle.size * 0.5f,
                         particle.size, particle.size);
        }
    }
}

void VisualFeedbackEngine::renderScreenEffects(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Flash effect
    if (flashEffect.isActive)
    {
        const float alpha = flashEffect.getCurrentAlpha();
        g.setColour(flashEffect.color.withAlpha(alpha));
        g.fillRect(bounds);
    }
    
    // Chromatic aberration would be implemented here for advanced effects
    // This would typically require custom shaders or multiple passes
}

void VisualFeedbackEngine::renderGrid(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(currentColorTheme.gridColor);
    
    const int gridSpacing = 50;
    
    // Vertical lines
    for (int x = 0; x < bounds.getWidth(); x += gridSpacing)
    {
        g.drawVerticalLine(x, 0.0f, static_cast<float>(bounds.getHeight()));
    }
    
    // Horizontal lines
    for (int y = 0; y < bounds.getHeight(); y += gridSpacing)
    {
        g.drawHorizontalLine(y, 0.0f, static_cast<float>(bounds.getWidth()));
    }
}

//==============================================================================
// Color and Theme Management

juce::Colour VisualFeedbackEngine::getSpectrumColor(float frequency, float magnitude) const
{
    // Color based on frequency range
    if (frequency < 250.0f) // Bass
        return currentColorTheme.spectrumLow.withAlpha(magnitude);
    else if (frequency < 4000.0f) // Mids
        return currentColorTheme.spectrumMid.withAlpha(magnitude);
    else // Highs
        return currentColorTheme.spectrumHigh.withAlpha(magnitude);
}

void VisualFeedbackEngine::setColorScheme(ColorScheme scheme)
{
    switch (scheme)
    {
        case ColorScheme::Classic:
            currentColorTheme.backgroundColor = juce::Colour(0xff000000);
            currentColorTheme.foregroundColor = juce::Colour(0xff00ff00);
            currentColorTheme.accentColor = juce::Colour(0xffffff00);
            currentColorTheme.spectrumLow = juce::Colour(0xff0000ff);
            currentColorTheme.spectrumMid = juce::Colour(0xff00ff00);
            currentColorTheme.spectrumHigh = juce::Colour(0xffff0000);
            break;
            
        case ColorScheme::Modern:
            currentColorTheme.backgroundColor = juce::Colour(0xff1a1a1a);
            currentColorTheme.foregroundColor = juce::Colour(0xffffffff);
            currentColorTheme.accentColor = juce::Colour(0xff00aaff);
            currentColorTheme.spectrumLow = juce::Colour(0xff4080ff);
            currentColorTheme.spectrumMid = juce::Colour(0xff40ff80);
            currentColorTheme.spectrumHigh = juce::Colour(0xffff8040);
            break;
            
        case ColorScheme::Neon:
            currentColorTheme.backgroundColor = juce::Colour(0xff0a0a0a);
            currentColorTheme.foregroundColor = juce::Colour(0xffffffff);
            currentColorTheme.accentColor = juce::Colour(0xffff00ff);
            currentColorTheme.spectrumLow = juce::Colour(0xff00ffff);
            currentColorTheme.spectrumMid = juce::Colour(0xffff00ff);
            currentColorTheme.spectrumHigh = juce::Colour(0xffffff00);
            break;
            
        default:
            break;
    }
}

//==============================================================================
// Performance and Quality Management

void VisualFeedbackEngine::setQualityLevel(QualityLevel level)
{
    currentQualityLevel.store(static_cast<int>(level));
    applyQualitySettings();
}

void VisualFeedbackEngine::applyQualitySettings()
{
    const auto quality = getQualityLevel();
    
    switch (quality)
    {
        case QualityLevel::Performance:
            paintGlowEnabled.store(false);
            paintParticlesEnabled.store(false);
            particleEffectsEnabled.store(false);
            chromaticAberrationEnabled.store(false);
            break;
            
        case QualityLevel::Balanced:
            paintGlowEnabled.store(true);
            paintParticlesEnabled.store(false);
            particleEffectsEnabled.store(true);
            chromaticAberrationEnabled.store(false);
            break;
            
        case QualityLevel::Quality:
        case QualityLevel::Ultra:
            paintGlowEnabled.store(true);
            paintParticlesEnabled.store(true);
            particleEffectsEnabled.store(true);
            chromaticAberrationEnabled.store(quality == QualityLevel::Ultra);
            break;
    }
}

VisualFeedbackEngine::PerformanceMetrics VisualFeedbackEngine::getPerformanceMetrics() const
{
    return performanceMetrics;
}

void VisualFeedbackEngine::updatePerformanceMetrics()
{
    frameCounter++;
    
    if (deltaTime > 0.0f)
    {
        frameTimes.push_back(deltaTime * 1000.0f); // Convert to ms
    }
    
    // Update metrics every second
    auto now = juce::Time::getCurrentTime();
    if ((now - lastPerformanceUpdate).inMilliseconds() >= 1000)
    {
        if (!frameTimes.empty())
        {
            float totalTime = 0.0f;
            for (float time : frameTimes)
                totalTime += time;
            
            performanceMetrics.frameTimeMs = totalTime / frameTimes.size();
            performanceMetrics.averageFPS = 1000.0f / performanceMetrics.frameTimeMs;
            frameTimes.clear();
        }
        
        performanceMetrics.activeParticles = static_cast<int>(activeParticles.size());
        performanceMetrics.activePaintTrails = static_cast<int>(activePaintTrails.size());
        
        lastPerformanceUpdate = now;
    }
}

//==============================================================================
// Helper Method Implementations

juce::Point<float> VisualFeedbackEngine::ScreenShake::getCurrentOffset()
{
    if (!isActive) return {0.0f, 0.0f};
    
    const float strength = intensity * (timeRemaining / duration);
    return {
        (random.nextFloat() - 0.5f) * strength * 10.0f,
        (random.nextFloat() - 0.5f) * strength * 10.0f
    };
}

void VisualFeedbackEngine::ScreenShake::update(float deltaTime)
{
    if (isActive)
    {
        timeRemaining -= deltaTime;
        if (timeRemaining <= 0.0f)
        {
            isActive = false;
            timeRemaining = 0.0f;
        }
    }
}

void VisualFeedbackEngine::FlashEffect::update(float deltaTime)
{
    if (isActive)
    {
        timeRemaining -= deltaTime;
        if (timeRemaining <= 0.0f)
        {
            isActive = false;
            timeRemaining = 0.0f;
        }
    }
}

float VisualFeedbackEngine::FlashEffect::getCurrentAlpha() const
{
    if (!isActive) return 0.0f;
    return intensity * (timeRemaining / duration);
}

void VisualFeedbackEngine::Particle::update(float deltaTime)
{
    position.x += velocity.x * deltaTime;
    position.y += velocity.y * deltaTime;
    
    // Apply gravity
    velocity.y += 100.0f * deltaTime;
    
    // Apply friction
    velocity.x *= 0.98f;
    velocity.y *= 0.98f;
    
    life -= deltaTime;
}

//==============================================================================
// Frequency Visualization Implementation

void VisualFeedbackEngine::FrequencyVisualization::updateFromSpectrum(const std::vector<float>& spectrum, float sampleRate)
{
    const int numBands = juce::jmin(static_cast<int>(spectrum.size()), NUM_BANDS);
    
    for (int i = 0; i < numBands; ++i)
    {
        magnitudes[i] = spectrum[i];
        
        // Update peaks
        if (spectrum[i] > peakValues[i])
        {
            peakValues[i] = spectrum[i];
            peakAges[i] = 0.0f;
        }
    }
    
    // Update drum frequency bands
    for (auto& band : drumFrequencyBands)
    {
        band.currentLevel = 0.0f;
        
        // Find spectrum bins that fall within this frequency range
        const float binWidth = sampleRate / (2.0f * spectrum.size());
        const int lowBin = static_cast<int>(band.lowFreq / binWidth);
        const int highBin = static_cast<int>(band.highFreq / binWidth);
        
        float maxLevel = 0.0f;
        for (int bin = lowBin; bin <= highBin && bin < spectrum.size(); ++bin)
        {
            maxLevel = juce::jmax(maxLevel, spectrum[bin]);
        }
        
        band.currentLevel = maxLevel;
        if (maxLevel > band.peakLevel)
        {
            band.peakLevel = maxLevel;
        }
        
        band.isActive = (maxLevel > 0.01f);
    }
}

void VisualFeedbackEngine::FrequencyVisualization::updatePeaks(float deltaTime)
{
    for (int i = 0; i < NUM_BANDS; ++i)
    {
        peakAges[i] += deltaTime;
        
        // Decay peaks over time
        if (peakAges[i] > 0.5f)
        {
            peakValues[i] *= 0.95f;
        }
    }
    
    // Update drum band peaks
    for (auto& band : drumFrequencyBands)
    {
        band.peakLevel *= 0.98f; // Slow decay
    }
}

void VisualFeedbackEngine::FrequencyVisualization::renderBands(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const float bandHeight = static_cast<float>(bounds.getHeight()) / drumFrequencyBands.size();
    
    for (size_t i = 0; i < drumFrequencyBands.size(); ++i)
    {
        const auto& band = drumFrequencyBands[i];
        const auto bandBounds = juce::Rectangle<float>(
            static_cast<float>(bounds.getX()),
            static_cast<float>(bounds.getY()) + i * bandHeight,
            static_cast<float>(bounds.getWidth()),
            bandHeight - 2.0f
        );
        
        // Background
        g.setColour(juce::Colour(0xff2a2a2a));
        g.fillRect(bandBounds);
        
        // Current level
        const float levelWidth = bandBounds.getWidth() * band.currentLevel;
        g.setColour(band.bandColor.withAlpha(band.isActive ? 0.8f : 0.3f));
        g.fillRect(bandBounds.getX(), bandBounds.getY(), levelWidth, bandBounds.getHeight());
        
        // Peak indicator
        const float peakX = bandBounds.getX() + bandBounds.getWidth() * band.peakLevel;
        g.setColour(band.bandColor.brighter());
        g.fillRect(peakX - 1, bandBounds.getY(), 2.0f, bandBounds.getHeight());
        
        // Band name
        g.setColour(juce::Colours::white);
        g.setFont(12.0f);
        g.drawText(band.bandName, bandBounds.reduced(5), juce::Justification::centredLeft);
    }
}

//==============================================================================
// Tracker Visualization Implementation

void VisualFeedbackEngine::TrackerVisualization::updateFromPattern(const std::vector<std::vector<int>>& pattern)
{
    // Clear existing pattern
    for (int track = 0; track < MAX_TRACKS; ++track)
    {
        for (int row = 0; row < MAX_ROWS; ++row)
        {
            cells[track][row].hasNote = false;
            cells[track][row].intensity = 0.0f;
            cells[track][row].isPlaying = false;
        }
    }
    
    // Update with new pattern data
    for (size_t track = 0; track < pattern.size() && track < MAX_TRACKS; ++track)
    {
        for (size_t row = 0; row < pattern[track].size() && row < MAX_ROWS; ++row)
        {
            if (pattern[track][row] > 0)
            {
                cells[track][row].hasNote = true;
                cells[track][row].intensity = static_cast<float>(pattern[track][row]) / 127.0f;
                cells[track][row].cellColor = trackColors[track];
            }
        }
    }
}

void VisualFeedbackEngine::TrackerVisualization::setPlaybackPosition(int row, float subRowPosition)
{
    currentPlayRow = row;
    rowHighlightPosition = row + subRowPosition;
    
    // Update playing state
    for (int track = 0; track < MAX_TRACKS; ++track)
    {
        for (int r = 0; r < MAX_ROWS; ++r)
        {
            cells[track][r].isPlaying = (r == row);
        }
    }
}

void VisualFeedbackEngine::TrackerVisualization::renderPattern(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (bounds.isEmpty()) return;
    
    const float cellWidth = trackWidth;
    const float cellHeight = static_cast<float>(bounds.getHeight()) / MAX_ROWS;
    const int visibleTracks = juce::jmin(MAX_TRACKS, bounds.getWidth() / static_cast<int>(cellWidth));
    
    // Draw pattern grid
    for (int track = 0; track < visibleTracks; ++track)
    {
        for (int row = 0; row < MAX_ROWS; ++row)
        {
            const auto& cell = cells[track][row];
            const auto cellBounds = juce::Rectangle<float>(
                bounds.getX() + track * cellWidth,
                bounds.getY() + row * cellHeight,
                cellWidth - cellSpacing,
                cellHeight - cellSpacing
            );
            
            // Background
            juce::Colour bgColor = juce::Colour(0xff1a1a1a);
            if (cell.isPlaying)
            {
                bgColor = trackColors[track].withAlpha(0.3f);
            }
            
            g.setColour(bgColor);
            g.fillRect(cellBounds);
            
            // Note indicator
            if (cell.hasNote)
            {
                g.setColour(cell.cellColor.withAlpha(cell.intensity));
                g.fillRect(cellBounds.reduced(1));
            }
            
            // Row numbers
            if (track == 0 && showRowNumbers)
            {
                g.setColour(juce::Colours::grey);
                g.setFont(10.0f);
                g.drawText(juce::String(row), 
                          cellBounds.getX() - 20, cellBounds.getY(), 
                          15, cellBounds.getHeight(),
                          juce::Justification::centredRight);
            }
        }
        
        // Track names
        if (showTrackNames)
        {
            g.setColour(trackColors[track]);
            g.setFont(12.0f);
            g.drawText(juce::String(track + 1),
                      bounds.getX() + track * cellWidth, bounds.getY() - 20,
                      cellWidth, 15,
                      juce::Justification::centred);
        }
    }
    
    // Playback position highlight
    const float highlightY = bounds.getY() + rowHighlightPosition * cellHeight;
    g.setColour(juce::Colours::yellow.withAlpha(0.3f));
    g.fillRect(static_cast<float>(bounds.getX()), highlightY, 
              static_cast<float>(visibleTracks) * cellWidth, cellHeight);
}