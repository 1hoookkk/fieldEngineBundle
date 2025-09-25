#include "x3_sample_extract.hpp"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <vector>
#include <map>

namespace fs = std::filesystem;

namespace x3 {

// Robust PCM conversion with proper scaling and endian handling
static inline uint16_t bswap16(uint16_t v) {
    return static_cast<uint16_t>((v >> 8) | (v << 8));
}

static std::vector<int16_t> convert_to_pcm16le(const uint8_t* src, size_t nBytes,
                                               Encoding enc, Endian endianness, uint16_t channels) {
    std::vector<int16_t> out;
    out.reserve(nBytes); // upper bound

    switch (enc) {
    case Encoding::PCM16: {
        size_t nSamp16 = nBytes / 2;
        out.resize(nSamp16);
        const uint16_t* p = reinterpret_cast<const uint16_t*>(src);
        if (endianness == Endian::Big) {
            for (size_t i = 0; i < nSamp16; i++) {
                out[i] = static_cast<int16_t>(bswap16(p[i]));
            }
        } else {
            // little-endian in ROM — copy as-is
            std::memcpy(out.data(), src, nSamp16 * 2);
        }
        break;
    }
    case Encoding::PCM8S: {
        // −128..127 → scale to 16-bit range
        size_t n = nBytes;
        out.resize(n);
        for (size_t i = 0; i < n; i++) {
            int8_t v = static_cast<int8_t>(src[i]);
            out[i] = static_cast<int16_t>(static_cast<int32_t>(v) << 8); // *256
        }
        break;
    }
    case Encoding::PCM8U: {
        // 0..255 → center at 128, then scale
        size_t n = nBytes;
        out.resize(n);
        for (size_t i = 0; i < n; i++) {
            int32_t v = static_cast<int32_t>(src[i]) - 128;
            out[i] = static_cast<int16_t>(v << 8);
        }
        break;
    }
    default:
        // Unknown/ADPCM: return empty so caller can skip or decode elsewhere
        return {};
    }

    // Sanity: drop any trailing odd byte if channels*2 doesn't divide evenly
    const size_t bytesPerFrame = channels * 2;
    const size_t frames = (out.size() * 2) / bytesPerFrame;
    out.resize(frames * (bytesPerFrame / 2));
    return out;
}

// Enhanced audio quality validation
static void validateAudioQuality(const std::vector<int16_t>& samples, uint32_t offset, const std::string& name) {
    if (samples.empty()) {
        std::cerr << "[ERROR] Zero samples extracted for " << name << " at offset 0x" << std::hex << offset << std::dec << std::endl;
        return;
    }

    int16_t peak = 0;
    for (auto s : samples) {
        peak = std::max(peak, static_cast<int16_t>(std::abs(static_cast<int>(s))));
    }

    double peakDb = 20.0 * std::log10(std::max(1, static_cast<int>(peak)) / 32767.0);
    if (peak < 2000) { // ~ -24 dBFS
        std::cerr << "[WARN] Very low peak (" << std::fixed << std::setprecision(1) << peakDb
                  << " dBFS) for " << name << " at offset 0x" << std::hex << offset << std::dec << std::endl;
    }

    size_t frames = samples.size();
    if (frames == 0) {
        std::cerr << "[ERROR] Zero frames for " << name << " at offset 0x" << std::hex << offset << std::dec << std::endl;
    }
}

bool writeWav16(const std::string& path,
                const int16_t* interleaved, size_t numFrames,
                uint16_t channels, uint32_t samplerate) {
    const uint32_t byteRate = samplerate * channels * 2;
    const uint16_t blockAlign = channels * 2;
    const uint32_t dataBytes = static_cast<uint32_t>(numFrames * blockAlign);

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    f.write("RIFF", 4);
    put_u32(f, 36 + dataBytes);
    f.write("WAVE", 4);
    f.write("fmt ", 4);
    put_u32(f, 16);                 // PCM fmt chunk
    put_u16(f, 1);                  // PCM
    put_u16(f, channels);
    put_u32(f, samplerate);
    put_u32(f, byteRate);
    put_u16(f, blockAlign);
    put_u16(f, 16);                 // bits/sample
    f.write("data", 4);
    put_u32(f, dataBytes);
    f.write(reinterpret_cast<const char*>(interleaved), dataBytes);
    return f.good();
}

std::vector<SampleManifest> toManifest(const std::vector<SampleGuess>& guesses,
                                       const std::string& outDir,
                                       const std::string& romName) {
    std::vector<SampleManifest> out;
    int idx = 0;
    for (const auto& g : guesses) {
        SampleManifest m;
        m.name = romName + "_" + std::to_string(idx++);
        m.offset = g.off;
        m.length = g.len;
        m.samplerate = g.samplerate;
        m.bitdepth = g.bitdepth;
        m.channels = g.channels;
        m.encoding = (g.encoding == Encoding::PCM16 ? "pcm16" :
                     (g.encoding == Encoding::PCM8U ? "pcm8u" :
                     (g.encoding == Encoding::PCM8S ? "pcm8s" : "unknown")));
        m.endianness = (g.endianness == Endian::Little ? "le" :
                       g.endianness == Endian::Big ? "be" : "-");
        m.wav_path = outDir + "/" + m.name + ".wav";
        if (g.loopStart && g.loopEnd) {
            m.loop_start = static_cast<int>(*g.loopStart);
            m.loop_end = static_cast<int>(*g.loopEnd);
        }
        m.evidence = g.evidence;
        m.rms = g.rms;
        m.peak = g.peak;
        m.dc = g.dc;
        m.clipPct = g.clipPct;
        m.specFlatness = g.specFlatness;
        out.push_back(std::move(m));
    }
    return out;
}

// Acceptance gate validation following E5P1 methodology
static bool passesAcceptanceGates(const SampleGuess& guess) {
    // Evidence threshold
    if (guess.evidence < 0.65) return false;

    // Peak and RMS in reasonable range
    if (guess.peak < 0.05 || guess.peak > 1.0) return false;
    double rmsDb = 20.0 * std::log10(std::max(guess.rms, 1e-6));
    if (rmsDb < -40.0) return false;

    // DC offset check
    if (std::abs(guess.dc) > 0.02) return false;

    // Clipping check
    if (guess.clipPct > 0.01) return false;

    // Spectral flatness check
    if (guess.specFlatness < 0.15 || guess.specFlatness > 0.95) return false;

    return true;
}

std::vector<SampleManifest> extractSamples(const std::string& romPath,
                                           const std::string& outDir,
                                           const std::vector<SampleGuess>& guesses,
                                           bool saveIfLowEvidence) {
    std::vector<SampleManifest> manifests;

    // Create output directory
    fs::create_directories(outDir);

    // Read ROM file
    std::ifstream romFile(romPath, std::ios::binary);
    if (!romFile) {
        std::cerr << "ERR: cannot read ROM file: " << romPath << std::endl;
        return manifests;
    }

    romFile.seekg(0, std::ios::end);
    size_t romSize = romFile.tellg();
    romFile.seekg(0, std::ios::beg);

    std::vector<uint8_t> romData(romSize);
    romFile.read(reinterpret_cast<char*>(romData.data()), romSize);

    fs::path romPathObj(romPath);
    std::string romName = romPathObj.stem().string();

    // Collect statistics for dominant bucket analysis
    std::map<uint32_t, size_t> lengthHist;
    for (const auto& g : guesses) {
        lengthHist[g.len]++;
    }

    uint32_t dominantLength = 0;
    size_t dominantCount = 0;
    for (const auto& kv : lengthHist) {
        if (kv.second > dominantCount) {
            dominantCount = kv.second;
            dominantLength = kv.first;
        }
    }

    size_t extracted = 0;
    size_t rejected = 0;

    for (const auto& guess : guesses) {
        bool shouldExtract = saveIfLowEvidence || passesAcceptanceGates(guess);

        if (!shouldExtract) {
            rejected++;
            continue;
        }

        // Validate offset bounds
        if (guess.off >= romSize || guess.off + guess.len > romSize) {
            std::cerr << "WARN: sample offset out of bounds, skipping" << std::endl;
            rejected++;
            continue;
        }

        // Use robust conversion with proper scaling and validation
        const uint8_t* srcData = romData.data() + guess.off;
        auto samples = convert_to_pcm16le(srcData, guess.len, guess.encoding, guess.endianness, guess.channels);

        // Validate converted audio quality
        validateAudioQuality(samples, guess.off, "sample_" + std::to_string(extracted));

        // Calculate proper frame count
        const uint16_t ch = guess.channels;
        const size_t frames = samples.size() / ch;

        if (samples.empty() || frames == 0) {
            std::cerr << "WARN: failed to convert sample data or zero frames, skipping" << std::endl;
            rejected++;
            continue;
        }

        // Create manifest entry
        SampleManifest manifest;
        manifest.name = romName + "_sample_" + std::to_string(extracted);
        manifest.offset = guess.off;
        manifest.length = guess.len;
        manifest.samplerate = guess.samplerate;
        manifest.bitdepth = guess.bitdepth;
        manifest.channels = guess.channels;
        manifest.encoding = (guess.encoding == Encoding::PCM16 ? "pcm16" :
                           (guess.encoding == Encoding::PCM8U ? "pcm8u" :
                           (guess.encoding == Encoding::PCM8S ? "pcm8s" : "unknown")));
        manifest.endianness = (guess.endianness == Endian::Little ? "le" :
                             guess.endianness == Endian::Big ? "be" : "-");
        manifest.wav_path = outDir + "/" + manifest.name + ".wav";
        if (guess.loopStart && guess.loopEnd) {
            manifest.loop_start = static_cast<int>(*guess.loopStart);
            manifest.loop_end = static_cast<int>(*guess.loopEnd);
        }
        manifest.evidence = guess.evidence;
        manifest.rms = guess.rms;
        manifest.peak = guess.peak;
        manifest.dc = guess.dc;
        manifest.clipPct = guess.clipPct;
        manifest.specFlatness = guess.specFlatness;

        // Write WAV file with validated data
        if (writeWav16(manifest.wav_path, samples.data(), frames, guess.channels, guess.samplerate)) {
            manifests.push_back(std::move(manifest));
            extracted++;
        } else {
            std::cerr << "ERR: failed to write WAV file: " << manifest.wav_path << std::endl;
            rejected++;
        }
    }

    // Print statistics in E5P1 style
    std::cout << "EXTRACT: processed " << guesses.size() << " candidates" << std::endl;
    std::cout << "EXTRACT: wrote " << extracted << " samples, rejected " << rejected << std::endl;
    if (dominantLength > 0) {
        std::cout << "EXTRACT: dominant length " << dominantLength << " (count=" << dominantCount << ")" << std::endl;
    }

    return manifests;
}

} // namespace x3