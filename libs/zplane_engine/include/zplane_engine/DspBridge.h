#pragma once

#include <array>
#include <atomic>
#include <cstddef>

#include "zplane/BiquadCascade.h"
#include "zplane_models/EmuMapConfig.h"
#include "zplane_models/Zmf1Loader.h"

namespace emu_sysex { struct LayerFilter14; }

namespace pe {

struct ZPlaneParams {
    int   modelIndex { 0 };
    float morph { 0.0f };
    float resonance { 0.0f };
    float cutoff { 0.0f };
    float mix { 1.0f };
};

class DspBridge {
public:
    void setSampleRate(float sr) noexcept { sr_ = sr; }
    void reset() noexcept;

    bool loadModelByBinarySymbol(int modelIndex);
    bool loadZmf1FromMemory(const uint8_t* data, size_t size) { return loader_.loadFromMemory(data, size); }

    bool apply(const emu_sysex::LayerFilter14& filter, const fe::morphengine::EmuMapConfig& cfg) noexcept;

    void process(float* const* inOut, int numCh, int numSamp, const ZPlaneParams& params) noexcept;

    static float estimatePassivityScalar(const Biquad5* sections, int numSections, double sampleRate) noexcept;
    static void applyResonanceToSections(Biquad5* sections, int numSections, float resonance) noexcept;

private:
    float sr_ { 48000.0f };
    Zmf1Loader loader_;
    BiquadCascade6 cascadeL_ {};
    BiquadCascade6 cascadeR_ {};
    std::array<float, 2048> dryBuffer_ {};
    int lastModelIndex_ { -1 };
    float passivityGain_ { 1.0f };

    static float mapCutoffToFreq(float norm01) noexcept;
    static float mapResonanceToQ(float norm01) noexcept;

    void mixInPlace(float* const* io, int numCh, int numSamp,
                    const float* dryBuffer, float mixAmount) noexcept;
};

} // namespace pe
