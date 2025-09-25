#include "AutoApplyFirstFilter.h"
#include <zplane_models/PackUtil.h>
#include <fstream>
#if __has_include(<juce_core/juce_core.h>)
  #include <juce_core/juce_core.h>
#endif

namespace fe { namespace morphengine {

static bool fileExists(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  return f.good();
}

bool autoApplyFirstFilter(pe::DspBridge& bridge, const fe::morphengine::EmuMapConfig& cfg) {
  std::string candidate;
  // Check for explicit ZMF1 path first
#if __has_include(<juce_core/juce_core.h>)
  {
    juce::String envZ = juce::SystemStats::getEnvironmentVariable("FE_ZMF1_PATH", "");
    if (envZ.isNotEmpty() && fileExists(envZ.toStdString())) {
      if (loadZmf1File(envZ.toStdString(), bridge)) return true;
    }
  }
#else
  if (const char* z = std::getenv("FE_ZMF1_PATH")) {
    std::string zpath = z;
    if (!zpath.empty() && fileExists(zpath)) {
      if (loadZmf1File(zpath, bridge)) return true;
    }
  }
#endif

#if __has_include(<juce_core/juce_core.h>)
  {
    juce::String env = juce::SystemStats::getEnvironmentVariable("FE_PACK_PATH", "");
    if (env.isNotEmpty()) candidate = env.toStdString();
  }
#else
  const char* env = std::getenv("FE_PACK_PATH");
  if (env && *env) candidate = env;
#endif

  if (!candidate.empty() && fileExists(candidate)) {
    // Prefer ZMF1 if present inside the pack
    if (loadFirstZmf1FromPack(candidate, bridge)) return true;
    return applyFirstLayerFilterFromPack(candidate, bridge, cfg);
  }

  // Dev-friendly relative default
  candidate = std::string("plugins/_packs/a2k_user/00_0000_HOUSE.bin");
  if (fileExists(candidate)) {
    if (loadFirstZmf1FromPack(candidate, bridge)) return true;
    return applyFirstLayerFilterFromPack(candidate, bridge, cfg);
  }

  return false;
}

}} // namespace fe::morphengine
