#pragma once

#include <cstdint>
#include <vector>
#include <tuple>
#include <map>
#include <algorithm>
#include <cmath>

namespace x3 {

struct E5P1Hit { uint32_t off; uint32_t len; };

// Helper to check if three bytes equal ASCII '5','P','1'
inline bool isASCII_5P1(const uint8_t a, const uint8_t b, const uint8_t c) {
	return a=='5' && b=='P' && c=='1';
}

// Big-endian 32-bit to host
inline uint32_t be32_at(const uint8_t* p) {
	return (static_cast<uint32_t>(p[0]) << 24) |
	       (static_cast<uint32_t>(p[1]) << 16) |
	       (static_cast<uint32_t>(p[2]) << 8)  |
	        static_cast<uint32_t>(p[3]);
}

// Signature scan for "E5P1" with false-positive hardening.
// - Enforces even length, 3.5KB..40KB, bounds-safe, and guards against immediate '5P1' drift.
inline std::vector<E5P1Hit> indexE5P1_bySig(const uint8_t* data, size_t size) {
	std::vector<E5P1Hit> hits;
	if (!data || size < 8) return hits;
	for (size_t i = 0; i + 8 <= size; ++i) {
		if (data[i] == 'E' && data[i+1] == '5' && data[i+2] == 'P' && data[i+3] == '1') {
			const uint32_t len = be32_at(data + i + 4);
			const bool evenLength = (len % 2u) == 0u;
			const bool inRange = (len >= 3500u && len <= 40000u);
			const bool inBounds = (i + 8ull + static_cast<size_t>(len) <= size);
			bool guardOK = true;
			// simple junk guard: the next 3 bytes must not be ASCII '5','P','1'
			if (inBounds && i + 11 <= size) {
				guardOK = !isASCII_5P1(data[i+8], data[i+9], data[i+10]);
			}
			if (evenLength && inRange && inBounds && guardOK) {
				hits.push_back({ static_cast<uint32_t>(i), len });
			}
		}
	}
	return hits;
}

struct Bucket { uint32_t len; std::vector<E5P1Hit> items; };

// Compute basic statistics over a vector of numbers (double)
inline std::tuple<double,double,double,double> quartiles(std::vector<double> v) {
	if (v.empty()) return {0.0,0.0,0.0,0.0};
	std::sort(v.begin(), v.end());
	const size_t n = v.size();
	const auto q_at = [&](double q)->double{
		double idx = q * (n - 1);
		size_t i = static_cast<size_t>(idx);
		double frac = idx - static_cast<double>(i);
		if (i + 1 < n) return v[i] * (1.0 - frac) + v[i+1] * frac;
		return v[i];
	};
	double q1 = q_at(0.25);
	double q2 = q_at(0.50);
	double q3 = q_at(0.75);
	return {q1,q2,q3,v.back()};
}

inline std::pair<double,double> mean_stdev(const std::vector<double>& v) {
	if (v.empty()) return {0.0, 0.0};
	double sum = 0.0;
	for (double x : v) sum += x;
	double mean = sum / static_cast<double>(v.size());
	double var = 0.0;
	for (double x : v) var += (x - mean) * (x - mean);
	var /= static_cast<double>(v.size());
	return {mean, std::sqrt(var)};
}

// Group hits into exact-length buckets, optionally returning only buckets whose length is within
// the Tukey inlier range [Q1-1.5*IQR, Q3+1.5*IQR] computed across all hit lengths.
inline std::vector<Bucket> bucketByLen(const std::vector<E5P1Hit>& hits, bool applyIQRFilter = false) {
	std::map<uint32_t, std::vector<E5P1Hit>> byLen;
	byLen.clear();
	std::vector<double> lens;
	lens.reserve(hits.size());
	for (const auto& h : hits) {
		byLen[h.len].push_back(h);
		lens.push_back(static_cast<double>(h.len));
	}

	double lower = 0.0, upper = std::numeric_limits<double>::infinity();
	if (applyIQRFilter && !lens.empty()) {
		double q1, q2, q3, maxv;
		std::tie(q1, q2, q3, maxv) = quartiles(lens);
		double iqr = q3 - q1;
		lower = q1 - 1.5 * iqr;
		upper = q3 + 1.5 * iqr;
	}

	std::vector<Bucket> out;
	out.reserve(byLen.size());
	for (auto& kv : byLen) {
		uint32_t L = kv.first;
		if (applyIQRFilter) {
			double Ld = static_cast<double>(L);
			if (!(Ld >= lower && Ld <= upper)) continue; // treat as non-preset; drop
		}
		Bucket b{L, std::move(kv.second)};
		out.push_back(std::move(b));
	}
	std::sort(out.begin(), out.end(), [](const Bucket& a, const Bucket& b){ return a.len < b.len; });
	return out;
}

// Choose dominant bucket among inliers: bucket with maximum count; tie-break on smaller length.
inline std::tuple<bool, Bucket, double, double> dominantBucket(const std::vector<E5P1Hit>& hits) {
	auto buckets = bucketByLen(hits, /*applyIQRFilter*/ true);
	if (buckets.empty()) return {false, Bucket{}, 0.0, 0.0};
	const Bucket* best = &buckets[0];
	for (const auto& b : buckets) {
		if (b.items.size() > best->items.size()) best = &b;
		else if (b.items.size() == best->items.size() && b.len < best->len) best = &b;
	}
	// Stats across all inlier lengths
	std::vector<double> inlierLens;
	inlierLens.reserve(hits.size());
	for (const auto& b : buckets) {
		for (const auto& it : b.items) inlierLens.push_back(static_cast<double>(it.len));
	}
	auto ms = mean_stdev(inlierLens);
	return {true, *best, ms.first, ms.second};
}

} // namespace x3


