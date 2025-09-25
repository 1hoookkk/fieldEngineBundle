// FormantRescue.h
#pragma once
#include <algorithm>
#include <cmath>
#include "AuthenticEMUZPlane.h"

struct FormantRescue {
    void prepare(double fs){ (void)fs; }
    void setStyle(int styleIndex){ style = styleIndex; } // 0=Air,1=Focus,2=Velvet

    void processBlock(AuthenticEMUZPlane& emu, const float* ratio, int N){
        // summarize correction this block
        double sum=0.0; for (int i=0;i<N;++i) sum += std::abs(std::log2(std::max(1e-6f, ratio[i])));
        const float semis = (float)(12.0 * (sum / std::max(1,N))); // avg semitone dev

        // base mapping
        float morph = std::clamp(0.5f + 0.03f * semis * morphSign(), 0.f, 1.f);
        float inten = std::clamp(0.2f + 0.06f * semis, 0.2f, 1.0f);

        // style skew
        switch (style){
            case 0: /*Air*/   morph = std::clamp(morph + 0.10f, 0.f, 1.f); inten *= 0.85f; break;
            case 1: /*Focus*/ morph = std::clamp(morph, 0.f, 1.f);        inten *= 1.00f; break;
            case 2: /*Velvet*/morph = std::clamp(morph - 0.08f, 0.f, 1.f); inten *= 1.15f; break;
        }

        emu.setShapePair(AuthenticEMUZPlane::Shape::VowelAe_Bright, AuthenticEMUZPlane::Shape::VowelOh_Round);
        emu.setMorphPosition(morph);
        emu.setIntensity(inten);
        // coeffs update is inside emu (block-rate)
    }

private:
    int style = 1;
    static inline float morphSign(){ return 1.f; } // +: brighter when shifting up more; keep +1 for simplicity
};