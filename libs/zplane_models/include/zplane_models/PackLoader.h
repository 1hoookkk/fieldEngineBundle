#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace fe { namespace morphengine {

struct PackEntry {
  uint32_t id;
  uint16_t type; // command
  uint16_t sub;  // subcommand if any
  uint16_t flags;
  const uint8_t* data;
  uint32_t length;
};

struct PackView {
  std::vector<uint8_t> buffer; // owns file bytes
  std::vector<PackEntry> entries;
};

// Load a ZPK1 pack from disk into memory and expose entries.
bool loadPackFile(const std::string& path, PackView& out, std::string* error = nullptr);

}} // namespace fe::morphengine

