#pragma once
#include <string>
#include <zplane_models/PackLoader.h>
#include <zplane_models/EmuMapConfig.h>
#include <sysex/vendors/ProteusLayerFilter.h>

namespace pe { class DspBridge; }

namespace fe { namespace morphengine {

// Loads the first Layer Filter (type=0x10, sub=0x22) from a ZPK1 pack and applies via bridge.
// Returns true if applied.
bool applyFirstLayerFilterFromPack(const std::string& packPath, pe::DspBridge& bridge, const fe::morphengine::EmuMapConfig& cfg);

// Load first ZMF1 entry from a ZPK1 pack into the bridge's loader.
bool loadFirstZmf1FromPack(const std::string& packPath, pe::DspBridge& bridge);

// Load a standalone ZMF1 file into the bridge's loader.
bool loadZmf1File(const std::string& zmfPath, pe::DspBridge& bridge);

}} // namespace fe::morphengine
