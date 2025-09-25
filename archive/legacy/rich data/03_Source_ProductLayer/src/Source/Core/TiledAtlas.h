#pragma once
#include "AtlasIds.h"
#include "RealtimeSafeTypes.h"
#include <atomic>
#include <array>
#include <memory>
#include <cstring>

/**
 * Tiled spectral atlas with atomic page flipping.
 * Supports tear-free updates for real-time spectral visualization.
 */
class TiledAtlas {
public:
    using PageData = std::array<float, AtlasConfig::ATLAS_WIDTH * AtlasConfig::ATLAS_HEIGHT>;
    
    TiledAtlas() noexcept;
    ~TiledAtlas() noexcept = default;
    
    // Non-copyable for RT safety
    TiledAtlas(const TiledAtlas&) = delete;
    TiledAtlas& operator=(const TiledAtlas&) = delete;
    
    // Initialization (main thread only - may allocate)
    bool initialize() noexcept;
    void shutdown() noexcept;
    
    // Page management (background thread)
    AtlasPageHandle allocateFreePage() noexcept;
    void releasePage(AtlasPageHandle handle) noexcept;
    bool isPageReady(AtlasPageHandle handle) const noexcept;
    
    // Column writing (background thread)
    bool writeColumn(AtlasPageHandle pageHandle, 
                    const AtlasPosition& position,
                    const float* columnData,
                    size_t numBins) noexcept;
    
    // Atomic page flipping (audio thread → GPU thread)
    void activatePage(AtlasPageHandle handle) noexcept;
    AtlasPageHandle getActivePage() const noexcept;
    AtlasPageHandle getPendingPage() const noexcept;
    
    // GPU texture access (GPU thread only)
    const float* getActivePageData() const noexcept;
    const float* getPageData(AtlasPageHandle handle) const noexcept;
    bool hasPageUpdate() const noexcept;
    void acknowledgePageUpdate() noexcept;
    
    // Column reading (any thread)
    bool readColumn(AtlasPageHandle pageHandle,
                   const AtlasPosition& position,
                   float* outColumnData,
                   size_t maxBins) const noexcept;
    
    // Statistics (any thread)
    size_t getUsedPages() const noexcept;
    size_t getTotalPages() const noexcept { return MAX_PAGES; }
    bool isInitialized() const noexcept { return initialized_.load(std::memory_order_acquire); }

public:
    // Forward declaration for public API
    struct Page {
        alignas(32) PageData data;
        std::atomic<bool> inUse{false};
        std::atomic<bool> ready{false};
        uint32_t generation = 0;
        
        Page() noexcept {
            data.fill(0.0f);
        }
    };

private:
    static constexpr size_t MAX_PAGES = 4;  // Double buffering + 2 spare
    
    // Page storage
    AlignedStorage<std::array<Page, MAX_PAGES>> pages_;
    
    // Atomic page handles for tear-free flipping
    std::atomic<AtlasPageHandle> activePage_;
    std::atomic<AtlasPageHandle> pendingPage_;
    
    // Page update notification for GPU
    std::atomic<bool> pageUpdateFlag_;
    
    // Generation counter for handle creation
    std::atomic<uint32_t> nextGeneration_;
    
    // Initialization state
    std::atomic<bool> initialized_;
    
    // Helper methods  
    Page* getPageById(uint32_t pageId) noexcept;
    const Page* getPageById(uint32_t pageId) const noexcept;
    bool isValidPosition(const AtlasPosition& pos) const noexcept;
    size_t getLinearIndex(const AtlasPosition& pos) const noexcept;
    uint32_t findFreePageId() noexcept;
};

// Inline implementations for performance-critical methods

inline bool TiledAtlas::isValidPosition(const AtlasPosition& pos) const noexcept {
    return pos.tileX < AtlasConfig::NUM_TILES_X &&
           pos.tileY < AtlasConfig::NUM_TILES_Y &&
           pos.columnInTile < AtlasConfig::TILE_WIDTH &&
           pos.binStart < AtlasConfig::TILE_HEIGHT;
}

inline size_t TiledAtlas::getLinearIndex(const AtlasPosition& pos) const noexcept {
    const size_t tileStartX = pos.tileX * AtlasConfig::TILE_WIDTH;
    const size_t tileStartY = pos.tileY * AtlasConfig::TILE_HEIGHT;
    const size_t x = tileStartX + pos.columnInTile;
    const size_t y = tileStartY + pos.binStart;
    return y * AtlasConfig::ATLAS_WIDTH + x;
}

inline AtlasPageHandle TiledAtlas::getActivePage() const noexcept {
    return activePage_.load(std::memory_order_acquire);
}

inline AtlasPageHandle TiledAtlas::getPendingPage() const noexcept {
    return pendingPage_.load(std::memory_order_acquire);
}

inline bool TiledAtlas::hasPageUpdate() const noexcept {
    return pageUpdateFlag_.load(std::memory_order_acquire);
}

inline void TiledAtlas::acknowledgePageUpdate() noexcept {
    pageUpdateFlag_.store(false, std::memory_order_release);
}