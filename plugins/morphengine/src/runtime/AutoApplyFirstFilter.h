#pragma once
#include <string>
#include <zplane_engine/DspBridge.h>
#include <zplane_models/EmuMapConfig.h>

namespace fe { namespace morphengine {

// Attempts to locate a ZPK1 pack and apply the first 0x10/0x22 entry to the bridge.
// Search order:
// 1) Environment var FE_PACK_PATH (absolute or relative)
// 2) Relative dev path: plugins/_packs/a2k_user/00_0000_HOUSE.bin
// Returns true on success.
bool autoApplyFirstFilter(pe::DspBridge& bridge, const fe::morphengine::EmuMapConfig& cfg);

}} // namespace fe::morphengine

