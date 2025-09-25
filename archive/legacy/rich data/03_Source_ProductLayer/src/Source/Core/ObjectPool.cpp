#include "ObjectPool.h"
#include "SpectralMask.h"
#include "PaintEngine.h"
#include <memory>

namespace Pools
{
    // Global pool instances
    SpectralFramePool* spectralFramePool = nullptr;
    StrokePool* strokePool = nullptr;
    CanvasRegionPool* canvasRegionPool = nullptr;
    
    void initializePools()
    {
        DBG("Initializing global object pools...");
        
        // Initialize pools with appropriate sizes for different object types
        spectralFramePool = new SpectralFramePool();  // 128 SpectralFrame objects
        strokePool = new StrokePool();                // 32 Stroke objects  
        canvasRegionPool = new CanvasRegionPool();    // 16 CanvasRegion objects
        
        DBG("Global object pools initialized successfully");
    }
    
    void shutdownPools()
    {
        DBG("Shutting down global object pools...");
        
        delete spectralFramePool;
        spectralFramePool = nullptr;
        
        delete strokePool;
        strokePool = nullptr;
        
        delete canvasRegionPool;
        canvasRegionPool = nullptr;
        
        DBG("Global object pools shut down");
    }
    
    void resetAllStatistics()
    {
        if (spectralFramePool)
            spectralFramePool->resetStatistics();
            
        if (strokePool)
            strokePool->resetStatistics();
            
        if (canvasRegionPool)
            canvasRegionPool->resetStatistics();
            
        DBG("All pool statistics reset");
    }
}