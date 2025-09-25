#include "TiledAtlas.h"
#include <algorithm>
#include <cstring>

TiledAtlas::TiledAtlas() noexcept
    : pageUpdateFlag_(false)
    , nextGeneration_(1) // Start at 1, 0 is invalid
    , initialized_(false)
{
    activePage_.store({}, std::memory_order_release);
    pendingPage_.store({}, std::memory_order_release);
}

bool TiledAtlas::initialize() noexcept {
    if (initialized_.load(std::memory_order_acquire)) return true;
    
    // Initialize page storage
    auto& pages = pages_.get();
    for (size_t i = 0; i < MAX_PAGES; ++i) {
        pages[i].inUse.store(false, std::memory_order_release);
        pages[i].ready.store(false, std::memory_order_release);
        pages[i].generation = 0;
        pages[i].data.fill(0.0f);
    }
    
    initialized_.store(true, std::memory_order_release);
    return true;
}

void TiledAtlas::shutdown() noexcept {
    initialized_.store(false, std::memory_order_release);
    
    // Release all pages
    auto& pages = pages_.get();
    for (auto& page : pages) {
        page.inUse.store(false, std::memory_order_release);
        page.ready.store(false, std::memory_order_release);
    }
    
    activePage_.store({}, std::memory_order_release);
    pendingPage_.store({}, std::memory_order_release);
    pageUpdateFlag_.store(false, std::memory_order_release);
}

AtlasPageHandle TiledAtlas::allocateFreePage() noexcept {
    if (!initialized_.load(std::memory_order_acquire)) return {};
    
    const uint32_t pageId = findFreePageId();
    if (pageId == 0) return {}; // No free pages
    
    auto* page = getPageById(pageId);
    if (!page) return {};
    
    // Mark page as in use
    page->inUse.store(true, std::memory_order_release);
    page->ready.store(false, std::memory_order_release);
    
    // Generate new handle with unique generation
    const uint32_t generation = nextGeneration_.fetch_add(1, std::memory_order_acq_rel);
    page->generation = generation;
    
    return {pageId, generation};
}

void TiledAtlas::releasePage(AtlasPageHandle handle) noexcept {
    auto* page = getPageById(handle.id);
    if (!page || page->generation != handle.generation) return;
    
    // Clear page data
    page->data.fill(0.0f);
    page->ready.store(false, std::memory_order_release);
    page->inUse.store(false, std::memory_order_release);
}

bool TiledAtlas::isPageReady(AtlasPageHandle handle) const noexcept {
    const auto* page = getPageById(handle.id);
    if (!page || page->generation != handle.generation) return false;
    
    return page->ready.load(std::memory_order_acquire);
}

bool TiledAtlas::writeColumn(AtlasPageHandle pageHandle, 
                           const AtlasPosition& position,
                           const float* columnData,
                           size_t numBins) noexcept {
    if (!columnData || numBins == 0) return false;
    if (!isValidPosition(position)) return false;
    
    auto* page = getPageById(pageHandle.id);
    if (!page || page->generation != pageHandle.generation) return false;
    if (!page->inUse.load(std::memory_order_acquire)) return false;
    
    // Calculate write position
    const size_t baseIndex = getLinearIndex(position);
    const size_t binsToWrite = std::min(numBins, AtlasConfig::TILE_HEIGHT - position.binStart);
    
    // Write column data
    auto& pageData = page->data;
    for (size_t bin = 0; bin < binsToWrite; ++bin) {
        const size_t writeIndex = baseIndex + (bin * AtlasConfig::ATLAS_WIDTH);
        if (writeIndex < pageData.size()) {
            pageData[writeIndex] = columnData[bin];
        }
    }
    
    return true;
}

void TiledAtlas::activatePage(AtlasPageHandle handle) noexcept {
    auto* page = getPageById(handle.id);
    if (!page || page->generation != handle.generation) return;
    if (!page->inUse.load(std::memory_order_acquire)) return;
    
    // Mark page as ready
    page->ready.store(true, std::memory_order_release);
    
    // Atomic page flip
    const auto previousActive = activePage_.exchange(handle, std::memory_order_acq_rel);
    
    // Set previous page as pending for reuse
    if (previousActive.isValid()) {
        pendingPage_.store(previousActive, std::memory_order_release);
    }
    
    // Notify GPU thread of page update
    pageUpdateFlag_.store(true, std::memory_order_release);
}

const float* TiledAtlas::getActivePageData() const noexcept {
    const auto activeHandle = activePage_.load(std::memory_order_acquire);
    return getPageData(activeHandle);
}

const float* TiledAtlas::getPageData(AtlasPageHandle handle) const noexcept {
    const auto* page = getPageById(handle.id);
    if (!page || page->generation != handle.generation) return nullptr;
    if (!page->ready.load(std::memory_order_acquire)) return nullptr;
    
    return page->data.data();
}

bool TiledAtlas::readColumn(AtlasPageHandle pageHandle,
                          const AtlasPosition& position,
                          float* outColumnData,
                          size_t maxBins) const noexcept {
    if (!outColumnData || maxBins == 0) return false;
    if (!isValidPosition(position)) return false;
    
    const auto* page = getPageById(pageHandle.id);
    if (!page || page->generation != pageHandle.generation) return false;
    if (!page->ready.load(std::memory_order_acquire)) return false;
    
    // Calculate read position
    const size_t baseIndex = getLinearIndex(position);
    const size_t binsToRead = std::min(maxBins, AtlasConfig::TILE_HEIGHT - position.binStart);
    
    // Read column data
    const auto& pageData = page->data;
    for (size_t bin = 0; bin < binsToRead; ++bin) {
        const size_t readIndex = baseIndex + (bin * AtlasConfig::ATLAS_WIDTH);
        if (readIndex < pageData.size()) {
            outColumnData[bin] = pageData[readIndex];
        } else {
            outColumnData[bin] = 0.0f;
        }
    }
    
    // Zero remaining bins if requested more than available
    for (size_t bin = binsToRead; bin < maxBins; ++bin) {
        outColumnData[bin] = 0.0f;
    }
    
    return true;
}

size_t TiledAtlas::getUsedPages() const noexcept {
    const auto& pages = pages_.get();
    size_t count = 0;
    
    for (const auto& page : pages) {
        if (page.inUse.load(std::memory_order_acquire)) {
            count++;
        }
    }
    
    return count;
}

TiledAtlas::Page* TiledAtlas::getPageById(uint32_t pageId) noexcept {
    if (pageId == 0 || pageId > MAX_PAGES) return nullptr;
    
    auto& pages = pages_.get();
    return &pages[pageId - 1]; // Convert 1-based ID to 0-based index
}

const TiledAtlas::Page* TiledAtlas::getPageById(uint32_t pageId) const noexcept {
    if (pageId == 0 || pageId > MAX_PAGES) return nullptr;
    
    const auto& pages = pages_.get();
    return &pages[pageId - 1]; // Convert 1-based ID to 0-based index
}

uint32_t TiledAtlas::findFreePageId() noexcept {
    auto& pages = pages_.get();
    
    for (size_t i = 0; i < MAX_PAGES; ++i) {
        if (!pages[i].inUse.load(std::memory_order_acquire)) {
            return static_cast<uint32_t>(i + 1); // Convert to 1-based ID
        }
    }
    
    return 0; // No free pages
}