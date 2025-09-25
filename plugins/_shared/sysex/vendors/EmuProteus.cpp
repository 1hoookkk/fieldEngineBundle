#include "../Decoder.h"
#include "EmuProteus.h"

namespace fe { namespace sysex { namespace proteus {

std::optional<Message> parse(const std::vector<uint8_t>& payload) {
  if (payload.empty()) return std::nullopt;
  // Expect first byte to be manufacturer id 0x18 for single-byte ID.
  if (payload[0] != kManufacturerId) return std::nullopt;
  if (payload.size() < 4) return std::nullopt;
  Message m{};
  m.hdr.deviceId = payload[1];
  m.hdr.productId = payload[2];
  m.hdr.command = payload[3];
  m.data.assign(payload.begin() + 4, payload.end());
  return m;
}

}}} // namespace fe::sysex::proteus

