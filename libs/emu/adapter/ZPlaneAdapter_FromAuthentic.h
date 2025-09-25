#pragma once
#include <vector>
#include <array>
#include <algorithm>
#include "ZPlaneCoefficientBank.h"

// Adapter: AUTHENTIC_EMU_SHAPES (or JSON) -> std::vector<zplane::Model>
// - Path A: from in-memory [r,theta]*6 arrays (AUTHENTIC_EMU_SHAPES style)
// - Path B: from JSON files (audity_shapes_*_48k.json schema) [optional JUCE]

namespace zplane::adapter
{
    inline Model modelFromPolar12 (const float polar12[12], float overallGain = 1.0f)
    {
        Model m{};
        m.numSections = 6;
        for (int i = 0; i < 6; ++i)
        {
            const float r  = std::clamp(polar12[2*i + 0], 0.0f, 0.999999f);
            const float th = polar12[2*i + 1];
            m.s[(size_t)i] = SectionPolar { r, th, 0.0f, 0.0f, 1.0f }; // zeros/gain neutral
        }
        m.overallGain = overallGain;
        return m;
    }

    // --- Path A: From AUTHENTIC_EMU_SHAPES-style memory table ----------------
    // shapes: pointer to [numShapes][12] (r,theta)*6 at 48k reference
    inline std::vector<Model> bankFromAuthentic (const float (*shapes)[12], size_t numShapes)
    {
        std::vector<Model> out;
        out.reserve(numShapes);
        for (size_t i = 0; i < numShapes; ++i)
            out.push_back(modelFromPolar12(shapes[i], 1.0f));
        return out;
    }

    // --- Path B: From JSON (optional; requires JUCE) -------------------------
    // JSON schema expected:
    // { "sampleRateRef": 48000, "shapes":[ { "id":"...", "poles":[{"r":..,"theta":..} *6] } * ] }
    #if __has_include(<juce_core/juce_core.h>)
    #include <juce_core/juce_core.h>
    inline std::vector<Model> bankFromJsonFile (const juce::File& jsonFile)
    {
        std::vector<Model> out;
        if (!jsonFile.existsAsFile())
            return out;

        auto parsed = juce::JSON::parse(jsonFile);
        if (!parsed.isObject())
            return out;

        auto shapesVar = parsed.getProperty("shapes", juce::var());
        if (!shapesVar.isArray())
            return out;

        for (auto& sv : *shapesVar.getArray())
        {
            if (!sv.isObject()) continue;
            auto polesVar = sv.getProperty("poles", juce::var());
            if (!polesVar.isArray()) continue;

            float polar12[12] {};
            auto* pa = polesVar.getArray();
            const int n = std::min(6, (int) pa->size());
            for (int i = 0; i < n; ++i)
            {
                auto pv = (*pa)[i];
                const float r  = (float) pv.getProperty("r", 0.95f);
                const float th = (float) pv.getProperty("theta", 0.0f);
                polar12[2*i + 0] = std::clamp(r, 0.0f, 0.999999f);
                polar12[2*i + 1] = th;
            }
            out.push_back(modelFromPolar12(polar12, 1.0f));
        }
        return out;
    }
    #endif

    // OPTIONAL bank helper (add this method to your ZPlaneCoefficientBank):
    //
    // void ZPlaneCoefficientBank::loadFromModels(const std::vector<Model>& v)
    // {
    //     modelCount_ = (uint16_t) std::min(v.size(), models_.size());
    //     for (size_t i = 0; i < modelCount_; ++i)
    //         models_[i] = v[i];
    // }
} // namespace zplane::adapter