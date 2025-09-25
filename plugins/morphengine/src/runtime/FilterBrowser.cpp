#include "FilterBrowser.h"
#include <algorithm>

namespace fe { namespace morphengine {

bool FilterBrowser::loadPack(const std::string& packPath, std::string* error) {
  packPath_ = packPath;
  filters_.clear();
  index_ = 0;

  PackView view;
  std::string err;
  if (!loadPackFile(packPath, view, &err)) {
    if (error) *error = err;
    return false;
  }
  for (const auto& e : view.entries) {
    if (e.type == 0x10 && e.sub == 0x22 && e.length == 14 && e.data) {
      filters_.push_back(emu_sysex::parseLayerFilter14(e.data));
    }
  }
  if (filters_.empty()) {
    if (error) *error = "No 0x10/0x22 entries found in pack: " + packPath;
    return false;
  }
  return true;
}

bool FilterBrowser::applyCurrent(pe::DspBridge& bridge, const fe::morphengine::EmuMapConfig& cfg) const {
  if (filters_.empty()) return false;
  int i = index_;
  if (i < 0) i = 0; if (i >= (int)filters_.size()) i = (int)filters_.size() - 1;
  bridge.apply(filters_[i], cfg);
  return true;
}

bool FilterBrowser::next(pe::DspBridge& bridge, const fe::morphengine::EmuMapConfig& cfg) {
  if (filters_.empty()) return false;
  index_ = (index_ + 1) % (int)filters_.size();
  return applyCurrent(bridge, cfg);
}

bool FilterBrowser::prev(pe::DspBridge& bridge, const fe::morphengine::EmuMapConfig& cfg) {
  if (filters_.empty()) return false;
  index_ = (index_ - 1 + (int)filters_.size()) % (int)filters_.size();
  return applyCurrent(bridge, cfg);
}

bool FilterBrowser::setIndex(int idx, pe::DspBridge& bridge, const fe::morphengine::EmuMapConfig& cfg) {
  if (filters_.empty()) return false;
  int n = (int)filters_.size();
  if (n <= 0) return false;
  // wrap
  index_ = ((idx % n) + n) % n;
  return applyCurrent(bridge, cfg);
}

}} // namespace fe::morphengine

