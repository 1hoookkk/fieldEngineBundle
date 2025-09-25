#include "ResonanceLoom.h"
#include "MorphEngineAudioProcessor.h"

#include <cmath>
#include <complex>
#include <limits>

#include <juce_audio_basics/juce_audio_basics.h>

// Debug flag for UI initialization tracking
constexpr bool kEnableUIDebug = true;

namespace
{
    constexpr float kDenominatorGuard = 1.0e-20f;
    constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
}

ResonanceMagnitudeSampler::ResonanceMagnitudeSampler()
{
    rebuildFrequencyTable();
}

void ResonanceMagnitudeSampler::setSampleRate (double newSampleRate) noexcept
{
    if (newSampleRate > 0.0 && std::abs (sampleRate - newSampleRate) > 1.0)
    {
        sampleRate = newSampleRate;
        frequencyTableDirty = true;
    }
}

void ResonanceMagnitudeSampler::setFrequencyRange (float minHz, float maxHz) noexcept
{
    minFrequency = juce::jmax (1.0f, juce::jmin (minHz, maxHz));
    maxFrequency = juce::jmax (minFrequency + 1.0f, maxHz);
    frequencyTableDirty = true;
}

void ResonanceMagnitudeSampler::setNumBins (int bins)
{
    const int clamped = juce::jlimit (16, 4096, bins);
    if (clamped == numBins)
        return;

    numBins = clamped;
    frequencyTableDirty = true;

    totalMagnitudes.resize ((size_t) numBins, 0.0f);
    sectionMagnitudes.resize ((size_t) numBins * (size_t) numSections, 0.0f);
}

bool ResonanceMagnitudeSampler::compute (const std::array<AuthenticEMUZPlane::BiquadCoeffs, 6>& coeffs)
{
    if (sampleRate <= 0.0 || numBins <= 0)
        return false;

    if (frequencyTableDirty)
        rebuildFrequencyTable();

    const float sr = (float) sampleRate;
    const size_t stride = (size_t) numSections;

    for (int i = 0; i < numBins; ++i)
    {
        const float f = frequencies[(size_t) i];
        const float omega = kTwoPi * f / sr;
        const float c = std::cos (omega);
        const float s = std::sin (omega);
        const float c2 = std::cos (omega + omega);
        const float s2 = std::sin (omega + omega);

        float total = 1.0f;
        for (int section = 0; section < numSections; ++section)
        {
            const auto& cf = coeffs[(size_t) section];
            const float Nr = cf.b0 + cf.b1 * c + cf.b2 * c2;
            const float Ni = - (cf.b1 * s + cf.b2 * s2);
            const float Dr = 1.0f + cf.a1 * c + cf.a2 * c2;
            const float Di = - (cf.a1 * s + cf.a2 * s2);

            const float num = Nr * Nr + Ni * Ni;
            const float den = juce::jmax (Dr * Dr + Di * Di, kDenominatorGuard);
            const float mag = std::sqrt (num / den);

            sectionMagnitudes[(size_t) i * stride + (size_t) section] = mag;
            total *= mag;
        }

        totalMagnitudes[(size_t) i] = total;
    }

#if JUCE_DEBUG
    if (numBins >= 4)
    {
        const int checkIndex = numBins / 2;
        const float f = frequencies[(size_t) checkIndex];
        const double omega = juce::MathConstants<double>::twoPi * (double) f / sampleRate;
        const double c = std::cos (omega);
        const double s = std::sin (omega);
        const double c2 = std::cos (2.0 * omega);
        const double s2 = std::sin (2.0 * omega);

        double totalRef = 1.0;
        for (int section = 0; section < numSections; ++section)
        {
            const auto& cf = coeffs[(size_t) section];
            const double Nr = cf.b0 + cf.b1 * c + cf.b2 * c2;
            const double Ni = - (cf.b1 * s + cf.b2 * s2);
            const double Dr = 1.0 + cf.a1 * c + cf.a2 * c2;
            const double Di = - (cf.a1 * s + cf.a2 * s2);
            const double den = juce::jmax (Dr * Dr + Di * Di, (double) kDenominatorGuard);
            const double mag = std::sqrt ((Nr * Nr + Ni * Ni) / den);
            totalRef *= mag;
        }

        const double diff = std::abs (totalRef - totalMagnitudes[(size_t) checkIndex]);
        jassert (diff <= 0.01);
    }
#endif

    return true;
}

void ResonanceMagnitudeSampler::rebuildFrequencyTable()
{
    frequencyTableDirty = false;

    frequencies.resize ((size_t) numBins);
    if (numBins <= 1)
    {
        if (! frequencies.empty())
            frequencies[0] = minFrequency;
        return;
    }

    const float logMin = std::log10 (minFrequency);
    const float logMax = std::log10 (maxFrequency);
    const float step = (logMax - logMin) / (float) (numBins - 1);

    for (int i = 0; i < numBins; ++i)
        frequencies[(size_t) i] = std::pow (10.0f, logMin + step * (float) i);

    totalMagnitudes.resize ((size_t) numBins, 0.0f);
    sectionMagnitudes.resize ((size_t) numBins * (size_t) numSections, 0.0f);
}

namespace
{
    constexpr float kFloorDb = -36.0f;
    constexpr float kCeilDb  = 12.0f;

    inline float normaliseDb (float magnitude)
    {
        const float db = juce::Decibels::gainToDecibels (juce::jmax (magnitude, 1.0e-6f));
        return juce::jlimit (0.0f, 1.0f, (db - kFloorDb) / (kCeilDb - kFloorDb));
    }
}

ResonanceLoom::ResonanceLoom (MorphEngineAudioProcessor& processorRef)
    : processor (processorRef)
{
    if constexpr (kEnableUIDebug) { DBG("ResonanceLoom: Constructor entry"); }

    const double sr = processorRef.getSampleRate();
    sampler.setSampleRate (sr > 0.0 ? sr : 48000.0);
    sampler.setFrequencyRange (20.0f, 20000.0f);
    sampler.setNumBins (targetBins);

    resetSmoothingBuffers();

    // Delay timer start until first valid frame to avoid race conditions
    // startTimerHz (60);  // Moved to pullFrame()

    if constexpr (kEnableUIDebug) { DBG("ResonanceLoom: Constructor complete, timer delayed"); }
}

ResonanceLoom::~ResonanceLoom()
{
    stopTimer();
}

void ResonanceLoom::setBins (int bins)
{
    targetBins = juce::jlimit (64, 4096, bins);
    sampler.setNumBins (targetBins);
    resetSmoothingBuffers();
}

void ResonanceLoom::setFrequencyRange (float minHz, float maxHz)
{
    sampler.setFrequencyRange (minHz, maxHz);
    resetSmoothingBuffers();
}

void ResonanceLoom::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    drawBackground (g, bounds);

    if (! frameValid)
        return;

    auto plotBounds = bounds.reduced (6.0f);
    drawSections (g, plotBounds);
    drawTotal (g, plotBounds);
    drawModeLegend (g, plotBounds);
}

void ResonanceLoom::resized()
{
}

void ResonanceLoom::timerCallback()
{
    if (kEnableUIDebug && !frameValid) { DBG("ResonanceLoom: Timer callback, frameValid=false"); }

    pullFrame();
}

void ResonanceLoom::pullFrame()
{
    double sr = processor.getSampleRate();
    if (sr <= 0.0)
        sr = 48000.0;

    sampler.setSampleRate (sr);
    currentSampleRate = sr;

    MorphEngineAudioProcessor::FilterFrame latest;
    if (! processor.getLatestFilterFrame (latest))
        return;

    const auto startTicks = juce::Time::getHighResolutionTicks();

    if (! sampler.compute (latest.coeffs))
        return;

    const auto endTicks = juce::Time::getHighResolutionTicks();
    lastComputeMs = juce::Time::highResolutionTicksToSeconds (endTicks - startTicks) * 1000.0;

    const double sinceAdapt = (lastAdaptTicks == 0)
        ? std::numeric_limits<double>::infinity()
        : juce::Time::highResolutionTicksToSeconds (endTicks - lastAdaptTicks);

    if (sinceAdapt > 0.5)
    {
        if (lastComputeMs > 12.0 && targetBins > 256)
        {
            targetBins = juce::jmax (256, targetBins / 2);
            sampler.setNumBins (targetBins);
            resetSmoothingBuffers();
            lastAdaptTicks = endTicks;
            frameValid = false;
            return;
        }

        if (lastComputeMs < 4.0 && targetBins < 2048)
        {
            targetBins = juce::jmin (2048, targetBins * 2);
            sampler.setNumBins (targetBins);
            resetSmoothingBuffers();
            lastAdaptTicks = endTicks;
            frameValid = false;
            return;
        }
    }

    const auto& total = sampler.getTotalMagnitudes();
    const auto& perSection = sampler.getSectionMagnitudes();

    if (smoothedTotal.size() != total.size())
        smoothedTotal.resize (total.size(), 0.0f);
    if (smoothedSections.size() != perSection.size())
        smoothedSections.resize (perSection.size(), 0.0f);

    const float alpha = smoothing;
    const float beta = 1.0f - alpha;

    for (size_t i = 0; i < total.size(); ++i)
    {
        const float target = total[i];
        if (smoothedTotal[i] == 0.0f)
            smoothedTotal[i] = target;
        else
            smoothedTotal[i] = alpha * smoothedTotal[i] + beta * target;
    }

    for (size_t i = 0; i < perSection.size(); ++i)
    {
        const float target = perSection[i];
        if (smoothedSections[i] == 0.0f)
            smoothedSections[i] = target;
        else
            smoothedSections[i] = alpha * smoothedSections[i] + beta * target;
    }

    frame = latest;
    frameValid = true;

    // Start timer on first successful frame
    if (!timerStarted)
    {
        if constexpr (kEnableUIDebug) { DBG("ResonanceLoom: Starting timer after first valid frame"); }
        startTimerHz(60);
        timerStarted = true;
    }

    repaint();
}

void ResonanceLoom::resetSmoothingBuffers()
{
    smoothedTotal.assign (sampler.getFrequencies().size(), 0.0f);
    smoothedSections.assign (sampler.getFrequencies().size() * (size_t) sampler.getNumSections(), 0.0f);
}

void ResonanceLoom::drawBackground (juce::Graphics& g, const juce::Rectangle<float>& bounds) const
{
    g.setColour (juce::Colour::fromRGB (14, 18, 24));
    g.fillRoundedRectangle (bounds, 6.0f);

    if (sampler.getFrequencies().size() < 2)
        return;

    const auto logMin = std::log10 (sampler.getFrequencies().front());
    const auto logMax = std::log10 (sampler.getFrequencies().back());

    auto toX = [&](float hz)
    {
        const float clamped = juce::jlimit (sampler.getFrequencies().front(), sampler.getFrequencies().back(), hz);
        const float logF = std::log10 (clamped);
        const float norm = (logF - logMin) / (logMax - logMin);
        return bounds.getX() + norm * bounds.getWidth();
    };

    g.setColour (juce::Colour::fromFloatRGBA (0.35f, 0.42f, 0.50f, 0.18f));
    const std::array<float, 10> freqLines { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
    for (float hz : freqLines)
    {
        const float x = toX (hz);
        g.drawLine (x, bounds.getY() + 4.0f, x, bounds.getBottom() - 4.0f, 0.6f);
    }

    g.setColour (juce::Colour::fromFloatRGBA (0.8f, 0.8f, 0.85f, 0.08f));
    for (float db = kFloorDb; db <= kCeilDb; db += 6.0f)
    {
        const float norm = juce::jlimit (0.0f, 1.0f, (db - kFloorDb) / (kCeilDb - kFloorDb));
        const float y = bounds.getBottom() - norm * bounds.getHeight();
        g.drawLine (bounds.getX() + 4.0f, y, bounds.getRight() - 4.0f, y, db == 0.0f ? 1.2f : 0.6f);
    }

    g.setColour (juce::Colour::fromFloatRGBA (0.78f, 0.82f, 0.90f, 0.55f));
    g.setFont (juce::Font (11.0f, juce::Font::bold));

    const std::array<float, 4> freqLabels { 20.0f, 200.0f, 2000.0f, 20000.0f };
    for (float hz : freqLabels)
    {
        const float x = toX (hz);
        juce::String text = (hz >= 1000.0f) ? juce::String (hz / 1000.0f, 1) + "k" : juce::String ((int) hz);
        auto labelBounds = juce::Rectangle<float> (x - 18.0f, bounds.getBottom() - 18.0f, 36.0f, 16.0f);
        g.drawFittedText (text, labelBounds.toNearestInt(), juce::Justification::centred, 1);
    }

    g.setFont (juce::Font (10.0f));
    const std::array<float, 3> gainLabels { 0.0f, -12.0f, -24.0f };
    for (float db : gainLabels)
    {
        const float norm = juce::jlimit (0.0f, 1.0f, (db - kFloorDb) / (kCeilDb - kFloorDb));
        const float y = bounds.getBottom() - norm * bounds.getHeight();
        juce::String text = (db > -0.5f) ? juce::String ("0 dB") : juce::String (db, 0) + " dB";
        auto textBounds = juce::Rectangle<float> (bounds.getX() + 6.0f, y - 8.0f, 48.0f, 16.0f);
        g.drawFittedText (text, textBounds.toNearestInt(), juce::Justification::left, 1);
    }
}

void ResonanceLoom::drawSections (juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    const auto& freqs = sampler.getFrequencies();
    const int sections = sampler.getNumSections();
    if (freqs.size() < 2 || smoothedSections.empty())
        return;

    const float logMin = std::log10 (freqs.front());
    const float logMax = std::log10 (freqs.back());
    const float width = bounds.getWidth();
    const float height = bounds.getHeight();

    auto toX = [&](float hz)
    {
        const float logF = std::log10 (hz);
        return bounds.getX() + ((logF - logMin) / (logMax - logMin)) * width;
    };

    auto toY = [&](float magnitude)
    {
        const float norm = normaliseDb (magnitude);
        return bounds.getBottom() - norm * height;
    };

    for (int section = 0; section < sections; ++section)
    {
        juce::Path path;
        bool started = false;

        for (size_t i = 0; i < freqs.size(); ++i)
        {
            const float mag = smoothedSections[i * (size_t) sections + (size_t) section];
            const float x = toX (freqs[i]);
            const float y = toY (mag);

            if (! started)
            {
                path.startNewSubPath (x, y);
                started = true;
            }
            else
            {
                path.lineTo (x, y);
            }
        }

        if (path.isEmpty())
            continue;

        const float r = juce::jlimit (0.05f, 0.995f, frame.poles[(size_t) section].r);
        const float qProxy = 1.0f / juce::jmax (0.05f, 1.0f - r);
        const float thickness = juce::jlimit (1.2f, 6.0f, 1.1f + 0.45f * qProxy);

        g.setColour (getSectionColour (section).withAlpha (0.65f));
        g.strokePath (path, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void ResonanceLoom::drawTotal (juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    const auto& freqs = sampler.getFrequencies();
    if (freqs.size() < 2 || smoothedTotal.empty())
        return;

    const float logMin = std::log10 (freqs.front());
    const float logMax = std::log10 (freqs.back());
    const float width = bounds.getWidth();
    const float height = bounds.getHeight();

    auto toX = [&](float hz)
    {
        const float logF = std::log10 (hz);
        return bounds.getX() + ((logF - logMin) / (logMax - logMin)) * width;
    };

    auto toY = [&](float magnitude)
    {
        const float norm = normaliseDb (magnitude);
        return bounds.getBottom() - norm * height;
    };

    juce::Path curve;
    bool started = false;
    for (size_t i = 0; i < freqs.size(); ++i)
    {
        const float x = toX (freqs[i]);
        const float y = toY (smoothedTotal[i]);
        if (! started)
        {
            curve.startNewSubPath (x, y);
            started = true;
        }
        else
        {
            curve.lineTo (x, y);
        }
    }

    if (curve.isEmpty())
        return;

    juce::Path fillPath (curve);
    fillPath.lineTo (bounds.getRight(), bounds.getBottom());
    fillPath.lineTo (bounds.getX(), bounds.getBottom());
    fillPath.closeSubPath();

    g.setColour (juce::Colour::fromFloatRGBA (0.1f, 0.8f, 0.9f, 0.12f));
    g.fillPath (fillPath);

    g.setColour (juce::Colour::fromFloatRGBA (0.1f, 0.9f, 1.0f, 0.8f));
    g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Colour ResonanceLoom::getSectionColour (int index) const
{
    static const juce::Colour palette[]
    {
        juce::Colour::fromRGB (68,   1,  84),
        juce::Colour::fromRGB (65,  68, 135),
        juce::Colour::fromRGB (42, 120, 142),
        juce::Colour::fromRGB (34, 168, 132),
        juce::Colour::fromRGB (122, 209,  81),
        juce::Colour::fromRGB (253, 231,  37)
    };

    return palette[index % (int) (sizeof (palette) / sizeof (juce::Colour))];
}

void ResonanceLoom::drawModeLegend (juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    const int sections = sampler.getNumSections();
    if (! frameValid || sections <= 0)
        return;

    auto legend = juce::Rectangle<float> (bounds.getX() + 10.0f,
                                          bounds.getY() + 10.0f,
                                          bounds.getWidth() - 20.0f,
                                          60.0f);

    g.setColour (juce::Colour::fromFloatRGBA (0.05f, 0.08f, 0.12f, 0.55f));
    g.fillRoundedRectangle (legend, 5.0f);

    const int rows = 3;
    const int columns = (sections + rows - 1) / rows;
    const float rowHeight = legend.getHeight() / (float) rows;
    const float columnWidth = legend.getWidth() / (float) columns;

    g.setFont (juce::Font (10.0f));

    for (int section = 0; section < sections; ++section)
    {
        const int column = section / rows;
        const int row = section % rows;

        juce::Rectangle<float> cell (
            legend.getX() + column * columnWidth,
            legend.getY() + row * rowHeight,
            columnWidth,
            rowHeight);

        const auto colour = getSectionColour (section);
        auto swatch = cell.removeFromLeft (14.0f).reduced (2.0f, 4.0f);
        g.setColour (colour.withAlpha (0.9f));
        g.fillRoundedRectangle (swatch, 2.0f);

        const float r = juce::jlimit (0.05f, 0.999f, frame.poles[(size_t) section].r);
        const float theta = frame.poles[(size_t) section].theta;
        const float oneMinusR = juce::jmax (0.0005f, 1.0f - r);

        const float centreHz = juce::jlimit (20.0f, 20000.0f,
                                             (float) (theta * (float) currentSampleRate /
                                                      juce::MathConstants<float>::twoPi));
        const float widthHz = juce::jlimit (5.0f, 5000.0f,
                                            oneMinusR * (float) currentSampleRate /
                                            juce::MathConstants<float>::pi);
        const float decayMs = juce::jlimit (15.0f, 2000.0f,
                                            1000.0f * 6.91f /
                                            ((float) currentSampleRate * oneMinusR));

        juce::String text;
        text << "Mode " << (section + 1) << "  ";
        if (centreHz >= 1000.0f)
            text << juce::String (centreHz / 1000.0f, 2) << "k";
        else
            text << juce::String (centreHz, 0);
        text << " Hz  Width " << juce::String (widthHz, 0) << " Hz  Decay "
             << juce::String (decayMs, 0) << " ms";

        g.setColour (juce::Colours::white.withAlpha (0.78f));
        g.drawFittedText (text, cell.toNearestInt(), juce::Justification::centredLeft, 1);
    }
}

