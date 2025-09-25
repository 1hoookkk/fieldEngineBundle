#include "ColorToSpectralMapper.h"
#include <algorithm>
#include <cmath>

ColorToSpectralMapper::ColorAnalysis::ColorAnalysis(juce::Colour color)
    : originalColor(color)
{
    hue = color.getHue();
    saturation = color.getSaturation();
    brightness = color.getBrightness();
    
    chroma = saturation * brightness;
    
    // Color temperature (warm = 0, cool = 1)
    if (hue < 0.08f || hue > 0.92f)
        colorTemperature = 0.0f;  // Red region
    else if (hue >= 0.08f && hue < 0.33f)
        colorTemperature = 0.3f;  // Yellow-Green region  
    else if (hue >= 0.33f && hue < 0.66f)
        colorTemperature = 1.0f;  // Blue-Cyan region
    else
        colorTemperature = 0.6f;  // Purple-Red region
    
    colorEnergy = (saturation * 0.6f + brightness * 0.4f) * (1.0f + chroma * 0.5f);
    
    isPrimaryColor = (std::abs(hue - 0.0f) < 0.05f ||      // Red
                     std::abs(hue - 0.33f) < 0.05f ||      // Green
                     std::abs(hue - 0.66f) < 0.05f) &&     // Blue
                     saturation > 0.7f;
}

ColorToSpectralMapper::ColorToSpectralMapper()
{
    setMappingMode(MappingMode::HueToEffect);
    initializeGlobalParameterScaling();
    
    smoothedColor = juce::Colours::black;
    previousColor = juce::Colours::black;
}

void ColorToSpectralMapper::setMappingMode(MappingMode mode)
{
    currentMappingMode = mode;
}

ColorToSpectralMapper::SpectralParameters ColorToSpectralMapper::mapColorToSpectralParameters(
    juce::Colour color,
    float pressure,
    float velocity,
    float positionX,
    float positionY)
{
    ColorAnalysis analysis = analyzeColor(color);
    SpectralParameters params;
    
    // Basic mapping based on current mode
    switch (currentMappingMode)
    {
        case MappingMode::HueToEffect:
        {
            // Hue → frequency (20Hz - 20kHz)
            params.frequency = 20.0f + analysis.hue * 19980.0f;
            
            // Saturation → filter cutoff
            params.filterCutoff = analysis.saturation;
            
            // Brightness → amplitude
            params.amplitude = analysis.brightness;
            
            // Color properties → spectral effects
            params.spectralBlur = analysis.chroma * 0.5f;
            params.harmonicContent = analysis.colorEnergy;
            break;
        }
        
        case MappingMode::SaturationDriven:
        {
            params.frequency = 440.0f * std::pow(2.0f, (analysis.hue - 0.5f) * 4.0f); // ±2 octaves
            params.amplitude = analysis.saturation;
            params.filterCutoff = analysis.brightness;
            params.spectralBlur = analysis.chroma;
            params.harmonicContent = analysis.colorEnergy;
            break;
        }
        
        case MappingMode::ProBeatmaker:
        {
            // Optimized for electronic music production
            params.frequency = 55.0f * std::pow(2.0f, analysis.hue * 7.0f); // Musical scale
            params.amplitude = std::pow(analysis.saturation, 0.7f); // More sensitive
            params.filterCutoff = analysis.brightness;
            params.resonance = analysis.chroma * 0.8f;
            params.spectralBlur = analysis.colorEnergy * 0.3f;
            params.harmonicContent = analysis.brightness * analysis.saturation;
            break;
        }
        
        case MappingMode::Experimental:
        {
            // Maximum creative freedom
            params.frequency = 20.0f * std::pow(100.0f, analysis.hue); // Exponential frequency
            params.amplitude = analysis.colorEnergy;
            params.filterCutoff = analysis.chroma;
            params.resonance = analysis.saturation;
            params.spectralBlur = analysis.brightness * 0.7f;
            params.spectralShift = (analysis.hue - 0.5f) * 0.5f;
            params.harmonicContent = analysis.colorTemperature;
            break;
        }
        
        default:
            params.frequency = 440.0f * std::pow(2.0f, (analysis.hue - 0.5f) * 2.0f);
            params.amplitude = analysis.brightness;
            params.filterCutoff = analysis.saturation;
            break;
    }
    
    // Apply spatial positioning
    params.positionX = positionX;
    params.positionY = positionY;
    params.pan = positionX; // X position controls stereo pan
    
    // Apply pressure and velocity
    params.pressure = pressure;
    params.velocity = velocity;
    
    // Apply global scaling from parameter scaling settings
    auto intensityScaling = getParameterScaling("intensity");
    params.amplitude *= intensityScaling.applyScaling(pressure);
    
    auto frequencyScaling = getParameterScaling("frequency");
    if (frequencyScaling.curveType != MusicalScaling::CurveType::Linear)
    {
        float normalizedFreq = (params.frequency - 20.0f) / 19980.0f;
        normalizedFreq = frequencyScaling.applyScaling(normalizedFreq);
        params.frequency = 20.0f + normalizedFreq * 19980.0f;
    }
    
    // Validate parameters
    params.frequency = juce::jlimit(20.0f, 20000.0f, params.frequency);
    params.amplitude = juce::jlimit(0.0f, 1.0f, params.amplitude);
    params.filterCutoff = juce::jlimit(0.0f, 1.0f, params.filterCutoff);
    params.resonance = juce::jlimit(0.0f, 1.0f, params.resonance);
    params.spectralBlur = juce::jlimit(0.0f, 1.0f, params.spectralBlur);
    params.harmonicContent = juce::jlimit(0.0f, 1.0f, params.harmonicContent);
    params.pan = juce::jlimit(0.0f, 1.0f, params.pan);
    
    return params;
}

ColorToSpectralMapper::ColorAnalysis ColorToSpectralMapper::analyzeColor(juce::Colour color)
{
    return ColorAnalysis(color);
}

float ColorToSpectralMapper::MusicalScaling::applyScaling(float normalizedValue) const
{
    float value = juce::jlimit(0.0f, 1.0f, normalizedValue);
    
    if (invertMapping)
        value = 1.0f - value;
    
    float scaledValue = 0.0f;
    
    switch (curveType)
    {
        case CurveType::Linear:
            scaledValue = value;
            break;
            
        case CurveType::Exponential:
            scaledValue = std::pow(value, curveFactor);
            break;
            
        case CurveType::Logarithmic:
            if (value > 0.0f)
                scaledValue = std::log(1.0f + value * (std::exp(curveFactor) - 1.0f)) / curveFactor;
            break;
            
        case CurveType::S_Curve:
            scaledValue = 1.0f / (1.0f + std::exp(-curveFactor * (value - 0.5f)));
            scaledValue = (scaledValue - 1.0f / (1.0f + std::exp(curveFactor * 0.5f))) /
                         (1.0f / (1.0f + std::exp(-curveFactor * 0.5f)) - 1.0f / (1.0f + std::exp(curveFactor * 0.5f)));
            break;
            
        case CurveType::Musical:
            if (curveFactor <= 12.0f) // Semitone mapping
            {
                int semitones = static_cast<int>(value * curveFactor);
                scaledValue = semitones / curveFactor;
            }
            else // Harmonic ratio mapping
            {
                scaledValue = std::pow(2.0f, value * std::log2(curveFactor));
            }
            break;
    }
    
    return minValue + scaledValue * (maxValue - minValue);
}

void ColorToSpectralMapper::setParameterScaling(const std::string& parameterName, const MusicalScaling& scaling)
{
    globalParameterScaling[parameterName] = scaling;
}

ColorToSpectralMapper::MusicalScaling ColorToSpectralMapper::getParameterScaling(const std::string& parameterName) const
{
    auto it = globalParameterScaling.find(parameterName);
    if (it != globalParameterScaling.end())
        return it->second;
    
    MusicalScaling defaultScaling;
    return defaultScaling;
}

void ColorToSpectralMapper::updateColorProcessingState(juce::Colour newColor)
{
    float colorDistance = calculatePerceptualColorDistance(newColor, previousColor);
    
    if (colorSmoothingTimeMs > 0.0f && colorDistance > colorChangeThreshold)
    {
        float smoothingFactor = 0.1f; // Simplified smoothing
        smoothedColor = smoothColor(newColor, smoothedColor, smoothingFactor);
    }
    else
    {
        smoothedColor = newColor;
    }
    
    previousColor = newColor;
}

float ColorToSpectralMapper::calculatePerceptualColorDistance(juce::Colour color1, juce::Colour color2)
{
    // Simplified perceptual distance using HSB space
    float h1 = color1.getHue(), s1 = color1.getSaturation(), b1 = color1.getBrightness();
    float h2 = color2.getHue(), s2 = color2.getSaturation(), b2 = color2.getBrightness();
    
    float hueDiff = calculateHueDistance(h1, h2);
    float satDiff = std::abs(s1 - s2);
    float brightDiff = std::abs(b1 - b2);
    
    return std::sqrt(hueDiff * hueDiff + satDiff * satDiff + brightDiff * brightDiff);
}

float ColorToSpectralMapper::calculateHueDistance(float hue1, float hue2)
{
    float diff = std::abs(hue1 - hue2);
    return std::min(diff, 1.0f - diff);
}

juce::Colour ColorToSpectralMapper::smoothColor(juce::Colour targetColor, juce::Colour currentColor, float smoothingFactor)
{
    float currentH = currentColor.getHue();
    float currentS = currentColor.getSaturation();
    float currentB = currentColor.getBrightness();
    
    float targetH = targetColor.getHue();
    float targetS = targetColor.getSaturation();
    float targetB = targetColor.getBrightness();
    
    // Handle hue wraparound
    float hueDiff = targetH - currentH;
    if (hueDiff > 0.5f) hueDiff -= 1.0f;
    if (hueDiff < -0.5f) hueDiff += 1.0f;
    
    float smoothedH = currentH + hueDiff * smoothingFactor;
    float smoothedS = currentS + (targetS - currentS) * smoothingFactor;
    float smoothedB = currentB + (targetB - currentB) * smoothingFactor;
    
    if (smoothedH < 0.0f) smoothedH += 1.0f;
    if (smoothedH > 1.0f) smoothedH -= 1.0f;
    
    return juce::Colour(smoothedH, smoothedS, smoothedB, 1.0f);
}

void ColorToSpectralMapper::initializeGlobalParameterScaling()
{
    MusicalScaling intensityScaling;
    intensityScaling.curveType = MusicalScaling::CurveType::S_Curve;
    intensityScaling.curveFactor = 3.0f;
    globalParameterScaling["intensity"] = intensityScaling;
    
    MusicalScaling frequencyScaling;
    frequencyScaling.curveType = MusicalScaling::CurveType::Exponential;
    frequencyScaling.curveFactor = 2.0f;
    globalParameterScaling["frequency"] = frequencyScaling;
    
    MusicalScaling timeScaling;
    timeScaling.curveType = MusicalScaling::CurveType::Logarithmic;
    timeScaling.curveFactor = 3.0f;
    globalParameterScaling["time"] = timeScaling;
}

float ColorToSpectralMapper::calculateChroma(float hue, float saturation, float brightness)
{
    return saturation * brightness;
}

float ColorToSpectralMapper::calculateColorTemperature(float hue)
{
    if (hue < 0.08f || hue > 0.92f)
        return 0.0f;  // Warm (red)
    else if (hue >= 0.33f && hue < 0.66f)
        return 1.0f;  // Cool (blue)
    else
        return 0.5f;  // Neutral
}

float ColorToSpectralMapper::calculateColorEnergy(float saturation, float brightness)
{
    return (saturation * 0.6f + brightness * 0.4f) * (1.0f + saturation * brightness * 0.5f);
}