#pragma once
#include "IShapeBank.h"
#include <array>

// Prefer the pitchEngine EMU tables if available
#if __has_include("pitchEngine/source/dsp/EMUTables.h")
  #include "pitchEngine/source/dsp/EMUTables.h"
#elif __has_include("source/dsp/EMUTables.h")
  #include "source/dsp/EMUTables.h"
#elif __has_include("../shapes/EMUAuthenticTables.h")
  #include "../shapes/EMUAuthenticTables.h"
#elif __has_include("shapes/EMUAuthenticTables.h")
  #include "shapes/EMUAuthenticTables.h"
#elif __has_include(<zplane/EMUAuthenticTables.h>)
  #include <zplane/EMUAuthenticTables.h>
#else
  // Minimal shim to keep compilation working; replace with authentic data
  static const float AUTHENTIC_EMU_SHAPES[2][12] = {
      {0.95f, 0.3f, 0.93f, 0.6f, 0.9f, 1.2f, 0.88f, 1.7f, 0.85f, 2.1f, 0.83f, 2.7f},
      {0.96f, 0.35f, 0.94f, 0.65f, 0.91f, 1.25f, 0.89f, 1.75f, 0.86f, 2.15f, 0.84f, 2.75f}
  };
  static const int MORPH_PAIRS[1][2] = {{0, 1}};
#endif

struct StaticShapeBank final : IShapeBank
{
    std::pair<int,int> morphPairIndices (int pairIndex) const override
    {
        const int num = (int) (sizeof(MORPH_PAIRS) / sizeof(MORPH_PAIRS[0]));
        const int idx = (pairIndex < 0 ? 0 : (pairIndex >= num ? num - 1 : pairIndex));
        return { MORPH_PAIRS[idx][0], MORPH_PAIRS[idx][1] };
    }
    const std::array<float,12>& shape (int idx) const override
    {
        const int N = (int) (sizeof(AUTHENTIC_EMU_SHAPES) / sizeof(AUTHENTIC_EMU_SHAPES[0]));
        const int i = (idx < 0 ? 0 : (idx >= N ? N - 1 : idx));
        return *reinterpret_cast<const std::array<float,12>*>(AUTHENTIC_EMU_SHAPES[i]);
    }
    int numPairs()  const override { return (int) (sizeof(MORPH_PAIRS) / sizeof(MORPH_PAIRS[0])); }
    int numShapes() const override { return (int) (sizeof(AUTHENTIC_EMU_SHAPES) / sizeof(AUTHENTIC_EMU_SHAPES[0])); }
};
