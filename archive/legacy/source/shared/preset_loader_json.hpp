#pragma once

#include <string>
#include <vector>
#include <optional>
#include <functional>

namespace fe {

struct FilterBundle {
	int   modelId   = -1;
	float cutoff01  = 0.5f;
	float res01     = 0.0f;
	float t1        = 0.5f;
	float t2        = 0.5f;
	bool  present   = false;
};

struct LFOBundle {
	std::string id;
	std::optional<float> rate_hz;
	std::optional<std::string> division; // tempo-sync: "1/4", "1/8", etc.
	std::string shape = "sine";
	bool tempo_sync = false; // true if using division instead of rate_hz
};

struct ENVBundle {
	std::string id;
	std::optional<float> a, d, s, r;
};

struct ModBundle {
	std::string src;
	std::string dst; // filter.cutoff | filter.resonance | filter.t1 | filter.t2
	std::optional<float> depth; // normalized; clamped to [-1..1]; omitted if ~0
	std::string pol = "unipolar";
};

struct PresetBundle {
	std::string name;
	std::string category;
	FilterBundle filter;
	std::vector<LFOBundle> lfos;
	std::vector<ENVBundle> envs;
	std::vector<ModBundle> mods;
};

struct BankBundle {
	std::string bankName;
	std::string sourcePath;
	std::vector<PresetBundle> presets;
};

// Loads JSON from disk. Safe for UI/message thread; NOT for audio thread.
bool loadBankJSON(const std::string& path, BankBundle& out, std::string& err);

// Map model string to internal id (expand as needed)
int modelIdFromString(const std::string& s);

struct EngineApply {
	std::function<void(int modelId, float cutoff01, float res01)> setFilter;
	std::function<void(float t1, float t2)>                       setMorphTargets;
	std::function<void(float ms)>                                  setCrossfadeMs;
	std::function<void(const LFOBundle&)>                          ensureLFO;
	std::function<void(const ENVBundle&)>                          ensureENV;
	std::function<void(const ModBundle&)>                          connectMod;
};

// Apply a single preset to your engine (message thread only)
void applyPresetToEngine(const PresetBundle& p, const EngineApply& api);

} // namespace fe


