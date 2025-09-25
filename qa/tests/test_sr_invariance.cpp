#include <cstdio>
#include <vector>
#include <juce_dsp/juce_dsp.h>
#include "api/StaticShapeBank.h"
#include "engines/AuthenticEMUEngine.h"
#include "qa/ZPlane_STFTHarness.h"

using namespace zplane::qa;

static void renderAtSR(double sr, int pair, float morph, std::vector<float>& out)
{
    ::StaticShapeBank shapes;
    ::AuthenticEMUEngine emu(shapes);

    juce::AudioBuffer<float> buf(1, 1024);
    buf.clear();

    // 1 kHz sine
    const float f = 1000.0f, twopi = juce::MathConstants<float>::twoPi;
    for (int n=0;n<buf.getNumSamples();++n)
        buf.getWritePointer(0)[n] = std::sin(twopi * f * (float)n / (float)sr);

    emu.prepare(sr, buf.getNumSamples(), 1);
    ZPlaneParams p{};
    p.morph = morph; p.intensity = 0.8f; p.morphPair = pair;
    emu.setParams(p);
    emu.processLinear(buf);

    STFTHarness h; h.prepare(sr, 10, 256);
    h.analyze(buf.getReadPointer(0), buf.getNumSamples(), out);
}

int main()
{
    std::vector<float> A,B,C;
    renderAtSR(44100, 0, 0.5f, A);
    renderAtSR(48000, 0, 0.5f, B);
    renderAtSR(96000, 0, 0.5f, C);

    // Find dominant bin in each
    auto peak = [](const std::vector<float>& v){ int i=0; float m=-1e9f;
        for (int k=1;k<(int)v.size()-1;++k){ if (v[k]>m){m=v[k]; i=k;} } return i; };

    int pA=peak(A), pB=peak(B), pC=peak(C);
    int dAB = std::abs(pA - pB);
    int dAC = std::abs(pA - pC);

    std::printf("peaks: 44.1=%d  48=%d  96=%d  (ΔAB=%d, ΔAC=%d)\n", pA,pB,pC,dAB,dAC);
    // pass if within ±1 bin
    return (dAB <= 1 && dAC <= 2) ? 0 : 1;
}