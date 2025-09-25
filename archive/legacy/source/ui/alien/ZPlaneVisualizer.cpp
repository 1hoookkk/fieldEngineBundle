#include "ZPlaneVisualizer.h"
#include "AlienGlyphs.h"
#include <cmath>

namespace AlienUI
{
    ZPlaneVisualizer::ZPlaneVisualizer()
    {
        // Setup OpenGL if available
        if (useOpenGL)
        {
            openGLContext.setRenderer(this);
            openGLContext.attachTo(*this);
            openGLContext.setContinuousRepainting(true);
        }
        
        // Start animation timer
        startTimerHz(60);
        
        // Initialize with default filter
        FilterCoefficients defaultCoeffs;
        defaultCoeffs.numPoles = 2;
        defaultCoeffs.numZeros = 2;
        defaultCoeffs.a[0] = std::complex<float>(0.5f, 0.3f);
        defaultCoeffs.a[1] = std::complex<float>(0.5f, -0.3f);
        defaultCoeffs.b[0] = std::complex<float>(-0.8f, 0.0f);
        defaultCoeffs.b[1] = std::complex<float>(0.8f, 0.0f);
        updateCoefficients(defaultCoeffs);
        
        setOpaque(true);
    }
    
    ZPlaneVisualizer::~ZPlaneVisualizer()
    {
        if (useOpenGL)
        {
            openGLContext.detach();
        }
        stopTimer();
    }
    
    void ZPlaneVisualizer::paint(juce::Graphics& g)
    {
        if (!useOpenGL)
        {
            // Software rendering fallback
            renderBackground(g);
            renderGrid(g);
            renderUnitCircle(g);
            renderConstellationConnections(g);
            renderFrequencyResponse(g);
            renderCoefficients(g);
            renderAlienEffects(g);
        }
    }
    
    void ZPlaneVisualizer::resized()
    {
        needsBackgroundRedraw = true;
        
        if (useOpenGL)
        {
            openGLContext.triggerRepaint();
        }
    }
    
    void ZPlaneVisualizer::timerCallback()
    {
        // Animate coefficients
        animateCoefficients();
        
        // Update energy field
        updateEnergyField();
        
        // Increment phase for animations
        fieldPhase += 0.02f;
        if (fieldPhase > juce::MathConstants<float>::twoPi)
            fieldPhase -= juce::MathConstants<float>::twoPi;
        
        repaint();
    }
    
    void ZPlaneVisualizer::renderBackground(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Deep space gradient background
        juce::ColourGradient spaceGradient(Colors::deepSpace, bounds.getTopLeft(),
                                          Colors::bgLayer2, bounds.getBottomRight(), true);
        spaceGradient.addColour(0.5f, Colors::bgLayer1);
        g.setGradientFill(spaceGradient);
        g.fillRect(bounds);
        
        // Add subtle nebula effect
        juce::Random rng(42);  // Fixed seed for consistent stars
        g.setColour(Colors::starWhite.withAlpha(0.05f));
        
        for (int i = 0; i < 100; ++i)
        {
            float x = rng.nextFloat() * bounds.getWidth();
            float y = rng.nextFloat() * bounds.getHeight();
            float size = rng.nextFloat() * 2.0f + 0.5f;
            float alpha = rng.nextFloat() * 0.5f + 0.1f;
            
            g.setColour(Colors::starWhite.withAlpha(alpha));
            g.fillEllipse(x - size * 0.5f, y - size * 0.5f, size, size);
        }
    }
    
    void ZPlaneVisualizer::renderGrid(juce::Graphics& g)
    {
        if (!showGrid) return;
        
        auto bounds = getLocalBounds().toFloat();
        auto centre = bounds.getCentre();
        auto scale = juce::jmin(bounds.getWidth(), bounds.getHeight()) * unitCircleRadius;
        
        g.setColour(Colors::gridLines);
        
        // Radial grid lines
        const int numRadialLines = 12;
        for (int i = 0; i < numRadialLines; ++i)
        {
            float angle = (float)i * juce::MathConstants<float>::twoPi / numRadialLines;
            auto endPoint = centre.getPointOnCircumference(scale * 1.2f, angle);
            g.drawLine(centre.x, centre.y, endPoint.x, endPoint.y, 0.5f);
        }
        
        // Concentric circles
        const int numCircles = 4;
        for (int i = 1; i <= numCircles; ++i)
        {
            float radius = scale * (float)i / numCircles;
            g.drawEllipse(centre.x - radius, centre.y - radius,
                         radius * 2.0f, radius * 2.0f, 0.5f);
        }
    }
    
    void ZPlaneVisualizer::renderUnitCircle(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        auto centre = bounds.getCentre();
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * unitCircleRadius;
        
        // Main unit circle with glow
        g.setColour(Colors::cosmicBlue.withAlpha(0.2f));
        for (int i = 3; i > 0; --i)
        {
            float glowRadius = radius + i * 4.0f;
            g.drawEllipse(centre.x - glowRadius, centre.y - glowRadius,
                         glowRadius * 2.0f, glowRadius * 2.0f, 2.0f);
        }
        
        g.setColour(Colors::cosmicBlue);
        g.drawEllipse(centre.x - radius, centre.y - radius,
                     radius * 2.0f, radius * 2.0f, 2.0f);
        
        // Add energy pulse effect
        float pulseRadius = radius * (1.0f + 0.05f * std::sin(fieldPhase * 2.0f));
        g.setColour(Colors::plasmaGlow.withAlpha(0.3f * std::abs(std::sin(fieldPhase))));
        g.drawEllipse(centre.x - pulseRadius, centre.y - pulseRadius,
                     pulseRadius * 2.0f, pulseRadius * 2.0f, 1.0f);
    }
    
    void ZPlaneVisualizer::renderCoefficients(juce::Graphics& g)
    {
        // Render poles
        for (size_t i = 0; i < polePositions.size(); ++i)
        {
            auto& pole = polePositions[i];
            auto screenPos = complexToScreen(currentCoeffs.a[i]);
            
            // Glow effect
            float glowIntensity = 0.5f + 0.5f * std::sin(pole.pulsePhase);
            g.setColour(Colors::filterPole.withAlpha(glowIntensity * 0.3f));
            g.fillEllipse(screenPos.x - glowRadius, screenPos.y - glowRadius,
                         glowRadius * 2.0f, glowRadius * 2.0f);
            
            // Pole marker (X)
            g.setColour(Colors::filterPole);
            g.drawLine(screenPos.x - pointRadius, screenPos.y - pointRadius,
                      screenPos.x + pointRadius, screenPos.y + pointRadius, 2.0f);
            g.drawLine(screenPos.x + pointRadius, screenPos.y - pointRadius,
                      screenPos.x - pointRadius, screenPos.y + pointRadius, 2.0f);
            
            // Selection highlight
            if (selectedPoleIndex == (int)i)
            {
                g.setColour(Colors::starWhite.withAlpha(0.8f));
                g.drawEllipse(screenPos.x - pointRadius * 1.5f, screenPos.y - pointRadius * 1.5f,
                             pointRadius * 3.0f, pointRadius * 3.0f, 2.0f);
            }
        }
        
        // Render zeros
        for (size_t i = 0; i < zeroPositions.size(); ++i)
        {
            auto& zero = zeroPositions[i];
            auto screenPos = complexToScreen(currentCoeffs.b[i]);
            
            // Glow effect
            float glowIntensity = 0.5f + 0.5f * std::sin(zero.pulsePhase + juce::MathConstants<float>::pi);
            g.setColour(Colors::filterZero.withAlpha(glowIntensity * 0.3f));
            g.fillEllipse(screenPos.x - glowRadius, screenPos.y - glowRadius,
                         glowRadius * 2.0f, glowRadius * 2.0f);
            
            // Zero marker (O)
            g.setColour(Colors::filterZero);
            g.drawEllipse(screenPos.x - pointRadius, screenPos.y - pointRadius,
                         pointRadius * 2.0f, pointRadius * 2.0f, 2.0f);
            
            // Selection highlight
            if (selectedZeroIndex == (int)i)
            {
                g.setColour(Colors::starWhite.withAlpha(0.8f));
                g.drawEllipse(screenPos.x - pointRadius * 1.5f, screenPos.y - pointRadius * 1.5f,
                             pointRadius * 3.0f, pointRadius * 3.0f, 2.0f);
            }
        }
    }
    
    void ZPlaneVisualizer::renderConstellationConnections(juce::Graphics& g)
    {
        // Connect poles and zeros with energy lines
        g.setColour(Colors::resonanceGlow.withAlpha(0.3f));
        
        // Connect conjugate pairs
        for (int i = 0; i < currentCoeffs.numPoles - 1; ++i)
        {
            auto p1 = complexToScreen(currentCoeffs.a[i]);
            auto p2 = complexToScreen(currentCoeffs.a[i + 1]);
            
            // Check if conjugate pair (imaginary parts have opposite signs)
            if (std::abs(currentCoeffs.a[i].real() - currentCoeffs.a[i + 1].real()) < 0.01f &&
                std::abs(currentCoeffs.a[i].imag() + currentCoeffs.a[i + 1].imag()) < 0.01f)
            {
                // Draw energy arc between conjugates
                juce::Path arc;
                arc.startNewSubPath(p1);
                auto control = juce::Point<float>((p1.x + p2.x) * 0.5f - 20.0f, (p1.y + p2.y) * 0.5f);
                arc.quadraticTo(control, p2);
                
                g.strokePath(arc, juce::PathStrokeType(1.5f));
            }
        }
        
        // Similar for zeros
        for (int i = 0; i < currentCoeffs.numZeros - 1; ++i)
        {
            auto z1 = complexToScreen(currentCoeffs.b[i]);
            auto z2 = complexToScreen(currentCoeffs.b[i + 1]);
            
            if (std::abs(currentCoeffs.b[i].real() - currentCoeffs.b[i + 1].real()) < 0.01f &&
                std::abs(currentCoeffs.b[i].imag() + currentCoeffs.b[i + 1].imag()) < 0.01f)
            {
                juce::Path arc;
                arc.startNewSubPath(z1);
                auto control = juce::Point<float>((z1.x + z2.x) * 0.5f + 20.0f, (z1.y + z2.y) * 0.5f);
                arc.quadraticTo(control, z2);
                
                g.setColour(Colors::resonanceGlow.withAlpha(0.2f));
                g.strokePath(arc, juce::PathStrokeType(1.5f));
            }
        }
    }
    
    void ZPlaneVisualizer::renderFrequencyResponse(juce::Graphics& g)
    {
        if (!showFreqResponse) return;
        
        auto bounds = getLocalBounds().toFloat();
        auto responseArea = bounds.removeFromBottom(bounds.getHeight() * 0.2f);
        
        // Background for frequency response
        g.setColour(Colors::bgLayer2.withAlpha(0.8f));
        g.fillRoundedRectangle(responseArea, 4.0f);
        
        // Calculate and draw magnitude response
        juce::Path magnitudePath;
        const int numPoints = 256;
        
        for (int i = 0; i < numPoints; ++i)
        {
            float freq = (float)i / (float)numPoints * juce::MathConstants<float>::pi;
            std::complex<float> z = std::exp(std::complex<float>(0.0f, freq));
            
            // Calculate H(z) = B(z) / A(z)
            std::complex<float> numerator = 1.0f;
            std::complex<float> denominator = 1.0f;
            
            for (int j = 0; j < currentCoeffs.numZeros; ++j)
            {
                numerator *= (z - currentCoeffs.b[j]);
            }
            
            for (int j = 0; j < currentCoeffs.numPoles; ++j)
            {
                denominator *= (z - currentCoeffs.a[j]);
            }
            
            float magnitude = std::abs(numerator / denominator);
            float magnitudeDb = 20.0f * std::log10(juce::jmax(0.0001f, magnitude));
            
            float x = responseArea.getX() + (float)i / (float)numPoints * responseArea.getWidth();
            float y = juce::jmap(magnitudeDb, -40.0f, 20.0f,
                                responseArea.getBottom(), responseArea.getY());
            
            if (i == 0)
                magnitudePath.startNewSubPath(x, y);
            else
                magnitudePath.lineTo(x, y);
        }
        
        g.setColour(Colors::plasmaGlow);
        g.strokePath(magnitudePath, juce::PathStrokeType(2.0f));
        
        // Grid lines
        g.setColour(Colors::gridLines);
        g.drawHorizontalLine((int)responseArea.getCentreY(), responseArea.getX(), responseArea.getRight());
        
        // Labels
        g.setFont(Glyphs::createAlienFont(10.0f));
        g.setColour(Colors::textSecondary);
        g.drawText("20Hz", responseArea.getX(), responseArea.getBottom() - 15, 40, 15,
                   juce::Justification::left);
        g.drawText("20kHz", responseArea.getRight() - 40, responseArea.getBottom() - 15, 40, 15,
                   juce::Justification::right);
    }
    
    void ZPlaneVisualizer::renderAlienEffects(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Energy field visualization
        const float cellSize = bounds.getWidth() / 32.0f;
        
        for (int x = 0; x < 32; ++x)
        {
            for (int y = 0; y < 32; ++y)
            {
                float energy = energyField[x][y];
                if (energy > 0.1f)
                {
                    float xPos = x * cellSize;
                    float yPos = y * cellSize;
                    
                    g.setColour(Colors::plasmaGlow.withAlpha(energy * 0.2f));
                    g.fillEllipse(xPos, yPos, cellSize * 2.0f, cellSize * 2.0f);
                }
            }
        }
        
        // Scanning line effect
        float scanY = bounds.getHeight() * (0.5f + 0.5f * std::sin(fieldPhase));
        g.setColour(Colors::cosmicBlue.withAlpha(0.1f));
        g.fillRect(0.0f, scanY - 2.0f, bounds.getWidth(), 4.0f);
    }
    
    void ZPlaneVisualizer::updateCoefficients(const FilterCoefficients& coeffs)
    {
        targetCoeffs = coeffs;
        
        // Ensure we have enough animated points
        while (polePositions.size() < coeffs.numPoles)
        {
            AnimatedPoint ap;
            ap.pulsePhase = juce::Random::getSystemRandom().nextFloat() * juce::MathConstants<float>::twoPi;
            polePositions.push_back(ap);
        }
        polePositions.resize(coeffs.numPoles);
        
        while (zeroPositions.size() < coeffs.numZeros)
        {
            AnimatedPoint ap;
            ap.pulsePhase = juce::Random::getSystemRandom().nextFloat() * juce::MathConstants<float>::twoPi;
            zeroPositions.push_back(ap);
        }
        zeroPositions.resize(coeffs.numZeros);
        
        // Set target positions
        for (int i = 0; i < coeffs.numPoles; ++i)
        {
            polePositions[i].target = complexToScreen(coeffs.a[i]);
        }
        
        for (int i = 0; i < coeffs.numZeros; ++i)
        {
            zeroPositions[i].target = complexToScreen(coeffs.b[i]);
        }
    }
    
    void ZPlaneVisualizer::animateCoefficients()
    {
        const float smoothing = 0.15f;
        const float energyDecay = 0.95f;
        
        // Animate poles
        for (size_t i = 0; i < polePositions.size(); ++i)
        {
            auto& pole = polePositions[i];
            
            // Smooth movement
            pole.current = pole.current + (pole.target - pole.current) * smoothing;
            
            // Update energy based on movement
            float movement = (pole.target - pole.current).getDistanceFromOrigin();
            pole.energy = pole.energy * energyDecay + movement * 0.1f;
            pole.energy = juce::jlimit(0.0f, 1.0f, pole.energy);
            
            // Update pulse phase
            pole.pulsePhase += 0.05f + pole.energy * 0.1f;
            if (pole.pulsePhase > juce::MathConstants<float>::twoPi)
                pole.pulsePhase -= juce::MathConstants<float>::twoPi;
            
            // Update actual coefficient for smooth animation
            if (i < currentCoeffs.numPoles)
            {
                currentCoeffs.a[i] = screenToComplex(pole.current);
            }
        }
        
        // Animate zeros
        for (size_t i = 0; i < zeroPositions.size(); ++i)
        {
            auto& zero = zeroPositions[i];
            
            zero.current = zero.current + (zero.target - zero.current) * smoothing;
            
            float movement = (zero.target - zero.current).getDistanceFromOrigin();
            zero.energy = zero.energy * energyDecay + movement * 0.1f;
            zero.energy = juce::jlimit(0.0f, 1.0f, zero.energy);
            
            zero.pulsePhase += 0.05f + zero.energy * 0.1f;
            if (zero.pulsePhase > juce::MathConstants<float>::twoPi)
                zero.pulsePhase -= juce::MathConstants<float>::twoPi;
            
            if (i < currentCoeffs.numZeros)
            {
                currentCoeffs.b[i] = screenToComplex(zero.current);
            }
        }
        
        currentCoeffs.numPoles = targetCoeffs.numPoles;
        currentCoeffs.numZeros = targetCoeffs.numZeros;
    }
    
    void ZPlaneVisualizer::updateEnergyField()
    {
        // Update energy field based on pole and zero positions
        const float decay = 0.9f;
        
        // Decay existing field
        for (auto& row : energyField)
        {
            for (auto& cell : row)
            {
                cell *= decay;
            }
        }
        
        // Add energy from poles and zeros
        auto bounds = getLocalBounds().toFloat();
        const float cellSize = bounds.getWidth() / 32.0f;
        
        auto addEnergy = [&](juce::Point<float> pos, float intensity)
        {
            int gridX = (int)(pos.x / cellSize);
            int gridY = (int)(pos.y / cellSize);
            
            // Add energy in a radius around the point
            const int radius = 3;
            for (int dx = -radius; dx <= radius; ++dx)
            {
                for (int dy = -radius; dy <= radius; ++dy)
                {
                    int x = gridX + dx;
                    int y = gridY + dy;
                    
                    if (x >= 0 && x < 32 && y >= 0 && y < 32)
                    {
                        float dist = std::sqrt(dx * dx + dy * dy);
                        float energy = intensity * std::exp(-dist * 0.5f);
                        energyField[x][y] = juce::jlimit(0.0f, 1.0f, energyField[x][y] + energy);
                    }
                }
            }
        };
        
        // Add energy from animated points
        for (const auto& pole : polePositions)
        {
            addEnergy(pole.current, pole.energy * 0.5f);
        }
        
        for (const auto& zero : zeroPositions)
        {
            addEnergy(zero.current, zero.energy * 0.3f);
        }
    }
    
    juce::Point<float> ZPlaneVisualizer::complexToScreen(const std::complex<float>& c) const
    {
        auto bounds = getLocalBounds().toFloat();
        auto centre = bounds.getCentre();
        auto scale = juce::jmin(bounds.getWidth(), bounds.getHeight()) * unitCircleRadius;
        
        return juce::Point<float>(
            centre.x + c.real() * scale,
            centre.y - c.imag() * scale  // Flip Y for conventional orientation
        );
    }
    
    std::complex<float> ZPlaneVisualizer::screenToComplex(juce::Point<float> p) const
    {
        auto bounds = getLocalBounds().toFloat();
        auto centre = bounds.getCentre();
        auto scale = juce::jmin(bounds.getWidth(), bounds.getHeight()) * unitCircleRadius;
        
        return std::complex<float>(
            (p.x - centre.x) / scale,
            -(p.y - centre.y) / scale  // Flip Y for conventional orientation
        );
    }
    
    void ZPlaneVisualizer::mouseDown(const juce::MouseEvent& event)
    {
        if (!isInteractive) return;
        
        auto pos = event.position;
        
        // Check if clicking on a pole
        for (int i = 0; i < currentCoeffs.numPoles; ++i)
        {
            auto polePos = complexToScreen(currentCoeffs.a[i]);
            if (pos.getDistanceFrom(polePos) < pointRadius * 2.0f)
            {
                selectedPoleIndex = i;
                selectedZeroIndex = -1;
                isDragging = true;
                dragOffset = pos - polePos;
                return;
            }
        }
        
        // Check if clicking on a zero
        for (int i = 0; i < currentCoeffs.numZeros; ++i)
        {
            auto zeroPos = complexToScreen(currentCoeffs.b[i]);
            if (pos.getDistanceFrom(zeroPos) < pointRadius * 2.0f)
            {
                selectedZeroIndex = i;
                selectedPoleIndex = -1;
                isDragging = true;
                dragOffset = pos - zeroPos;
                return;
            }
        }
    }
    
    void ZPlaneVisualizer::mouseDrag(const juce::MouseEvent& event)
    {
        if (!isDragging || !isInteractive) return;
        
        auto newPos = event.position - dragOffset;
        auto complexPos = screenToComplex(newPos);
        
        if (selectedPoleIndex >= 0)
        {
            targetCoeffs.a[selectedPoleIndex] = complexPos;
            polePositions[selectedPoleIndex].target = newPos;
            
            if (onPoleChanged)
            {
                float freq = getFrequencyAtPoint(newPos);
                float res = std::abs(complexPos);
                onPoleChanged(freq, res);
            }
        }
        else if (selectedZeroIndex >= 0)
        {
            targetCoeffs.b[selectedZeroIndex] = complexPos;
            zeroPositions[selectedZeroIndex].target = newPos;
            
            if (onZeroChanged)
            {
                float freq = getFrequencyAtPoint(newPos);
                float res = std::abs(complexPos);
                onZeroChanged(freq, res);
            }
        }
    }
    
    void ZPlaneVisualizer::mouseUp(const juce::MouseEvent&)
    {
        isDragging = false;
        selectedPoleIndex = -1;
        selectedZeroIndex = -1;
    }
    
    void ZPlaneVisualizer::mouseMove(const juce::MouseEvent& event)
    {
        // Update cursor based on hover state
        bool overInteractive = false;
        
        if (isInteractive)
        {
            auto pos = event.position;
            
            for (int i = 0; i < currentCoeffs.numPoles; ++i)
            {
                auto polePos = complexToScreen(currentCoeffs.a[i]);
                if (pos.getDistanceFrom(polePos) < pointRadius * 2.0f)
                {
                    overInteractive = true;
                    break;
                }
            }
            
            if (!overInteractive)
            {
                for (int i = 0; i < currentCoeffs.numZeros; ++i)
                {
                    auto zeroPos = complexToScreen(currentCoeffs.b[i]);
                    if (pos.getDistanceFrom(zeroPos) < pointRadius * 2.0f)
                    {
                        overInteractive = true;
                        break;
                    }
                }
            }
        }
        
        setMouseCursor(overInteractive ? juce::MouseCursor::DraggingHandCursor :
                                        juce::MouseCursor::NormalCursor);
    }
    
    float ZPlaneVisualizer::getFrequencyAtPoint(juce::Point<float> p) const
    {
        auto complex = screenToComplex(p);
        float angle = std::atan2(complex.imag(), complex.real());
        
        // Convert angle to frequency (0 to Nyquist)
        return std::abs(angle) / juce::MathConstants<float>::pi * 22050.0f;  // Assuming 44.1kHz sample rate
    }
    
    // OpenGL methods (stubs for now - implement if GPU acceleration needed)
    void ZPlaneVisualizer::newOpenGLContextCreated() {}
    void ZPlaneVisualizer::renderOpenGL() {}
    void ZPlaneVisualizer::openGLContextClosing() {}
}
