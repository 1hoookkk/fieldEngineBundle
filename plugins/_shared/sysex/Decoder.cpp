#include "Decoder.h"
#include <cstddef>

namespace fe { namespace sysex {

std::vector<Frame> extractFrames(const uint8_t* bytes, size_t len) {
  std::vector<Frame> out;
  if (!bytes || len == 0) return out;
  size_t i = 0;
  while (i < len) {
    // Find start byte
    while (i < len && bytes[i] != 0xF0) ++i;
    if (i >= len) break;
    size_t j = i + 1;
    while (j < len && bytes[j] != 0xF7) ++j;
    if (j < len && bytes[j] == 0xF7) {
      Frame f;
      f.data.assign(bytes + i, bytes + j + 1);
      out.emplace_back(std::move(f));
      i = j + 1;
    } else {
      break; // No terminator; stop
    }
  }
  return out;
}

Payload toPayload(const Frame& f, const VendorSpec& v) {
  Payload p;
  if (f.data.size() < 3) return p;
  if (f.data.front() != 0xF0 || f.data.back() != 0xF7) return p;
  // Copy bytes between F0 .. F7 (exclusive)
  std::vector<uint8_t> inner(f.data.begin() + 1, f.data.end() - 1);
  // If vendor specified, verify prefix matches
  if (!v.manufacturerId.empty()) {
    if (inner.size() < v.manufacturerId.size()) return p;
    for (size_t i = 0; i < v.manufacturerId.size(); ++i) {
      if (inner[i] != v.manufacturerId[i]) return p;
    }
  }
  p.bytes = std::move(inner);
  return p;
}

std::vector<uint8_t> unpack7bit(const uint8_t* data, size_t len) {
  std::vector<uint8_t> out;
  if (!data || len == 0) return out;
  out.reserve((len / 8) * 7);
  size_t i = 0;
  while (i < len) {
    size_t remain = len - i;
    if (remain == 0) break;
    uint8_t msb = data[i];
    size_t block = remain >= 8 ? 8 : remain;
    for (size_t k = 1; k < block; ++k) {
      uint8_t lsb = data[i + k] & 0x7F;
      uint8_t bit = (msb >> (k - 1)) & 0x01;
      out.push_back(static_cast<uint8_t>(lsb | (bit << 7)));
    }
    i += block;
    if (block < 8) break;
  }
  return out;
}

bool validateChecksum(const uint8_t* data, size_t len) {
  // Expect last byte to be checksum; some devices use 0x7F as ignore.
  if (!data || len < 2) return false;
  uint8_t expected = data[len - 1] & 0x7F;
  if (expected == 0x7F) return true;
  uint32_t sum = 0;
  for (size_t i = 0; i + 1 < len; ++i) {
    sum += (data[i] & 0x7F);
  }
  uint8_t chk = static_cast<uint8_t>((~(sum & 0x7F)) & 0x7F);
  return chk == expected;
}

}} // namespace fe::sysex

