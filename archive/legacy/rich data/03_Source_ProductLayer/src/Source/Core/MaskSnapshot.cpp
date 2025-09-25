#include "MaskSnapshot.h"

MaskSnapshot::MaskSnapshot()
{
    // Allocate buffers on construction (not in RT thread)
    workBuffer = std::make_unique<MaskData>();
    pendingBuffer = std::make_unique<MaskData>();
    audioBuffer1 = std::make_unique<MaskData>();
    audioBuffer2 = std::make_unique<MaskData>();
    
    // Initialize with buffer1 as current snapshot
    currentSnapshot.store(audioBuffer1.get(), std::memory_order_release);
    
    DBG("MaskSnapshot initialized with " << MASK_WIDTH << "x" << MASK_HEIGHT << " resolution");
}

MaskSnapshot::~MaskSnapshot()
{
    // Ensure no dangling pointers
    currentSnapshot.store(nullptr, std::memory_order_release);
}

void MaskSnapshot::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    this->samplesPerBlock = samplesPerBlock;
    
    // Initialize frequency scaling in all buffers
    auto initializeBuffer = [sampleRate](MaskData* buffer) {
        if (buffer)
        {
            buffer->timeScale = 1.0f;
            buffer->freqScale = 1.0f;  
            buffer->minFreq = 20.0f;
            buffer->maxFreq = sampleRate / 3.0f; // Nyquist limit with safety margin
        }
    };
    
    initializeBuffer(workBuffer.get());
    initializeBuffer(pendingBuffer.get());
    initializeBuffer(audioBuffer1.get());
    initializeBuffer(audioBuffer2.get());
}

float MaskSnapshot::sampleMask(float timeNorm, float frequencyHz, double sampleRate) const noexcept
{
    // RT-SAFE: Get current snapshot with memory barrier
    const MaskData* snapshot = currentSnapshot.load(std::memory_order_acquire);
    if (!snapshot)
        return 1.0f; // No masking if no snapshot
    
    // Convert frequency to Y coordinate (logarithmic mapping)
    float y = frequencyToY(frequencyHz, snapshot);
    
    // Convert time to X coordinate  
    float x = timeToX(timeNorm);
    
    // Get base mask value via bilinear sampling
    float maskValue = snapshot->sampleBilinear(x, y);
    
    // Apply parameters (RT-safe atomic loads)
    float blend = maskBlend.load(std::memory_order_acquire);
    float strength = maskStrength.load(std::memory_order_acquire);
    float thresh = threshold.load(std::memory_order_acquire);
    
    // Apply threshold (use pre-computed linear value - RT-SAFE)
    float thresholdLinear = thresh; // Assuming thresh is already linear
    if (maskValue > thresholdLinear)
    {
        // Apply feathering near threshold
        float featherRange = 0.1f; // 10% feather range
        float featherFactor = juce::jlimit(0.0f, 1.0f, (maskValue - thresholdLinear) / featherRange);
        maskValue = thresholdLinear + featherFactor * (maskValue - thresholdLinear);
    }
    
    // Apply strength and blend
    maskValue = juce::jlimit(0.0f, 1.0f, maskValue * strength);
    return juce::jlimit(0.0f, 1.0f, (1.0f - blend) + blend * maskValue);
}

void MaskSnapshot::commitWorkBuffer() noexcept
{
    // NON-RT: This runs on GUI thread
    
    // Copy work buffer to pending buffer
    if (workBuffer && pendingBuffer)
    {
        std::memcpy(pendingBuffer->maskValues, workBuffer->maskValues, sizeof(workBuffer->maskValues));
        pendingBuffer->timeScale = workBuffer->timeScale;
        pendingBuffer->freqScale = workBuffer->freqScale;
        pendingBuffer->minFreq = workBuffer->minFreq;
        pendingBuffer->maxFreq = workBuffer->maxFreq;
        pendingBuffer->featherTime = featherTime.load(std::memory_order_acquire);
        pendingBuffer->featherFreq = featherFreq.load(std::memory_order_acquire);
        pendingBuffer->threshold = threshold.load(std::memory_order_acquire);
        pendingBuffer->protectHarmonics = protectHarmonics.load(std::memory_order_acquire);
        pendingBuffer->timestamp = juce::Time::getMillisecondCounterHiRes();
    }
    
    // Atomic pointer swap: determine which audio buffer is not current
    const MaskData* current = currentSnapshot.load(std::memory_order_acquire);
    MaskData* nextBuffer = (current == audioBuffer1.get()) ? audioBuffer2.get() : audioBuffer1.get();
    
    // Copy pending to next audio buffer
    if (pendingBuffer && nextBuffer)
    {
        std::memcpy(nextBuffer, pendingBuffer.get(), sizeof(MaskData));
    }
    
    // Atomic swap: audio thread will see new snapshot
    currentSnapshot.store(nextBuffer, std::memory_order_release);
    
    // Update statistics
    statistics.swapCount++;
    statistics.lastSwapTime = juce::Time::getMillisecondCounterHiRes();
    
    // RT-SAFE: Debug logging removed
}

void MaskSnapshot::clearWorkBuffer() noexcept
{
    if (workBuffer)
    {
        std::fill(std::begin(workBuffer->maskValues), std::end(workBuffer->maskValues), 1.0f);
        workBuffer->timestamp = juce::Time::getMillisecondCounterHiRes();
    }
}

void MaskSnapshot::paintCircle(float centerX, float centerY, float radius, float value)
{
    if (!workBuffer)
        return;
    
    // Convert normalized coordinates to pixel coordinates
    int pixelX = int(centerX * MASK_WIDTH);
    int pixelY = int(centerY * MASK_HEIGHT);
    int pixelRadius = int(radius * juce::jmin(MASK_WIDTH, MASK_HEIGHT));
    
    // Paint circle with anti-aliasing
    int minX = juce::jmax(0, pixelX - pixelRadius - 1);
    int maxX = juce::jmin(MASK_WIDTH - 1, pixelX + pixelRadius + 1);
    int minY = juce::jmax(0, pixelY - pixelRadius - 1);
    int maxY = juce::jmin(MASK_HEIGHT - 1, pixelY + pixelRadius + 1);
    
    for (int y = minY; y <= maxY; ++y)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            float dx = float(x - pixelX);
            float dy = float(y - pixelY);
            float distSq = dx * dx + dy * dy;  // Use squared distance
            float radiusSq = float(pixelRadius * pixelRadius);
            
            if (distSq <= radiusSq)
            {
                // Anti-aliasing using squared distance to avoid sqrt
                float alpha = 1.0f;
                if (distSq > (pixelRadius - 0.5f) * (pixelRadius - 0.5f))
                {
                    // Feather edge: w = saturate((radius^2 - dist^2) * invFeather^2)
                    const float featherRange = 1.0f;  // 1 pixel feather
                    const float invFeatherSq = 1.0f / (featherRange * featherRange);
                    alpha = juce::jlimit(0.0f, 1.0f, (radiusSq - distSq) * invFeatherSq);
                }
                
                float currentValue = workBuffer->getMaskValue(x, y);
                float newValue = currentValue + alpha * (value - currentValue);
                workBuffer->setMaskValue(x, y, newValue);
            }
        }
    }
}

void MaskSnapshot::paintRectangle(float x, float y, float width, float height, float value)
{
    if (!workBuffer)
        return;
    
    // Convert normalized coordinates to pixel coordinates
    int pixelX = int(x * MASK_WIDTH);
    int pixelY = int(y * MASK_HEIGHT);
    int pixelWidth = int(width * MASK_WIDTH);
    int pixelHeight = int(height * MASK_HEIGHT);
    
    int minX = juce::jmax(0, pixelX);
    int maxX = juce::jmin(MASK_WIDTH - 1, pixelX + pixelWidth);
    int minY = juce::jmax(0, pixelY);
    int maxY = juce::jmin(MASK_HEIGHT - 1, pixelY + pixelHeight);
    
    for (int py = minY; py <= maxY; ++py)
    {
        for (int px = minX; px <= maxX; ++px)
        {
            workBuffer->setMaskValue(px, py, value);
        }
    }
}

void MaskSnapshot::paintLine(float x1, float y1, float x2, float y2, float lineWidth, float value)
{
    if (!workBuffer)
        return;
    
    // Convert to pixel coordinates
    int px1 = int(x1 * MASK_WIDTH);
    int py1 = int(y1 * MASK_HEIGHT);
    int px2 = int(x2 * MASK_WIDTH);
    int py2 = int(y2 * MASK_HEIGHT);
    
    // Bresenham's line algorithm with width
    int dx = std::abs(px2 - px1);
    int dy = std::abs(py2 - py1);
    int sx = (px1 < px2) ? 1 : -1;
    int sy = (py1 < py2) ? 1 : -1;
    int err = dx - dy;
    
    int halfWidth = int(lineWidth * juce::jmin(MASK_WIDTH, MASK_HEIGHT) * 0.5f);
    
    int x = px1;
    int y = py1;
    
    while (true)
    {
        // Paint circle at current point
        for (int dy = -halfWidth; dy <= halfWidth; ++dy)
        {
            for (int dx = -halfWidth; dx <= halfWidth; ++dx)
            {
                if (dx * dx + dy * dy <= halfWidth * halfWidth)
                {
                    int paintX = x + dx;
                    int paintY = y + dy;
                    if (paintX >= 0 && paintX < MASK_WIDTH && paintY >= 0 && paintY < MASK_HEIGHT)
                    {
                        workBuffer->setMaskValue(paintX, paintY, value);
                    }
                }
            }
        }
        
        if (x == px2 && y == py2)
            break;
        
        int e2 = 2 * err;
        if (e2 > -dy)
        {
            err -= dy;
            x += sx;
        }
        if (e2 < dx)
        {
            err += dx;
            y += sy;
        }
    }
}

inline float MaskSnapshot::frequencyToY(float frequencyHz, const MaskData* snapshot) const noexcept
{
    if (!snapshot)
        return 0.0f;
    
    // Logarithmic frequency mapping
    float minFreq = snapshot->minFreq;
    float maxFreq = snapshot->maxFreq;
    
    if (frequencyHz <= minFreq)
        return 0.0f;
    if (frequencyHz >= maxFreq)
        return float(MASK_HEIGHT - 1);
    
    // RT-SAFE log mapping using fast approximation
    float logRatio = fastLog2(frequencyHz / minFreq) / fastLog2(maxFreq / minFreq);
    return logRatio * float(MASK_HEIGHT - 1);
}

inline float MaskSnapshot::timeToX(float timeNorm) const noexcept
{
    return juce::jlimit(0.0f, float(MASK_WIDTH - 1), timeNorm * float(MASK_WIDTH - 1));
}

void MaskSnapshot::updateStatistics() const noexcept
{
    // Count active mask pixels (non-1.0 values)
    const MaskData* snapshot = currentSnapshot.load(std::memory_order_acquire);
    if (snapshot)
    {
        int activePixels = 0;
        for (int i = 0; i < MASK_SIZE; ++i)
        {
            if (std::abs(snapshot->maskValues[i] - 1.0f) > 0.001f)
                activePixels++;
        }
        statistics.activeMaskPixels = activePixels;
    }
}