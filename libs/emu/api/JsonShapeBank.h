#pragma once
#include "IShapeBank.h"
#include <array>
#include <vector>
#include <unordered_map>
#include <string>

#if __has_include(<juce_core/juce_core.h>)
 #include <juce_core/juce_core.h>
#endif

// JsonShapeBank loads two 48 kHz-referenced shape files (A/B) and provides pair indices by id.
// Schema expected (from rich data):
// {
//   "sampleRateRef": 48000,
//   "shapes": [ { "id": "vowel_pair", "poles": [ {"r":..., "theta":...} x 6 ] }, ... ]
// }
struct JsonShapeBank final : IShapeBank
{
    // pairIds lists the pair ids to expose as morph pairs (order is UI order)
    // Defaults to EMU-style core pairs
    explicit JsonShapeBank(
        const juce::File& fileA,
        const juce::File& fileB,
        const std::vector<std::string>& pairIds = { "vowel_pair", "bell_pair", "low_pair" })
    {
    #if __has_include(<juce_core/juce_core.h>)
        loadFile(fileA, indexA, idToIndexA, shapesA48k);
        loadFile(fileB, indexB, idToIndexB, shapesB48k);
        // Build morph pairs for requested ids
        for (const auto& pid : pairIds)
        {
            const auto itA = idToIndexA.find(pid);
            const auto itB = idToIndexB.find(pid);
            if (itA != idToIndexA.end() && itB != idToIndexB.end())
                pairList.emplace_back(itA->second, itB->second);
        }
    #else
        (void)fileA; (void)fileB; (void)pairIds;
    #endif
    }

    std::pair<int,int> morphPairIndices (int pairIndex) const override
    {
        if (pairList.empty()) return { 0, 0 };
        const int n = (int) pairList.size();
        const int i = pairIndex < 0 ? 0 : (pairIndex >= n ? n - 1 : pairIndex);
        return pairList[(size_t)i];
    }

    const std::array<float,12>& shape (int index) const override
    {
        const int N = (int) shapesA48k.size();
        const int i = index < 0 ? 0 : (index >= N ? N - 1 : index);
        return shapesA48k[(size_t)i]; // note: A/B share ids; engine uses pair indices to fetch A and B
    }

    int numPairs()  const override { return (int) pairList.size(); }
    int numShapes() const override { return (int) shapesA48k.size(); }

private:
#if __has_include(<juce_core/juce_core.h>)
    static void loadFile(const juce::File& file,
                         std::vector<std::string>& outIndex,
                         std::unordered_map<std::string,int>& outIdToIndex,
                         std::vector<std::array<float,12>>& outShapes)
    {
        outIndex.clear(); outIdToIndex.clear(); outShapes.clear();
        if (!file.existsAsFile()) return;
        auto parsed = juce::JSON::parse(file);
        if (!parsed.isObject()) return;
        const auto shapes = parsed.getProperty("shapes", juce::var());
        if (!shapes.isArray()) return;
        int idx = 0;
        for (auto& sv : *shapes.getArray())
        {
            if (!sv.isObject()) continue;
            const auto id = sv.getProperty("id", juce::var()).toString().toStdString();
            const auto polesVar = sv.getProperty("poles", juce::var());
            if (!polesVar.isArray()) continue;
            std::array<float,12> polar12{};
            auto* pa = polesVar.getArray();
            const int n = std::min(6, (int) pa->size());
            for (int i = 0; i < n; ++i)
            {
                auto pv = (*pa)[i];
                const float r  = (float) pv.getProperty("r", 0.95f);
                const float th = (float) pv.getProperty("theta", 0.0f);
                polar12[2*i+0] = std::clamp(r, 0.0f, 0.999999f);
                polar12[2*i+1] = th;
            }
            outIdToIndex[id] = idx++;
            outIndex.push_back(id);
            outShapes.push_back(polar12);
        }
    }
#endif

    // A: reference side A shapes by id; B: reference side B shapes
    std::vector<std::array<float,12>> shapesA48k;
    std::vector<std::array<float,12>> shapesB48k;
    std::vector<std::string> indexA, indexB;
    std::unordered_map<std::string,int> idToIndexA, idToIndexB;

    // pairs map into A and B by index
    std::vector<std::pair<int,int>> pairList;
};
