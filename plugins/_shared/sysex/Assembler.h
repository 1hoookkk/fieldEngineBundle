#pragma once
#include <cstdint>
#include <vector>
#include <optional>

namespace fe { namespace sysex {

struct Chunk {
  uint32_t id = 0;         // program/sample/model id
  uint16_t seq = 0;        // sequence number
  uint16_t total = 0;      // total chunks
  std::vector<uint8_t> data;
};

struct AssemblyState {
  uint32_t id = 0;
  uint16_t expectedTotal = 0;
  std::vector<std::vector<uint8_t>> parts; // by seq
};

// Push a chunk; returns contiguous assembled buffer when complete.
std::optional<std::vector<uint8_t>> push(AssemblyState& st, const Chunk& c);

}} // namespace fe::sysex

