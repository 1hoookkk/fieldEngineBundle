#pragma once
#include <vector>
#include <cstdint>

namespace fe { namespace morphengine {

class ProteusHandler {
public:
  // Returns true if the frame was recognized and queued for processing.
  bool onSysExFrame(const std::vector<uint8_t>& frameBytes);
};

}} // namespace fe::morphengine

