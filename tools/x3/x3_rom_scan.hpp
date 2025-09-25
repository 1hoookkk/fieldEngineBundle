#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <optional>

namespace x3 {

// --- Guessed audio description ---
enum class Encoding { PCM16, PCM8U, PCM8S, ADPCM_IMA, Unknown };
enum class Endian { Little, Big, N_A };

struct SampleGuess {
    uint32_t off = 0, len = 0;
    uint32_t samplerate = 44100;   // guess; allow override
    uint16_t bitdepth = 16;
    uint16_t channels = 1;
    Encoding encoding = Encoding::PCM16;
    Endian endianness = Endian::Little;
    // Optional loop info (samples)
    std::optional<uint32_t> loopStart, loopEnd;

    // Evidence (0..1): combined score from detectors
    double evidence = 0.0;

    // Quality metrics
    double rms = 0.0, peak = 0.0, dc = 0.0, clipPct = 0.0, specFlatness = 0.0;
};

struct OffsetTable {
    uint32_t tableOff = 0;
    std::vector<uint32_t> offsets;  // validated, in-bounds, ascending
};

// Scan heuristics configuration
struct ScanCfg {
    size_t minLen = 1024;            // bytes
    size_t maxLen = 32 * 1024 * 1024;
    uint32_t defaultRate = 44100;
    bool tryStereo = true;
    bool tryBothEndians16 = true;
    bool probeADPCM = false;
};

std::vector<OffsetTable> findOffsetTables(const uint8_t* p, size_t n);
std::vector<SampleGuess>   scanForPCMWindows(const uint8_t* p, size_t n, const ScanCfg& cfg);

// Validate & refine a guess (trimming silence, computing metrics)
SampleGuess analyzeWindow(const uint8_t* p, size_t n, SampleGuess g);

// Optional loop finder
void estimateLoopPoints(const int16_t* mono, size_t N, uint32_t& loopStart, uint32_t& loopEnd);

} // namespace x3