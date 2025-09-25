#pragma once
#include <cstdint>
#include <vector>

namespace fe { namespace zplane {

// Minimal, engine-agnostic representation.
struct CoeffFrame {                 // One morph frame of IIR coefficients
  std::vector<float> a;             // feedback
  std::vector<float> b;             // feedforward
};

struct Model {
  uint32_t id = 0;                  // internal id
  std::vector<CoeffFrame> frames;   // precomputed along morph axis
  uint32_t sampleRate = 48000;      // Hz
};

}} // namespace fe::zplane

