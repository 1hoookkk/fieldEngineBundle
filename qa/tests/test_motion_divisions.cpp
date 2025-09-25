#include <cmath>
#include <cstdio>
#include <array>

namespace
{
    constexpr std::array<double, 14> baseBeats { 4.0, 2.0, 2.0, 2.0, 1.0, 1.0, 1.0, 0.5, 0.5, 0.5, 0.25, 0.25, 0.25, 0.125 };
    constexpr std::array<double, 14> dotted    { 1.0, 1.0, 1.5, 2.0/3.0, 1.0, 1.5, 2.0/3.0, 1.0, 1.5, 2.0/3.0, 1.0, 1.5, 2.0/3.0, 1.0 };

    double frequencyForDivision(int divIndex, double bpm) noexcept
    {
        divIndex = std::max(0, std::min(13, divIndex));
        const double beats = baseBeats[(size_t) divIndex] * dotted[(size_t) divIndex];
        const double periodSec = (60.0 / std::max(1.0, bpm)) * beats;
        return 1.0 / std::max(1.0e-6, periodSec);
    }
}

int main()
{
    struct Test { int div; double bpm; double expectedHz; };
    const Test tests[] = {
        { 4, 120.0, 2.0 },       // quarter
        { 3, 120.0, 3.0 },       // half triplet -> 3 Hz
        { 5, 90.0, 1.0 },        // quarter dotted at 90 bpm -> 1 Hz
        { 7, 128.0, 4.2666666667 }, // 1/8 at 128 bpm
        { 9, 100.0, 5.3333333333 }, // 1/8 dotted at 100 bpm
        { 12, 140.0, 14.0 }      // 1/16 triplet at 140 bpm
    };

    bool ok = true;
    for (const auto& t : tests)
    {
        const double hz = frequencyForDivision(t.div, t.bpm);
        const double diff = std::abs(hz - t.expectedHz);
        std::printf("div=%d bpm=%.1f -> hz=%.6f (expected %.6f) diff=%.6e\n", t.div, t.bpm, hz, t.expectedHz, diff);
        if (diff > 1.0e-6)
            ok = false;
    }

    return ok ? 0 : 1;
}
