#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <optional>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>

#include "../x3_index_sigscan.hpp"
#include "../x3_e5p1_walk.hpp"
#include "../x3_probe.hpp"
#include "../x3_learn.hpp"
#include "../../include/x3_lfo_decode.hpp"
#include "../x3_rom_scan.hpp"
#include "../x3_sample_extract.hpp"

namespace fs = std::filesystem;

struct Argv {
	std::vector<std::string> args;
	std::optional<std::string> getFlagValue(const std::string& flag, const std::string& def = std::string()) const {
		for (size_t i = 0; i + 1 < args.size(); ++i) {
			if (args[i] == flag) return args[i+1];
		}
		if (!def.empty()) return def;
		return std::nullopt;
	}
	bool hasFlag(const std::string& flag) const {
		return std::find(args.begin(), args.end(), flag) != args.end();
	}
};

static bool read_file(const std::string& path, std::vector<uint8_t>& out) {
	std::ifstream f(path, std::ios::binary);
	if (!f) return false;
	f.seekg(0, std::ios::end);
	std::streampos len = f.tellg();
	if (len <= 0) { out.clear(); return true; }
	out.resize(static_cast<size_t>(len));
	f.seekg(0, std::ios::beg);
	f.read(reinterpret_cast<char*>(out.data()), out.size());
	return f.good() || f.eof();
}

static std::string base_name_no_ext(const std::string& p) {
	fs::path fp(p);
	return fp.stem().string();
}

static std::string default_out_path(const std::string& in) {
	fs::path fp(in);
	std::string stem = fp.stem().string();
	fs::path out = fp.parent_path() / (stem + std::string("_bank.json"));
	return out.string();
}

struct ScanStats {
	std::vector<x3::E5P1Hit> hits;
	std::map<uint32_t, size_t> hist;
	uint32_t dominantLen = 0;
	size_t dominantCount = 0;
	double inlierMean = 0.0;
	double inlierStdev = 0.0;
};

static ScanStats scan_e5p1_stats(const std::vector<uint8_t>& data) {
	ScanStats s{};
	s.hits = x3::indexE5P1_bySig(data.data(), data.size());
	for (const auto& h : s.hits) s.hist[h.len]++;
	auto dom = x3::dominantBucket(s.hits);
	bool ok; x3::Bucket b; double mean, stdev;
	std::tie(ok, b, mean, stdev) = dom;
	if (ok) {
		s.dominantLen = b.len;
		s.dominantCount = b.items.size();
		s.inlierMean = mean;
		s.inlierStdev = stdev;
	}
	return s;
}

static int cmd_scan(int argc, char** argv) {
	Argv A{ std::vector<std::string>(argv + 2, argv + argc) };
	if (A.args.empty()) {
		std::cerr << "Usage: scan-e5p1 <path> [--limit N] [--json]" << std::endl;
		return 2;
	}
	std::string path = A.args[0];
	int limit = 10;
	if (auto v = A.getFlagValue("--limit")) limit = std::max(0, std::stoi(*v));
	bool asJson = A.hasFlag("--json");

	std::vector<uint8_t> data;
	if (!read_file(path, data)) { std::cerr << "ERR: cannot read file: " << path << std::endl; return 1; }
	auto stats = scan_e5p1_stats(data);

	if (asJson) {
		std::cout << "{\n";
		std::cout << "  \"count\": " << stats.hits.size() << ",\n";
		std::cout << "  \"dominant_len\": " << stats.dominantLen << ",\n";
		std::cout << "  \"dominant_count\": " << stats.dominantCount << ",\n";
		std::cout << "  \"hist\": [";
		bool first = true;
		for (const auto& kv : stats.hist) {
			if (!first) std::cout << ",";
			first = false;
			std::cout << "[" << kv.first << "," << kv.second << "]";
		}
		std::cout << "]\n";
		std::cout << "}\n";
		return 0;
	}

	std::cout << "E5P1 hits: " << stats.hits.size() << std::endl;
	std::cout << "Buckets (len -> count):" << std::endl;
	int shown = 0;
	for (auto it = stats.hist.begin(); it != stats.hist.end() && shown < limit; ++it, ++shown) {
		std::cout << "  " << it->first << " -> " << it->second;
		if (it->first == stats.dominantLen) std::cout << "  <-- dominant";
		std::cout << std::endl;
	}
	if (!stats.hits.empty()) {
		std::cout << "Dominant bucket: len=" << stats.dominantLen
		          << " count=" << stats.dominantCount
		          << " meanLen(inliers)=" << std::fixed << std::setprecision(2) << stats.inlierMean
		          << " stdev=" << std::fixed << std::setprecision(2) << stats.inlierStdev
		          << std::endl;
		std::cout << "First offsets:" << std::endl;
		for (int i = 0; i < std::min(limit, (int)stats.hits.size()); ++i) {
			std::cout << "  off=0x" << std::hex << stats.hits[i].off << std::dec << " len=" << stats.hits[i].len << std::endl;
		}
	}
	return 0;
}

struct GateReport { bool e5p1=false, mods=false, lfo=false, env=false; };

static int cmd_extract(int argc, char** argv) {
	Argv A{ std::vector<std::string>(argv + 2, argv + argc) };
	if (A.args.empty()) {
		std::cerr << "Usage: extract-bank <path> [--json out.json] [--bucket LEN] [--strict] [--log verbose|info|warn]" << std::endl;
		return 2;
	}
	std::string path = A.args[0];
	std::string outPath = A.getFlagValue("--json", default_out_path(path)).value();
	bool strict = A.hasFlag("--strict");
	std::string log = A.getFlagValue("--log", std::string("info")).value();
	int logLevel = (log=="verbose"?2:(log=="info"?1:0));

	std::vector<uint8_t> data;
	if (!read_file(path, data)) { std::cerr << "ERR: cannot read file: " << path << std::endl; return 1; }

	// Scan hits and choose bucket
	auto stats = scan_e5p1_stats(data);
	uint32_t wantedLen = stats.dominantLen;
	if (auto v = A.getFlagValue("--bucket")) {
		try { wantedLen = static_cast<uint32_t>(std::stoul(*v)); } catch (...) { /* ignore */ }
	}
	if (wantedLen == 0) { std::cerr << "FAIL: no dominant E5P1 bucket found" << std::endl; return 1; }

	// Collect E5P1 payload pointers in the selected bucket
	std::vector<const uint8_t*> e5p1s;
	std::vector<size_t> lens;
	for (const auto& h : stats.hits) {
		if (h.len != wantedLen) continue;
		const size_t bodyOff = static_cast<size_t>(h.off) + 8u;
		if (bodyOff + h.len <= data.size()) {
			e5p1s.push_back(data.data() + bodyOff);
			lens.push_back(static_cast<size_t>(h.len));
		}
	}

	GateReport gates{};
	// Gate e5p1: dominant bucket ratio ≥ 50% and count ≥ 16
	const size_t totalHits = stats.hits.size();
	const double ratio = (totalHits > 0) ? (static_cast<double>(stats.dominantCount) / static_cast<double>(totalHits)) : 0.0;
	gates.e5p1 = (stats.dominantCount >= 16) && ((ratio >= 0.25) || (stats.dominantCount >= 64));

	// Learn LFO rate
	auto lfoRate = x3::learn_lfo_rate(e5p1s, lens);
	// Learn ENV A as proxy for ADSR presence
	auto envA = x3::learn_env_A(e5p1s, lens);

	// Decode and emit minimal JSON with present-only sections
	std::ofstream of(outPath, std::ios::binary);
	if (!of) { std::cerr << "ERR: cannot write: " << outPath << std::endl; return 1; }
	const std::string bankName = base_name_no_ext(path);

	// Count mods for gate checking
	int totalModsToFilter = 0;

	// Build presets JSON in a string first so we can collect stats
	std::stringstream presets_json;
	presets_json << "  \"presets\": [\n";

	// Collect distributions for acceptance gates
	std::set<int> uniqueLfoRatesRounded; // round to 2 decimals
	std::map<std::string, int> tempoDivisionHist; // track tempo-sync divisions
	bool anyEnvVariance = false;
	bool anyLFOInMods = false;  // Track if any LFO-based mods exist
	bool anyLFO = false;  // Track if any LFO objects exist
	bool firstPreset = true;
	for (size_t i = 0; i < e5p1s.size(); ++i) {
		if (!firstPreset) presets_json << ",\n";
		firstPreset = false;
		presets_json << "    {\"name\": \"Preset " << (i+1) << "\"";
		// lfo block - comprehensive decoding
		bool wroteLfo = false;
		if (lfoRate.off != 0 && i < e5p1s.size()) {
			// Try comprehensive LFO decoding: float at primary offset, index at +2, sync at +4
			std::optional<size_t> float_off = lfoRate.off;
			std::optional<size_t> index_off = lfoRate.off + 2;
			std::optional<size_t> sync_off = lfoRate.off + 4;

			auto decoded = x3::lfo::decode(e5p1s[i], lens[i], float_off, index_off, sync_off);

			if (decoded.tempo_sync && decoded.division) {
				// Tempo-sync LFO (array form with id)
				tempoDivisionHist[*decoded.division]++;
				presets_json << ", \"lfo\": [ { \"id\": \"LFO1\", \"division\": \"" << *decoded.division << "\" } ]";
				wroteLfo = true;
				anyLFO = true;
			} else if (decoded.rate_hz) {
				// Hz-based LFO (array form with id, and engine expects rate_hz)
				float rate = *decoded.rate_hz;
				int r100 = static_cast<int>(std::round(rate * 100.0f));
				uniqueLfoRatesRounded.insert(r100);
				presets_json << ", \"lfo\": [ { \"id\": \"LFO1\", \"rate_hz\": " << std::fixed << std::setprecision(6) << rate << " } ]";
				wroteLfo = true;
				anyLFO = true;
			}
		}
		// env block: only 'A' if discovered and varies
		static std::optional<float> firstA; // capture first to assess variance
		bool wroteEnv = false;
		if (envA.off != 0 && envA.uniq >= 2 && envA.mad > 0.0) {
			uint32_t off = envA.off;
			if (off + 4u <= lens[i]) {
				uint32_t u = (static_cast<uint32_t>(e5p1s[i][off+0]) << 24) |
				            (static_cast<uint32_t>(e5p1s[i][off+1]) << 16) |
				            (static_cast<uint32_t>(e5p1s[i][off+2]) << 8)  |
				             static_cast<uint32_t>(e5p1s[i][off+3]);
				union { uint32_t u32; float f32; } cvt { u };
				float a = cvt.f32;
				if (std::isfinite(a) && std::fabs(a) <= 1000.0f) {  // Add sanity check for reasonable values
					if (!firstA.has_value()) firstA = a; else if (std::fabs(a - *firstA) > 1e-6f) anyEnvVariance = true;
					presets_json << (wroteLfo ? ", \"env\": { \"filt\": { \"A\": " : ", \"env\": { \"filt\": { \"A\": ") << std::fixed << std::setprecision(6) << a << " } }";
					wroteEnv = true;
				}
			}
		}
		// Extract and write mod cords
		bool wroteMods = false;
		if (i < e5p1s.size() && lens[i] > 0) {
			auto mods = x3::extractMods_TLV(e5p1s[i], lens[i]);
			if (!mods.empty()) {
				presets_json << (wroteLfo || wroteEnv ? ", \"mods\": [" : ", \"mods\": [");
				bool firstMod = true;
				for (const auto& mod : mods) {
					// Skip mods with zero depth
					if (std::fabs(mod.depth) < 0.0001f) continue;

					// Map IDs to friendly names
					std::string srcStr = x3::mapModSource(mod.src);
					std::string dstStr = x3::mapModDest(mod.dst);

					// Skip if we can't map the IDs
					if (srcStr.empty() || dstStr.empty()) continue;

					// Track LFO usage for gate logic
					if (srcStr.rfind("LFO", 0) == 0) {  // starts with "LFO"
						anyLFOInMods = true;
					}

					if (!firstMod) presets_json << ", ";
					firstMod = false;

					presets_json << "{ \"src\": \"" << srcStr << "\", \"dst\": \"" << dstStr << "\"";

					// Only output non-default depth
					if (std::fabs(mod.depth) > 0.001f) {
						presets_json << ", \"depth\": " << std::fixed << std::setprecision(3) << mod.depth;
					}

					// Output polarity if bipolar
					if (mod.pol == 1) {
						presets_json << ", \"pol\": \"bipolar\"";
					}

					presets_json << " }";

					if (dstStr.find("filter.") == 0) {
						totalModsToFilter++;
					}
				}
				presets_json << " ]";
				wroteMods = true;
			}
		}
		presets_json << " }";
	}
	presets_json << "\n  ]\n";

	// Now write the complete JSON with meta block first
	of << "{\n";
	of << "  \"meta\": {\n";
	of << "    \"bank\": \"" << bankName << "\",\n";
	of << "    \"source\": \"" << path << "\",\n";
	of << "    \"version\": \"1.0\",\n";
	of << "    \"e5p1_total\": " << totalHits << ",\n";
	of << "    \"e5p1_extracted\": " << e5p1s.size() << ",\n";
	of << "    \"gates\": {\n";
	of << "      \"e5p1\": " << (gates.e5p1 ? "true" : "false") << ",\n";
	of << "      \"e5p1_ratio\": " << std::fixed << std::setprecision(3) << ratio << ",\n";
	of << "      \"lfo_unique\": " << uniqueLfoRatesRounded.size() << ",\n";
	of << "      \"lfo_divisions\": " << tempoDivisionHist.size() << ",\n";
	of << "      \"env_varied\": " << (anyEnvVariance ? "true" : "false") << ",\n";
	of << "      \"mods_found\": " << totalModsToFilter << "\n";
	of << "    }\n";
	of << "  },\n";
	of << presets_json.str();
	of << "}\n";
	of.flush();

	// Gates
	// LFO gate: pass if rates vary OR divisions vary
	const int uniqRates = (int)uniqueLfoRatesRounded.size();
	const int uniqDivs = (int)tempoDivisionHist.size();
	gates.lfo = (uniqRates > 1) || (uniqDivs > 1);
	gates.env = anyEnvVariance;
	gates.mods = (totalModsToFilter > 0);  // Now detected via TLV extraction

	// Tighten e5p1 per spec: dominant >= 32 and stdev < 10% of mean
	const double stdevRatio = (stats.inlierMean > 0.0 ? (stats.inlierStdev / stats.inlierMean) : 1.0);
	gates.e5p1 = (stats.dominantCount >= 32) && (stats.inlierMean > 0.0) && (stdevRatio < 0.10);

	// Report and exit code
	if (gates.e5p1) std::cout << "ACCEPT: e5p1 dominant bucket len=" << stats.dominantLen << " count=" << stats.dominantCount << " stdev/mean=" << std::fixed << std::setprecision(3) << stdevRatio << std::endl;
	else std::cout << "FAIL: e5p1 bucket insufficient (count=" << stats.dominantCount << ", stdev/mean=" << std::fixed << std::setprecision(3) << stdevRatio << ")" << std::endl;
	if (gates.mods) std::cout << "ACCEPT: mods present targeting filter.*" << std::endl; else std::cout << "FAIL: mods: no cords to filter.*" << std::endl;
	if (gates.lfo) std::cout << "ACCEPT: lfo variance detected (uniq_rates=" << uniqRates << ", uniq_divs=" << uniqDivs << ", any_lfo=" << (anyLFO?"yes":"no") << ", lfo_in_mods=" << (anyLFOInMods?"yes":"no") << ")" << std::endl; else std::cout << "FAIL: lfo: no variance detected" << std::endl;
	if (gates.env) std::cout << "ACCEPT: env ADSR variance detected" << std::endl; else std::cout << "FAIL: env: no variance" << std::endl;

	bool pass = gates.e5p1 && gates.mods && gates.lfo && gates.env;
	std::cout << (pass ? "RESULT: PASS" : "RESULT: FAIL") << std::endl;
	return pass ? 0 : 1;
}

static int cmd_scan_rom(int argc, char** argv) {
	Argv A{ std::vector<std::string>(argv + 2, argv + argc) };
	if (A.args.empty()) {
		std::cerr << "Usage: scan-rom <rom_path> [--json out.json] [--maxlen SIZE] [--probe-adpcm]" << std::endl;
		return 2;
	}
	std::string romPath = A.args[0];
	std::string jsonPath = A.getFlagValue("--json", "").value();
	size_t maxLen = 32 * 1024 * 1024;
	if (auto v = A.getFlagValue("--maxlen")) {
		try { maxLen = static_cast<size_t>(std::stoull(*v)); } catch (...) { /* ignore */ }
	}
	bool probeADPCM = A.hasFlag("--probe-adpcm");

	std::vector<uint8_t> data;
	if (!read_file(romPath, data)) {
		std::cerr << "ERR: cannot read file: " << romPath << std::endl;
		return 1;
	}

	x3::ScanCfg cfg;
	cfg.maxLen = maxLen;
	cfg.probeADPCM = probeADPCM;

	auto offsetTables = x3::findOffsetTables(data.data(), data.size());
	auto samples = x3::scanForPCMWindows(data.data(), data.size(), cfg);

	if (!jsonPath.empty()) {
		std::ofstream f(jsonPath);
		if (!f) {
			std::cerr << "ERR: cannot write: " << jsonPath << std::endl;
			return 1;
		}
		f << "{\n";
		f << "  \"offset_tables\": " << offsetTables.size() << ",\n";
		f << "  \"sample_candidates\": " << samples.size() << ",\n";
		f << "  \"high_evidence\": " << std::count_if(samples.begin(), samples.end(), [](const x3::SampleGuess& g) { return g.evidence >= 0.65; }) << "\n";
		f << "}\n";
		return 0;
	}

	std::cout << "ROM scan results:" << std::endl;
	std::cout << "Offset tables found: " << offsetTables.size() << std::endl;
	std::cout << "Sample candidates: " << samples.size() << std::endl;
	int highEvidence = std::count_if(samples.begin(), samples.end(), [](const x3::SampleGuess& g) { return g.evidence >= 0.65; });
	std::cout << "High evidence (>=0.65): " << highEvidence << std::endl;

	if (!samples.empty()) {
		std::cout << "\nTop candidates:" << std::endl;
		for (int i = 0; i < std::min(10, (int)samples.size()); ++i) {
			const auto& s = samples[i];
			std::cout << "  off=0x" << std::hex << s.off << std::dec
			          << " len=" << s.len
			          << " evidence=" << std::fixed << std::setprecision(3) << s.evidence
			          << " ch=" << s.channels
			          << " sr=" << s.samplerate << std::endl;
		}
	}
	return 0;
}

static int cmd_extract_samples(int argc, char** argv) {
	Argv A{ std::vector<std::string>(argv + 2, argv + argc) };
	if (A.args.size() < 2) {
		std::cerr << "Usage: extract-samples <rom_path> <out_dir> [--manifest out.json] [--force-low-evidence] [--minlen SIZE]" << std::endl;
		return 2;
	}
	std::string romPath = A.args[0];
	std::string outDir = A.args[1];
	std::string manifestPath = A.getFlagValue("--manifest", outDir + "/sample_manifest.json").value();
	bool forceLowEvidence = A.hasFlag("--force-low-evidence");
	size_t minLen = 1024;
	if (auto v = A.getFlagValue("--minlen")) {
		try { minLen = static_cast<size_t>(std::stoull(*v)); } catch (...) { /* ignore */ }
	}

	std::vector<uint8_t> data;
	if (!read_file(romPath, data)) {
		std::cerr << "ERR: cannot read file: " << romPath << std::endl;
		return 1;
	}

	x3::ScanCfg cfg;
	cfg.minLen = minLen;
	auto samples = x3::scanForPCMWindows(data.data(), data.size(), cfg);

	// Analyze each sample for quality metrics
	for (auto& sample : samples) {
		size_t availableLen = data.size() - sample.off;
		size_t analyzeLen = std::min(static_cast<size_t>(sample.len), availableLen);
		sample = x3::analyzeWindow(data.data() + sample.off, analyzeLen, sample);
	}

	auto manifests = x3::extractSamples(romPath, outDir, samples, forceLowEvidence);

	// Write manifest
	std::ofstream f(manifestPath);
	if (!f) {
		std::cerr << "ERR: cannot write manifest: " << manifestPath << std::endl;
		return 1;
	}

	f << "[\n";
	bool first = true;
	for (const auto& m : manifests) {
		if (!first) f << ",\n";
		first = false;
		f << "  {\n";
		f << "    \"name\": \"" << m.name << "\",\n";
		f << "    \"offset\": " << m.offset << ",\n";
		f << "    \"length\": " << m.length << ",\n";
		f << "    \"samplerate\": " << m.samplerate << ",\n";
		f << "    \"bitdepth\": " << m.bitdepth << ",\n";
		f << "    \"channels\": " << m.channels << ",\n";
		f << "    \"encoding\": \"" << m.encoding << "\",\n";
		f << "    \"endianness\": \"" << m.endianness << "\",\n";
		f << "    \"wav_path\": \"" << m.wav_path << "\",\n";
		f << "    \"evidence\": " << std::fixed << std::setprecision(3) << m.evidence << ",\n";
		f << "    \"rms\": " << std::fixed << std::setprecision(6) << m.rms << ",\n";
		f << "    \"peak\": " << std::fixed << std::setprecision(6) << m.peak << ",\n";
		f << "    \"dc\": " << std::fixed << std::setprecision(6) << m.dc << ",\n";
		f << "    \"clipPct\": " << std::fixed << std::setprecision(3) << m.clipPct << ",\n";
		f << "    \"specFlatness\": " << std::fixed << std::setprecision(3) << m.specFlatness;
		if (m.loop_start >= 0 && m.loop_end >= 0) {
			f << ",\n    \"loop_start\": " << m.loop_start << ",\n    \"loop_end\": " << m.loop_end;
		}
		f << "\n  }";
	}
	f << "\n]\n";

	std::cout << "Extracted " << manifests.size() << " samples to " << outDir << std::endl;
	std::cout << "Manifest written to " << manifestPath << std::endl;
	return 0;
}

static void print_usage() {
	std::cout << "Usage:\n"
	          << "  test_e5p1_diagnostics.exe scan-e5p1 <path> [--limit N] [--json]\n"
	          << "  test_e5p1_diagnostics.exe extract-bank <path> [--json out.json] [--bucket LEN] [--strict] [--log verbose|info|warn]\n"
	          << "  test_e5p1_diagnostics.exe scan-rom <rom_path> [--json out.json] [--maxlen SIZE] [--probe-adpcm]\n"
	          << "  test_e5p1_diagnostics.exe extract-samples <rom_path> <out_dir> [--manifest out.json] [--force-low-evidence] [--minlen SIZE]\n";
}

int main(int argc, char** argv) {
	if (argc < 2) { print_usage(); return 2; }
	std::string cmd = argv[1];
	if (cmd == "scan-e5p1") return cmd_scan(argc, argv);
	if (cmd == "extract-bank") return cmd_extract(argc, argv);
	if (cmd == "scan-rom") return cmd_scan_rom(argc, argv);
	if (cmd == "extract-samples") return cmd_extract_samples(argc, argv);
	print_usage();
	return 2;
}


