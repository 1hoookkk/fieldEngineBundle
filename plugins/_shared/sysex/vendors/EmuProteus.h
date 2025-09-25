#pragma once
#include <cstdint>
#include <vector>
#include <optional>

namespace fe { namespace sysex { namespace proteus {

static constexpr uint8_t kManufacturerId = 0x18; // E-mu Systems (single-byte ID)

struct Header {
  uint8_t deviceId{};   // per spec
  uint8_t productId{};  // model id per family
  uint8_t command{};    // message command
};

struct Message {
  Header hdr{};
  std::vector<uint8_t> data; // remaining payload bytes after header
};

// Parse a payload (manufacturer byte stripped) into Header + data.
std::optional<Message> parse(const std::vector<uint8_t>& payload);

}}} // namespace fe::sysex::proteus

