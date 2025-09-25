#pragma once
#include <cstdint>
#include <vector>

namespace fe { namespace sysex {

struct Frame {
  std::vector<uint8_t> data; // raw 0xF0..0xF7 inclusive or payload-only depending on capture
};

struct Payload {
  std::vector<uint8_t> bytes; // unframed, 7-bit unpacked
};

struct VendorSpec {
  // Accept either 1-byte or 3-byte manufacturer IDs
  std::vector<uint8_t> manufacturerId; // {0x7D} or {0x00,0x20,0x33}
};

// Split a stream into SysEx frames (F0..F7). Invalid bytes are ignored.
std::vector<Frame> extractFrames(const uint8_t* bytes, size_t len);

// Verify vendor and strip header/EOX. Returns empty on mismatch.
Payload toPayload(const Frame& f, const VendorSpec& v);

// Typical 7-bit unpack used by many devices: first byte carries MSBs for the next 7 bytes.
// Input length must be a multiple of 8 for strict mode; partial final block allowed otherwise.
std::vector<uint8_t> unpack7bit(const uint8_t* data, size_t len);

// Hook to validate checksums; implementation depends on vendor.
bool validateChecksum(const uint8_t* data, size_t len);

}} // namespace fe::sysex

