#include <zplane_models/Zmf1.h>
#include <cstring>

namespace fe { namespace morphengine {

static constexpr char kZmfMagic[4] = { 'Z', 'M', 'F', '1' };

bool parseZmf1(const uint8_t* data, size_t len, Zmf1Model& out) {
  if (!data || len < 4 + 2 + 2 + 2 + 2) return false;
  if (std::memcmp(data, kZmfMagic, 4) != 0) return false;
  const uint8_t* p = data + 4;
  auto rd16 = [&](const uint8_t*& q) -> uint16_t { uint16_t v = uint16_t(q[0]) | uint16_t(q[1]) << 8; q += 2; return v; };
  out.version = rd16(p);
  out.biquads = rd16(p);
  out.frames  = rd16(p);
  out.sampleRate = rd16(p);
  size_t needFloats = size_t(out.frames) * size_t(out.biquads) * 5u;
  size_t needBytes = needFloats * sizeof(float);
  if ((size_t)(p - data) + needBytes > len) return false;
  out.coeffs.resize(needFloats);
  std::memcpy(out.coeffs.data(), p, needBytes);
  return true;
}

}} // namespace fe::morphengine

