#include <zplane_models/PackLoader.h>
#include <fstream>
#include <cstring>

namespace fe { namespace morphengine {

static bool parsePack(const uint8_t* bytes, size_t size, PackView& out) {
  if (size < 8) return false;
  if (std::memcmp(bytes, "ZPK1", 4) != 0) return false;
  const uint8_t* p = bytes + 4;
  uint16_t version = p[0] | (p[1] << 8); p += 2;
  if (version != 1) return false;
  uint16_t count = p[0] | (p[1] << 8); p += 2;
  size_t header = 4 + 2 + 2;
  size_t entrySize = 4 + 2 + 2 + 2 + 4 + 4;
  if (size < header + count * entrySize) return false;
  out.entries.clear();
  out.entries.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    const uint8_t* e = bytes + header + i * entrySize;
    uint32_t id = e[0] | (e[1] << 8) | (e[2] << 16) | (e[3] << 24);
    uint16_t type = e[4] | (e[5] << 8);
    uint16_t sub  = e[6] | (e[7] << 8);
    uint16_t flags= e[8] | (e[9] << 8);
    uint32_t off  = e[10] | (e[11] << 8) | (e[12] << 16) | (e[13] << 24);
    uint32_t len  = e[14] | (e[15] << 8) | (e[16] << 16) | (e[17] << 24);
    if (off + len > size) return false;
    PackEntry pe{ id, type, sub, flags, bytes + off, len };
    out.entries.emplace_back(pe);
  }
  return true;
}

bool loadPackFile(const std::string& path, PackView& out, std::string* error) {
  std::ifstream f(path, std::ios::binary);
  if (!f.good()) {
    if (error) *error = "Failed to open pack: " + path;
    return false;
  }
  f.seekg(0, std::ios::end);
  auto sz = static_cast<size_t>(f.tellg());
  f.seekg(0, std::ios::beg);
  out.buffer.resize(sz);
  f.read(reinterpret_cast<char*>(out.buffer.data()), sz);
  if (!parsePack(out.buffer.data(), out.buffer.size(), out)) {
    if (error) *error = "Invalid pack format: " + path;
    out.buffer.clear();
    out.entries.clear();
    return false;
  }
  return true;
}

}} // namespace fe::morphengine

