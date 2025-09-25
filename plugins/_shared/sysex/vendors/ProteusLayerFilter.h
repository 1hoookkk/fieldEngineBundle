#pragma once
#include <cstdint>
#include <optional>
#include <array>
#include <string>

namespace emu_sysex {

struct LayerFilter14 {
  uint8_t filterType = 0;
  uint8_t cutoff = 0;
  uint8_t q = 0;
  uint8_t morphIndex = 0;
  uint8_t morphDepth = 0;
  int8_t tilt = 0;
  std::array<uint8_t, 8> reserved{};

  std::string debug() const;
};

inline LayerFilter14 parseLayerFilter14(const uint8_t* data14) {
  LayerFilter14 f{};
  f.filterType = data14[0];
  f.cutoff = data14[1];
  f.q = data14[2];
  f.morphIndex = data14[3];
  f.morphDepth = data14[4];
  f.tilt = static_cast<int8_t>(data14[5]);
  for (int i = 0; i < 8; ++i) f.reserved[i] = data14[6 + i];
  return f;
}

// Parses a presumed Layer Filter section (subcmd 0x22) payload bytes (length >= 1+14).
// Expects first byte of data to be subcommand, followed by section bytes, then checksum.
inline std::optional<LayerFilter14> parseLayerFilter(const uint8_t* data, size_t len) {
  if (!data || len < 1 + 14) return std::nullopt;
  if (data[0] != 0x22) return std::nullopt;
  return parseLayerFilter14(data + 1);
}

} // namespace emu_sysex
