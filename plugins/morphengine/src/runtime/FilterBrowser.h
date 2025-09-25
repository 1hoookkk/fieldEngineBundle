#pragma once
#include <string>
#include <vector>
#include <zplane_engine/DspBridge.h>
#include <zplane_models/PackLoader.h>
#include <zplane_models/EmuMapConfig.h>
#include "../../../_shared/sysex/vendors/ProteusLayerFilter.h"

namespace fe { namespace morphengine {

class FilterBrowser {
public:
  bool loadPack(const std::string& packPath, std::string* error = nullptr);

  // Set/Get current index (wraps into range). Returns true if applied.
  bool applyCurrent(pe::DspBridge& bridge, const fe::morphengine::EmuMapConfig& cfg) const;
  bool next(pe::DspBridge& bridge, const fe::morphengine::EmuMapConfig& cfg);
  bool prev(pe::DspBridge& bridge, const fe::morphengine::EmuMapConfig& cfg);
  bool setIndex(int idx, pe::DspBridge& bridge, const fe::morphengine::EmuMapConfig& cfg);

  int index() const { return index_; }
  size_t size() const { return filters_.size(); }
  const std::string& packPath() const { return packPath_; }

private:
  std::string packPath_;
  std::vector<emu_sysex::LayerFilter14> filters_;
  int index_ = 0;
};

}} // namespace fe::morphengine

