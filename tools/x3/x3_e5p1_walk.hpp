#pragma once

#include <cstdint>
#include <vector>

namespace x3 {

// Walk a BE, align=2 TLV blob; returns {tag, off, len} triples.
struct TLV { uint32_t tag; uint32_t off; uint32_t len; };

// Interpret the E5P1 body as a sequence of TLVs.
// Layout per element: BE u16 tag, BE u16 len, then len bytes of payload, 2-byte aligned.
inline std::vector<TLV> walkE5P1(const uint8_t* p, size_t n) {
	std::vector<TLV> v;
	if (!p) return v;
	const uint8_t* cur = p;
	const uint8_t* end = p + n;
	while (cur + 4 <= end) {
		uint16_t tag = (static_cast<uint16_t>(cur[0]) << 8) | static_cast<uint16_t>(cur[1]);
		uint16_t len = (static_cast<uint16_t>(cur[2]) << 8) | static_cast<uint16_t>(cur[3]);
		cur += 4;
		if (cur + len > end) break; // stop on bounds issue
		TLV t;
		t.tag = static_cast<uint32_t>(tag);
		t.off = static_cast<uint32_t>(cur - p);
		t.len = static_cast<uint32_t>(len);
		v.push_back(t);
		// advance with 2-byte alignment
		const size_t adv = (static_cast<size_t>(len) + 1u) & ~static_cast<size_t>(1u);
		if (cur + adv < cur) break; // overflow guard
		cur += adv;
	}
	return v;
}

} // namespace x3


