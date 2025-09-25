#pragma once
#include <string>
#include <vector>
#include <iostream>
#include "x3_rom_scan.hpp"

namespace x3 {

struct SampleManifest {
    std::string name;
    uint32_t offset = 0, length = 0;
    uint32_t samplerate = 44100;
    uint16_t bitdepth = 16, channels = 1;
    std::string encoding;   // "pcm16", "pcm8u", "ima-adpcm", ...
    std::string endianness; // "le", "be", "-"
    std::string wav_path;
    double evidence = 0.0;

    // Optional
    int loop_start = -1, loop_end = -1;

    // Quality
    double rms = 0.0, peak = 0.0, dc = 0.0, clipPct = 0.0, specFlatness = 0.0;
};

bool writeWav16(const std::string& path,
                const int16_t* interleaved, size_t numFrames,
                uint16_t channels, uint32_t samplerate);

std::vector<SampleManifest> extractSamples(const std::string& romPath,
                                           const std::string& outDir,
                                           const std::vector<x3::SampleGuess>& guesses,
                                           bool saveIfLowEvidence);

// Convert guesses to manifest format
std::vector<SampleManifest> toManifest(const std::vector<SampleGuess>& guesses,
                                       const std::string& outDir,
                                       const std::string& romName);

// Inline WAV writing utilities
static inline void put_u16(std::ostream& o, uint16_t v) {
    o.put(v & 0xFF); o.put((v>>8)&0xFF);
}

static inline void put_u32(std::ostream& o, uint32_t v) {
    o.put(v & 0xFF); o.put((v>>8)&0xFF); o.put((v>>16)&0xFF); o.put((v>>24)&0xFF);
}

} // namespace x3