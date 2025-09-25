#include "PaintEngine.h"

PaintEngine::PaintEngine() = default;
PaintEngine::~PaintEngine() = default;

void PaintEngine::prepareToPlay(double sr, int samplesPerBlock) 
{
    sampleRate = sr;
}

void PaintEngine::processBlock(juce::AudioBuffer<float>& buffer)
{
    // Basic passthrough - paint functionality will be added incrementally
}

void PaintEngine::releaseResources() {}

void PaintEngine::beginStroke(Point position, float pressure, juce::Colour color) {}
void PaintEngine::updateStroke(Point position, float pressure) {}  
void PaintEngine::endStroke() {}

void PaintEngine::setPlayheadPosition(float normalisedPosition) {}
void PaintEngine::setCanvasRegion(float leftX, float rightX, float bottomY, float topY) {}
void PaintEngine::clearCanvas() {}
void PaintEngine::setMasterGain(float gain) {}
void PaintEngine::setFrequencyRange(float minHz, float maxHz) {}
float PaintEngine::canvasYToFrequency(float y) const { return 440.0f; }