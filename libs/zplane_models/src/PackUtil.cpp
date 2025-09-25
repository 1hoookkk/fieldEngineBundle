#include <zplane_models/PackUtil.h>
#include <zplane_engine/DspBridge.h>
#include <fstream>
#include <vector>
#include <cstring>

namespace fe { namespace morphengine {

bool applyFirstLayerFilterFromPack(const std::string& packPath, pe::DspBridge& bridge, const fe::morphengine::EmuMapConfig& cfg) {
  PackView view;
  std::string err;
  if (!loadPackFile(packPath, view, &err)) {
    return false;
  }
  for (const auto& e : view.entries) {
    if (e.type == 0x10 && e.sub == 0x22 && e.length == 14 && e.data != nullptr) {
      emu_sysex::LayerFilter14 lf = emu_sysex::parseLayerFilter14(e.data);
      bridge.apply(lf, cfg);
      return true;
    }
  }
  return false;
}

bool loadFirstZmf1FromPack(const std::string& packPath, pe::DspBridge& bridge) {
  PackView view;
  std::string err;
  if (!loadPackFile(packPath, view, &err)) return false;
  for (const auto& e : view.entries) {
    if (e.type == 0x90 && e.sub == 0x01 && e.length > 0 && e.data) {
      return bridge.loadZmf1FromMemory(e.data, e.length);
    }
  }
  return false;
}

bool loadZmf1File(const std::string& zmfPath, pe::DspBridge& bridge) {
  std::ifstream f(zmfPath, std::ios::binary);
  if (!f.good()) return false;
  f.seekg(0, std::ios::end);
  auto sz = static_cast<size_t>(f.tellg());
  f.seekg(0, std::ios::beg);
  std::vector<uint8_t> buf(sz);
  f.read(reinterpret_cast<char*>(buf.data()), sz);
  return bridge.loadZmf1FromMemory(buf.data(), buf.size());
}

}} // namespace fe::morphengine
