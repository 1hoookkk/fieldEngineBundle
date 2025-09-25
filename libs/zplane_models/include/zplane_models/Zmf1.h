#pragma once
#include <cstdint>
#include <vector>

namespace fe { namespace morphengine {

struct Zmf1Model {
  uint16_t version = 1;
  uint16_t biquads = 0;
  uint16_t frames = 0;
  uint16_t sampleRate = 48000;
  // Coefficients layout: frames * biquads * 5 (b0,b1,b2,a1,a2) float32
  std::vector<float> coeffs;
};

// Parse a ZMF1 blob from memory. Returns true on success.
bool parseZmf1(const uint8_t* data, size_t len, Zmf1Model& out);

}} // namespace fe::morphengine

