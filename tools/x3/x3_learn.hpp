#pragma once

#include <vector>
#include <cstdint>
#include <optional>
#include <algorithm>
#include <cmath>
#include <array>
#include <string>

namespace x3 {

struct LearnResult { uint32_t off; double mad; int uniq; };

// Robust statistics helpers
inline double median(std::vector<double> v) {
	if (v.empty()) return 0.0;
	std::sort(v.begin(), v.end());
	size_t n = v.size();
	if (n % 2 == 1) return v[n/2];
	return 0.5 * (v[n/2 - 1] + v[n/2]);
}

inline double mad_about_median(const std::vector<double>& data) {
	if (data.empty()) return 0.0;
	std::vector<double> diffs;
	diffs.reserve(data.size());
	double med = median(std::vector<double>(data.begin(), data.end()));
	for (double x : data) diffs.push_back(std::fabs(x - med));
	return median(std::move(diffs));
}

inline int unique_count_sorted(std::vector<double> v, double eps = 1e-9) {
	if (v.empty()) return 0;
	std::sort(v.begin(), v.end());
	int count = 1;
	for (size_t i = 1; i < v.size(); ++i) {
		if (std::fabs(v[i] - v[i-1]) > eps) ++count;
	}
	return count;
}

inline double correlation_pearson(const std::vector<double>& a, const std::vector<double>& b) {
	if (a.size() != b.size() || a.size() < 2) return 0.0;
	double meanA = 0.0, meanB = 0.0;
	for (size_t i = 0; i < a.size(); ++i) { meanA += a[i]; meanB += b[i]; }
	meanA /= static_cast<double>(a.size());
	meanB /= static_cast<double>(b.size());
	double num = 0.0, denA = 0.0, denB = 0.0;
	for (size_t i = 0; i < a.size(); ++i) {
		double da = a[i] - meanA;
		double db = b[i] - meanB;
		num += da * db;
		denA += da * da;
		denB += db * db;
	}
	if (denA <= 0.0 || denB <= 0.0) return 0.0;
	return num / std::sqrt(denA * denB);
}

// Generic learner: given a set of candidate offsets (4-byte BE floats), choose the best one
// based on unique value count and MAD, with minimum N and correlation de-duplication.
inline std::optional<LearnResult> learn_best_float_offset(
	const std::vector<const uint8_t*>& e5p1s,
	const std::vector<size_t>& lens,
	const std::vector<uint32_t>& candidateOffsets,
	int minN = 10,
	double corrDropThreshold = 0.95) {
	if (e5p1s.size() != lens.size() || e5p1s.size() < static_cast<size_t>(minN)) return std::nullopt;
	// Collect per-candidate series
	struct Series { uint32_t off; std::vector<double> vals; int uniq; double mad; };
	std::vector<Series> series;
	series.reserve(candidateOffsets.size());
	for (uint32_t off : candidateOffsets) {
		Series s{off, {}, 0, 0.0};
		s.vals.reserve(e5p1s.size());
		for (size_t i = 0; i < e5p1s.size(); ++i) {
			if (off + 4u <= lens[i]) {
				uint32_t u = (static_cast<uint32_t>(e5p1s[i][off+0]) << 24) |
				            (static_cast<uint32_t>(e5p1s[i][off+1]) << 16) |
				            (static_cast<uint32_t>(e5p1s[i][off+2]) << 8)  |
				             static_cast<uint32_t>(e5p1s[i][off+3]);
				union { uint32_t u32; float f32; } cvt { u };
				double val = static_cast<double>(cvt.f32);
				if (std::isfinite(val) && std::fabs(val) <= 1.0e6) s.vals.push_back(val);
			}
		}
		if (static_cast<int>(s.vals.size()) >= minN) {
			s.uniq = unique_count_sorted(s.vals);
			s.mad = mad_about_median(s.vals);
			series.push_back(std::move(s));
		}
	}
	if (series.empty()) return std::nullopt;
	// Drop highly correlated duplicates: keep lowest offset per correlated cluster
	std::vector<bool> dropped(series.size(), false);
	for (size_t i = 0; i < series.size(); ++i) {
		if (dropped[i]) continue;
		for (size_t j = i + 1; j < series.size(); ++j) {
			if (dropped[j]) continue;
			double r = correlation_pearson(series[i].vals, series[j].vals);
			if (r > corrDropThreshold) {
				// drop the higher offset
				if (series[i].off < series[j].off) dropped[j] = true; else dropped[i] = true;
			}
		}
	}
	// Choose best by uniq then MAD
	std::optional<LearnResult> best;
	for (size_t i = 0; i < series.size(); ++i) {
		if (dropped[i]) continue;
		LearnResult lr{ series[i].off, series[i].mad, series[i].uniq };
		if (!best.has_value()) best = lr;
		else if (lr.uniq > best->uniq || (lr.uniq == best->uniq && lr.mad > best->mad)) best = lr;
	}
	return best;
}

// Specific learners (placeholders choose candidates as simple grid; a real impl would infer TLV offsets)
inline LearnResult learn_lfo_rate(const std::vector<const uint8_t*>& e5p1s, const std::vector<size_t>& lens) {
	std::vector<uint32_t> candidates;
	// simple stride scan at 2-byte alignment within first 256 bytes
	for (uint32_t off = 0; off + 4 <= 256; off += 2) candidates.push_back(off);
	auto r = learn_best_float_offset(e5p1s, lens, candidates);
	return r.value_or(LearnResult{0u,0.0,0});
}

inline LearnResult learn_env_A(const std::vector<const uint8_t*>& e5p1s, const std::vector<size_t>& lens) {
	std::vector<uint32_t> candidates;
	for (uint32_t off = 0; off + 4 <= 512; off += 2) candidates.push_back(off);
	auto r = learn_best_float_offset(e5p1s, lens, candidates);
	return r.value_or(LearnResult{0u,0.0,0});
}

// TLV-aware ENV detection
struct ENVBundle {
	float a, d, s, r;
	uint32_t offset;
	bool valid;
};

inline ENVBundle probeENV_TLV(const uint8_t* e5p1, size_t len) {
	ENVBundle result{0, 0, 0, 0, 0, false};

	// Walk potential ADSR locations looking for 4-float clusters
	for (size_t off = 0; off + 16 <= len; off += 2) {  // 2-byte alignment
		// Try reading 4 consecutive BE floats
		float vals[4];
		bool valid = true;

		for (int i = 0; i < 4; i++) {
			uint32_t u = (static_cast<uint32_t>(e5p1[off + i*4 + 0]) << 24) |
			            (static_cast<uint32_t>(e5p1[off + i*4 + 1]) << 16) |
			            (static_cast<uint32_t>(e5p1[off + i*4 + 2]) << 8)  |
			             static_cast<uint32_t>(e5p1[off + i*4 + 3]);
			union { uint32_t u32; float f32; } cvt { u };
			vals[i] = cvt.f32;

			if (!std::isfinite(vals[i])) {
				valid = false;
				break;
			}
		}

		if (!valid) continue;

		// ADSR heuristics:
		// A/D/R: [0..15] seconds (time parameters)
		// S: [0..1.2] level (sustain is a level, not time)
		bool timeValid = (vals[0] >= 0.0f && vals[0] <= 15.0f) &&
		                (vals[1] >= 0.0f && vals[1] <= 15.0f) &&
		                (vals[3] >= 0.0f && vals[3] <= 15.0f);
		bool susValid = (vals[2] >= 0.0f && vals[2] <= 1.2f);

		if (timeValid && susValid) {
			result.a = vals[0];
			result.d = vals[1];
			result.s = vals[2];
			result.r = vals[3];
			result.offset = static_cast<uint32_t>(off);
			result.valid = true;
			return result;  // Return first valid ADSR found
		}
	}

	return result;
}

// Mod cord detection structures
struct ModCord {
	uint16_t src;   // LFO1, ENV1, etc
	uint16_t dst;   // filter.cutoff, filter.res, etc
	float depth;
	uint8_t pol;    // unipolar/bipolar
};

// Helper to detect depth encoding format
inline float detectDepthFormat(const uint8_t* p, size_t len) {
	if (len < 4) return 0.0f;

	struct Candidate {
		float value;
		int score;
	};

	Candidate candidates[4];

	// A) BE float32 (4 bytes)
	uint32_t u32 = (static_cast<uint32_t>(p[0]) << 24) |
	               (static_cast<uint32_t>(p[1]) << 16) |
	               (static_cast<uint32_t>(p[2]) << 8) |
	                static_cast<uint32_t>(p[3]);
	union { uint32_t u; float f; } cvt { u32 };
	float f32 = cvt.f;
	candidates[0] = { std::isfinite(f32) ? std::max(-2.0f, std::min(2.0f, f32)) : 0.0f, 0 };

	// B) BE s16 (2 bytes) normalized
	int16_t s16 = static_cast<int16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
	candidates[1] = { static_cast<float>(s16) / 32767.0f, 0 };

	// C) s7 MIDI-style (1 byte, -64 to +63)
	int8_t s7 = static_cast<int8_t>(p[0]);
	if (s7 >= -64 && s7 <= 63) {
		candidates[2] = { static_cast<float>(s7) / 63.0f, 0 };
	} else {
		candidates[2] = { 0.0f, 0 };
	}

	// D) Fixed-point s8.7 (2 bytes)
	int16_t fx = static_cast<int16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
	candidates[3] = { static_cast<float>(fx) / 16384.0f, 0 };

	// Score by: non-zero, reasonable range, and likely format patterns
	for (int i = 0; i < 4; ++i) {
		Candidate& c = candidates[i];
		if (std::isfinite(c.value) && std::fabs(c.value) > 1e-6f && std::fabs(c.value) <= 1.0f) {
			c.score += 3;  // Valid range bonus
		}
		if (std::fabs(c.value) > 0.001f && std::fabs(c.value) < 1.0f) {
			c.score += 2;  // Non-trivial value bonus
		}
		// Check for common mod depth values (0.1, 0.25, 0.5, 0.75, 1.0)
		float abs_val = std::fabs(c.value);
		if (std::fabs(abs_val - 0.1f) < 0.01f || std::fabs(abs_val - 0.25f) < 0.01f ||
		    std::fabs(abs_val - 0.5f) < 0.01f || std::fabs(abs_val - 0.75f) < 0.01f ||
		    std::fabs(abs_val - 1.0f) < 0.01f) {
			c.score += 1;  // Common value bonus
		}
	}

	// Return the best scoring candidate
	Candidate* best = &candidates[0];
	for (int i = 1; i < 4; ++i) {
		if (candidates[i].score > best->score) {
			best = &candidates[i];
		}
	}

	return (best->score > 0) ? best->value : 0.0f;
}

// Map source IDs to friendly names
inline std::string mapModSource(uint16_t src) {
	// Common EMU source IDs
	if (src >= 0x01 && src <= 0x02) return "LFO" + std::to_string(src);
	if (src >= 0x10 && src <= 0x11) return "LFO" + std::to_string(src - 0x0F);
	if (src >= 0x20 && src <= 0x23) return "ENV" + std::to_string(src - 0x1F);
	if (src >= 0x30 && src <= 0x33) return "ENV" + std::to_string(src - 0x2F);
	if (src == 0x2B || src == 43) return "LFO1";  // Found in Orbit-3
	if (src >= 0x40 && src <= 0x4F) return "MIDI_CC" + std::to_string(src - 0x40);
	if (src >= 0x50 && src <= 0x5F) return "KEY";
	if (src >= 0x60 && src <= 0x6F) return "VEL";
	return "";  // Unknown source - will be filtered out
}

// Map destination IDs to filter parameter names
inline std::string mapModDest(uint16_t dst) {
	// Map various ID ranges to filter parameters
	switch(dst & 0xFF) {
		case 0x00: case 0x10: case 0x20: case 0x30: return "filter.cutoff";
		case 0x01: case 0x11: case 0x21: case 0x31: return "filter.resonance";
		case 0x02: case 0x12: case 0x22: case 0x32: return "filter.t1";
		case 0x03: case 0x13: case 0x23: case 0x33: return "filter.t2";
	}

	// Alternative mappings
	if ((dst & 0xF00) == 0x100) {
		switch(dst & 0x0F) {
			case 0: return "filter.cutoff";
			case 1: return "filter.resonance";
			case 2: return "filter.t1";
			case 3: return "filter.t2";
		}
	}

	return "";  // Unknown destination
}

inline std::vector<ModCord> extractMods_TLV(const uint8_t* e5p1, size_t len) {
	std::vector<ModCord> mods;

	// Try different structure sizes: 8, 10, 12 bytes
	for (int structSize : {8, 10, 12}) {
		std::vector<ModCord> candidates;

		for (size_t off = 0; off + structSize <= len; off += 2) {
			// Try parsing as: src(2) + dst(2) + depth(varies) + pol/flags
			uint16_t src = (static_cast<uint16_t>(e5p1[off]) << 8) | e5p1[off+1];
			uint16_t dst = (static_cast<uint16_t>(e5p1[off+2]) << 8) | e5p1[off+3];

			// Use auto-detection for depth format
			float depth = detectDepthFormat(e5p1 + off + 4, structSize - 4);

			// Skip if depth is effectively zero
			if (std::fabs(depth) < 0.0001f) continue;

			// Determine polarity from depth sign or flags byte
			uint8_t pol = (depth >= 0.0f) ? 0 : 1;  // 0=unipolar, 1=bipolar
			depth = std::fabs(depth);  // Store absolute value

			// Map IDs to strings
			std::string srcStr = mapModSource(src);
			std::string dstStr = mapModDest(dst);

			// Only accept if we can map both source and dest
			if (!srcStr.empty() && !dstStr.empty() && dstStr.find("filter.") == 0) {
				// Store with original IDs for now (will convert in main.cpp)
				candidates.push_back({src, dst, depth, pol});
			}
		}

		// If we found mods with this structure size, use them
		if (!candidates.empty()) {
			return candidates;
		}
	}

	return mods;
}

} // namespace x3


