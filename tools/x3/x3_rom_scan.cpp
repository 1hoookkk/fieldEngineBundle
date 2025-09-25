#include "x3_rom_scan.hpp"
#include <algorithm>
#include <cmath>
#include <complex>
#include <numeric>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace x3 {

// Helper functions for evidence scoring
static double computeRMS(const int16_t* samples, size_t count) {
    if (count == 0) return 0.0;
    double sum = 0.0;
    for (size_t i = 0; i < count; ++i) {
        double val = samples[i] / 32768.0;
        sum += val * val;
    }
    return std::sqrt(sum / count);
}

static double computePeak(const int16_t* samples, size_t count) {
    if (count == 0) return 0.0;
    int16_t maxVal = 0;
    for (size_t i = 0; i < count; ++i) {
        maxVal = std::max(maxVal, static_cast<int16_t>(std::abs(samples[i])));
    }
    return maxVal / 32768.0;
}

static double computeDC(const int16_t* samples, size_t count) {
    if (count == 0) return 0.0;
    double sum = 0.0;
    for (size_t i = 0; i < count; ++i) {
        sum += samples[i];
    }
    return (sum / count) / 32768.0;
}

static double computeClipPercent(const int16_t* samples, size_t count) {
    if (count == 0) return 0.0;
    size_t clipped = 0;
    for (size_t i = 0; i < count; ++i) {
        if (std::abs(samples[i]) >= 32767) clipped++;
    }
    return (double)clipped / count;
}

static double computeZCR(const int16_t* samples, size_t count) {
    if (count < 2) return 0.0;
    size_t crossings = 0;
    for (size_t i = 1; i < count; ++i) {
        if ((samples[i-1] >= 0 && samples[i] < 0) || (samples[i-1] < 0 && samples[i] >= 0)) {
            crossings++;
        }
    }
    return (double)crossings / count;
}

static double computeSpectralFlatness(const int16_t* samples, size_t count) {
    if (count < 256) return 0.5; // Default for too-small windows

    // Simple spectral flatness using windowed samples
    const size_t fftSize = 256;
    size_t start = count / 4; // Skip some samples from start
    if (start + fftSize > count) start = 0;

    double geometricMean = 0.0;
    double arithmeticMean = 0.0;
    size_t validBins = 0;

    // Simplified spectral analysis - compute power in frequency bins
    for (size_t bin = 1; bin < fftSize/2; ++bin) {
        double real = 0.0, imag = 0.0;
        for (size_t i = 0; i < fftSize && start + i < count; ++i) {
            double angle = 2.0 * M_PI * bin * i / fftSize;
            double sample = samples[start + i] / 32768.0;
            real += sample * std::cos(angle);
            imag += sample * std::sin(angle);
        }
        double power = real * real + imag * imag;
        if (power > 1e-12) {
            geometricMean += std::log(power);
            arithmeticMean += power;
            validBins++;
        }
    }

    if (validBins == 0) return 0.5;
    geometricMean = std::exp(geometricMean / validBins);
    arithmeticMean /= validBins;

    return (arithmeticMean > 1e-12) ? (geometricMean / arithmeticMean) : 0.0;
}

static double scoreHypothesis(const uint8_t* data, size_t len, Encoding enc, Endian endian, uint16_t channels) {
    if (len < 1024) return 0.0;

    // Convert to int16 based on encoding/endian
    std::vector<int16_t> samples;
    size_t sampleCount = 0;

    if (enc == Encoding::PCM16) {
        sampleCount = len / 2;
        samples.resize(sampleCount);
        for (size_t i = 0; i < sampleCount && i * 2 + 1 < len; ++i) {
            if (endian == Endian::Little) {
                samples[i] = static_cast<int16_t>((data[i*2+1] << 8) | data[i*2]);
            } else {
                samples[i] = static_cast<int16_t>((data[i*2] << 8) | data[i*2+1]);
            }
        }
    } else if (enc == Encoding::PCM8S) {
        sampleCount = len;
        samples.resize(sampleCount);
        for (size_t i = 0; i < sampleCount; ++i) {
            samples[i] = static_cast<int16_t>(static_cast<int8_t>(data[i])) << 8;
        }
    } else if (enc == Encoding::PCM8U) {
        sampleCount = len;
        samples.resize(sampleCount);
        for (size_t i = 0; i < sampleCount; ++i) {
            samples[i] = static_cast<int16_t>(static_cast<int16_t>(data[i]) - 128) << 8;
        }
    } else {
        return 0.0; // Unsupported encoding
    }

    if (samples.empty()) return 0.0;

    // For stereo, analyze only first channel for simplicity
    if (channels == 2 && samples.size() >= 2) {
        std::vector<int16_t> mono;
        for (size_t i = 0; i < samples.size(); i += 2) {
            mono.push_back(samples[i]);
        }
        samples = std::move(mono);
    }

    // Use subset for analysis to avoid processing huge blocks
    size_t analyzeCount = std::min(samples.size(), size_t(8192));

    double rms = computeRMS(samples.data(), analyzeCount);
    double peak = computePeak(samples.data(), analyzeCount);
    double dc = computeDC(samples.data(), analyzeCount);
    double clipPct = computeClipPercent(samples.data(), analyzeCount);
    double zcr = computeZCR(samples.data(), analyzeCount);
    double specFlatness = computeSpectralFlatness(samples.data(), analyzeCount);

    double evidence = 0.0;

    // RMS in reasonable range (-30 to -6 dBFS)
    double rmsDb = 20.0 * std::log10(std::max(rms, 1e-6));
    if (rmsDb >= -30.0 && rmsDb <= -6.0) evidence += 0.25;

    // Low DC offset (< 2%)
    if (std::abs(dc) < 0.02) evidence += 0.2;

    // Low clipping (< 1%)
    if (clipPct < 0.01) evidence += 0.2;

    // Plausible zero crossing rate (0.5-5 per 100 samples)
    double zcrPer100 = zcr * 100.0;
    if (zcrPer100 >= 0.5 && zcrPer100 <= 5.0) evidence += 0.15;

    // Reasonable spectral flatness (0.2-0.95)
    if (specFlatness >= 0.2 && specFlatness <= 0.95) evidence += 0.2;

    return evidence;
}

std::vector<OffsetTable> findOffsetTables(const uint8_t* p, size_t n) {
    std::vector<OffsetTable> tables;
    if (n < 64) return tables;

    // Scan for potential offset tables at 64-byte aligned positions
    for (size_t pos = 0; pos + 64 <= n; pos += 64) {
        std::vector<uint32_t> offsets;

        // Try to read up to 16 32-bit values as potential offsets
        for (size_t i = 0; i < 16 && pos + i * 4 + 4 <= n; ++i) {
            uint32_t offset = (static_cast<uint32_t>(p[pos + i*4 + 0]) << 0) |
                             (static_cast<uint32_t>(p[pos + i*4 + 1]) << 8) |
                             (static_cast<uint32_t>(p[pos + i*4 + 2]) << 16) |
                             (static_cast<uint32_t>(p[pos + i*4 + 3]) << 24);

            // Must be in bounds and reasonable
            if (offset >= n || offset < 1024) break;

            // Check if strictly increasing with reasonable deltas
            if (!offsets.empty()) {
                uint32_t delta = offset - offsets.back();
                if (delta == 0 || delta > 32 * 1024 * 1024) break; // Not increasing or too large
            }

            offsets.push_back(offset);
        }

        // Need at least 3 valid offsets to be a table
        if (offsets.size() >= 3) {
            // Sample a few spans to see if they look like audio
            size_t goodSpans = 0;
            for (size_t i = 0; i + 1 < std::min(offsets.size(), size_t(5)); ++i) {
                uint32_t start = offsets[i];
                uint32_t end = offsets[i + 1];
                if (end > start && end - start >= 1024 && start < n && end <= n) {
                    // Quick check - does this span have audio-like properties?
                    double score = scoreHypothesis(p + start, end - start, Encoding::PCM16, Endian::Little, 1);
                    if (score > 0.3) goodSpans++;
                }
            }

            // If at least 2 spans look like audio, accept the table
            if (goodSpans >= 2) {
                OffsetTable table;
                table.tableOff = static_cast<uint32_t>(pos);
                table.offsets = std::move(offsets);
                tables.push_back(std::move(table));
            }
        }
    }

    return tables;
}

std::vector<SampleGuess> scanForPCMWindows(const uint8_t* p, size_t n, const ScanCfg& cfg) {
    std::vector<SampleGuess> guesses;
    if (n < cfg.minLen) return guesses;

    // First try offset table approach
    auto tables = findOffsetTables(p, n);
    for (const auto& table : tables) {
        for (size_t i = 0; i + 1 < table.offsets.size(); ++i) {
            uint32_t start = table.offsets[i];
            uint32_t end = table.offsets[i + 1];
            if (end > start && end - start >= cfg.minLen && end - start <= cfg.maxLen && start < n && end <= n) {
                // Test different encoding hypotheses
                std::vector<std::tuple<Encoding, Endian, uint16_t>> hypotheses = {
                    {Encoding::PCM16, Endian::Little, 1},
                    {Encoding::PCM16, Endian::Little, 2}
                };

                if (cfg.tryBothEndians16) {
                    hypotheses.push_back({Encoding::PCM16, Endian::Big, 1});
                    hypotheses.push_back({Encoding::PCM16, Endian::Big, 2});
                }

                hypotheses.push_back({Encoding::PCM8S, Endian::N_A, 1});
                hypotheses.push_back({Encoding::PCM8U, Endian::N_A, 1});

                double bestEvidence = 0.0;
                SampleGuess bestGuess;

                for (const auto& hyp : hypotheses) {
                    double evidence = scoreHypothesis(p + start, end - start, std::get<0>(hyp), std::get<1>(hyp), std::get<2>(hyp));
                    if (evidence > bestEvidence) {
                        bestEvidence = evidence;
                        bestGuess.off = start;
                        bestGuess.len = end - start;
                        bestGuess.encoding = std::get<0>(hyp);
                        bestGuess.endianness = std::get<1>(hyp);
                        bestGuess.channels = std::get<2>(hyp);
                        bestGuess.samplerate = cfg.defaultRate;
                        bestGuess.bitdepth = (bestGuess.encoding == Encoding::PCM16) ? 16 : 8;
                        bestGuess.evidence = evidence;
                    }
                }

                if (bestEvidence > 0.1) { // Minimum threshold
                    guesses.push_back(bestGuess);
                }
            }
        }
    }

    // Also try sliding window approach for areas without clear tables
    if (guesses.size() < 10) { // Don't do expensive scan if we found lots already
        const size_t step = 4096; // Reasonable step size
        for (size_t pos = 0; pos + cfg.minLen <= n; pos += step) {
            size_t windowLen = std::min(cfg.maxLen, n - pos);
            if (windowLen < cfg.minLen) break;

            double bestEvidence = 0.0;
            SampleGuess bestGuess;

            // Test hypotheses
            std::vector<std::tuple<Encoding, Endian, uint16_t>> hypotheses = {
                {Encoding::PCM16, Endian::Little, 1},
                {Encoding::PCM16, Endian::Little, 2}
            };

            for (const auto& hyp : hypotheses) {
                double evidence = scoreHypothesis(p + pos, windowLen, std::get<0>(hyp), std::get<1>(hyp), std::get<2>(hyp));
                if (evidence > bestEvidence) {
                    bestEvidence = evidence;
                    bestGuess.off = static_cast<uint32_t>(pos);
                    bestGuess.len = static_cast<uint32_t>(windowLen);
                    bestGuess.encoding = std::get<0>(hyp);
                    bestGuess.endianness = std::get<1>(hyp);
                    bestGuess.channels = std::get<2>(hyp);
                    bestGuess.samplerate = cfg.defaultRate;
                    bestGuess.bitdepth = (bestGuess.encoding == Encoding::PCM16) ? 16 : 8;
                    bestGuess.evidence = evidence;
                }
            }

            if (bestEvidence > 0.4) { // Higher threshold for sliding window
                guesses.push_back(bestGuess);
            }
        }
    }

    // Sort by evidence score descending
    std::sort(guesses.begin(), guesses.end(), [](const SampleGuess& a, const SampleGuess& b) {
        return a.evidence > b.evidence;
    });

    return guesses;
}

SampleGuess analyzeWindow(const uint8_t* p, size_t n, SampleGuess g) {
    if (g.len == 0 || n < g.len) return g;

    // Convert to int16 for analysis
    std::vector<int16_t> samples;
    size_t sampleCount = 0;

    if (g.encoding == Encoding::PCM16) {
        sampleCount = g.len / 2;
        samples.resize(sampleCount);
        for (size_t i = 0; i < sampleCount && i * 2 + 1 < g.len; ++i) {
            if (g.endianness == Endian::Little) {
                samples[i] = static_cast<int16_t>((p[i*2+1] << 8) | p[i*2]);
            } else {
                samples[i] = static_cast<int16_t>((p[i*2] << 8) | p[i*2+1]);
            }
        }
    } else if (g.encoding == Encoding::PCM8S) {
        sampleCount = g.len;
        samples.resize(sampleCount);
        for (size_t i = 0; i < sampleCount; ++i) {
            samples[i] = static_cast<int16_t>(static_cast<int8_t>(p[i])) << 8;
        }
    } else if (g.encoding == Encoding::PCM8U) {
        sampleCount = g.len;
        samples.resize(sampleCount);
        for (size_t i = 0; i < sampleCount; ++i) {
            samples[i] = static_cast<int16_t>(static_cast<int16_t>(p[i]) - 128) << 8;
        }
    }

    if (samples.empty()) return g;

    // Trim silence from beginning and end
    const double silenceThreshold = 0.001; // -60 dBFS
    size_t start = 0, end = samples.size();

    // Find first non-silent sample
    for (size_t i = 0; i < samples.size(); ++i) {
        if (std::abs(samples[i]) / 32768.0 > silenceThreshold) {
            start = i;
            break;
        }
    }

    // Find last non-silent sample
    for (size_t i = samples.size(); i > 0; --i) {
        if (std::abs(samples[i-1]) / 32768.0 > silenceThreshold) {
            end = i;
            break;
        }
    }

    // Keep at least 1024 samples
    if (end > start && end - start >= 1024) {
        size_t trimmedSamples = end - start;
        size_t trimmedBytes = (g.encoding == Encoding::PCM16) ? trimmedSamples * 2 : trimmedSamples;

        g.off += static_cast<uint32_t>(start * ((g.encoding == Encoding::PCM16) ? 2 : 1));
        g.len = static_cast<uint32_t>(trimmedBytes);

        // Update samples vector for metrics
        samples.erase(samples.begin(), samples.begin() + start);
        samples.erase(samples.begin() + (end - start), samples.end());
    }

    // Compute quality metrics
    if (!samples.empty()) {
        g.rms = computeRMS(samples.data(), samples.size());
        g.peak = computePeak(samples.data(), samples.size());
        g.dc = computeDC(samples.data(), samples.size());
        g.clipPct = computeClipPercent(samples.data(), samples.size());
        g.specFlatness = computeSpectralFlatness(samples.data(), samples.size());

        // Try to detect loop points if sample is long enough
        if (samples.size() > 8192) {
            uint32_t loopStart = 0, loopEnd = 0;
            estimateLoopPoints(samples.data(), samples.size(), loopStart, loopEnd);
            if (loopEnd > loopStart && loopEnd - loopStart > 1024) {
                g.loopStart = loopStart;
                g.loopEnd = loopEnd;
            }
        }
    }

    return g;
}

void estimateLoopPoints(const int16_t* mono, size_t N, uint32_t& loopStart, uint32_t& loopEnd) {
    loopStart = loopEnd = 0;
    if (N < 8192) return;

    const size_t tailSize = 4096;
    const size_t searchSize = std::min(N - tailSize, size_t(48000));

    if (searchSize < tailSize) return;

    const int16_t* tail = mono + N - tailSize;
    double bestCorr = 0.85; // Minimum correlation threshold
    uint32_t bestStart = 0;

    // Search for best correlation
    for (size_t searchPos = 0; searchPos < searchSize; searchPos += 256) {
        const int16_t* candidate = mono + searchPos;

        // Compute normalized cross-correlation
        double sum1 = 0.0, sum2 = 0.0, sumProd = 0.0;
        for (size_t i = 0; i < tailSize; ++i) {
            double v1 = candidate[i] / 32768.0;
            double v2 = tail[i] / 32768.0;
            sum1 += v1 * v1;
            sum2 += v2 * v2;
            sumProd += v1 * v2;
        }

        double denom = std::sqrt(sum1 * sum2);
        if (denom > 1e-12) {
            double corr = sumProd / denom;
            if (corr > bestCorr) {
                bestCorr = corr;
                bestStart = static_cast<uint32_t>(searchPos);
            }
        }
    }

    if (bestStart > 0) {
        loopStart = bestStart;
        loopEnd = static_cast<uint32_t>(N - tailSize);
    }
}

} // namespace x3