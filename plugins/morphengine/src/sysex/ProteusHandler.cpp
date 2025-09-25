#include "ProteusHandler.h"
#include "../../_shared/sysex/Decoder.h"
#include "../../_shared/sysex/vendors/EmuProteus.h"
#include "../../_shared/sysex/vendors/ProteusLayerFilter.h"

namespace fe { namespace morphengine {

bool ProteusHandler::onSysExFrame(const std::vector<uint8_t>& frameBytes) {
  using namespace fe::sysex;
  auto frames = extractFrames(frameBytes.data(), frameBytes.size());
  if (frames.empty()) return false;
  // Handle only first for now
  auto payload = toPayload(frames[0], VendorSpec{std::vector<uint8_t>{0x18}});
  if (payload.bytes.empty()) return false;
  auto msg = proteus::parse(payload.bytes);
  if (!msg) return false;
  // Route by command/subcommand for quick wins
  if (msg->hdr.command == 0x10 && msg->data.size() >= 1) {
    uint8_t sub = msg->data[0];
    if (sub == 0x22) {
      auto lf = emu_sysex::parseLayerFilter(msg->data.data(), msg->data.size());
      if (lf) {
        // TODO: map lf->bytes into internal model params and update DspBridge
        return true;
      }
    }
  }
  return true;
}

}} // namespace fe::morphengine
