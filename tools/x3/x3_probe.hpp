#pragma once

#include <cstdint>
#include <optional>
#include <cmath>

namespace x3 {

// Returns float if bytes at BE offset plausibly a float (via bit-pattern sanity).
inline std::optional<float> probeF32_BE(const uint8_t* p) {
	if (!p) return std::nullopt;
	uint32_t u = (static_cast<uint32_t>(p[0]) << 24) |
	            (static_cast<uint32_t>(p[1]) << 16) |
	            (static_cast<uint32_t>(p[2]) << 8)  |
	             static_cast<uint32_t>(p[3]);
	// Reject signaling NaNs and infinities; accept subnormals sparingly
	uint32_t exp = (u >> 23) & 0xFFu;
	uint32_t mant = u & 0x7FFFFFu;
	if (exp == 0xFFu) return std::nullopt; // Inf/NaN
	// Heuristic range clamp: most control floats are within [-1000, 1000]
	float f;
	std::memcpy(&f, &u, sizeof(uint32_t));
	// If host is little endian, the bytes are in BE order in u; reinterpret via union trick
	union { uint32_t u32; float f32; } cvt { u };
	f = cvt.f32;
	if (!std::isfinite(f)) return std::nullopt;
	if (std::fabs(f) > 1.0e6f) return std::nullopt;
	return f;
}

} // namespace x3


