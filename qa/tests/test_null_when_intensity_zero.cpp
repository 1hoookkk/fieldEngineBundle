#include <cstdio>
#include <vector>
#include <juce_dsp/juce_dsp.h>
#include "api/StaticShapeBank.h"
#include "engines/AuthenticEMUEngine.h"

int main()
{
    StaticShapeBank shapes;
    AuthenticEMUEngine emu(shapes);

    const double sr = 48000.0;
    const int N = 1024;
    juce::AudioBuffer<float> in(1, N), wet(1, N);
    in.clear(); wet.clear();

    // broadband noise so a null is meaningful
    juce::Random rng(12345);
    for (int n=0;n<N;++n) in.getWritePointer(0)[n] = rng.nextFloat()*2.f-1.f;
    wet.makeCopyOf(in);

    emu.prepare(sr, N, 1);
    ZPlaneParams p{}; // intensity=0 by default
    p.morphPair = 0;
    emu.setParams(p);
    emu.processLinear(wet);

    // null test: intensity=0 should behave like bypass for linear path
    double err=0.0;
    for (int n=0;n<N;++n){
        const double d = (double)wet.getReadPointer(0)[n] - (double)in.getReadPointer(0)[n];
        err += d*d;
    }
    err = std::sqrt(err / N);
    std::printf("null RMS error = %.9f\n", err);
    return (err < 1e-6) ? 0 : 1;
}