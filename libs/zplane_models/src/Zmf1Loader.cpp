#include <zplane_models/Zmf1Loader.h>
#include <cstring>
#include <cstdio>

using namespace pe;

static constexpr uint32_t fourCC(const char s[5]) {
    return ((uint32_t)s[0]) | ((uint32_t)s[1] << 8) | ((uint32_t)s[2] << 16) | ((uint32_t)s[3] << 24);
}

bool Zmf1Loader::loadFromMemory(const uint8_t* data, size_t size)
{
    if (!data || size < 16) return false;

    // Read header manually to avoid struct packing issues
    const uint8_t* p = data;
    uint32_t magic = *(const uint32_t*)p; p += 4;
    uint16_t version = *(const uint16_t*)p; p += 2;
    uint16_t modelId = *(const uint16_t*)p; p += 2;
    uint8_t numFrames = *p++;
    uint8_t numSections = *p++;
    uint32_t sampleRateRef = *(const uint32_t*)p; p += 4;
    uint16_t reserved = *(const uint16_t*)p; p += 2;

    // p now points to the start of coefficient data
    const size_t headerSize = p - data;

    // Debug output
    uint32_t expectedMagic = fourCC("ZMF1");
    if (magic != expectedMagic) {
        std::printf("ZMF1 Magic mismatch: got 0x%08X, expected 0x%08X\n", magic, expectedMagic);
        return false;
    }
    if (version != 0x0001) {
        std::printf("ZMF1 Version mismatch: got 0x%04X, expected 0x0001\n", version);
        return false;
    }
    if (numFrames > kMaxFrames || numSections > kMaxSections) {
        std::printf("ZMF1 Invalid counts: frames=%d (max %d), sections=%d (max %d)\n",
                   numFrames, kMaxFrames, numSections, kMaxSections);
        return false;
    }

    modelId_       = (int)modelId;
    numFrames_     = (int)numFrames;
    numSections_   = (int)numSections;
    sampleRateRef_ = (float)sampleRateRef;

    const size_t frameBytes = (size_t)numSections_ * (5u * sizeof(float));
    const size_t needed = headerSize + (size_t)numFrames_ * frameBytes;
    if (size < needed) {
        std::printf("ZMF1 Size validation failed: got %zu bytes, need %zu bytes\n", size, needed);
        std::printf("  Header size: %zu, frame bytes: %zu x %d frames\n", headerSize, frameBytes, numFrames_);
        return false;
    }

    // p already points to coefficient data
    for (int f = 0; f < numFrames_; ++f) {
        for (int s = 0; s < numSections_; ++s) {
            Biquad5 bq{};
            std::memcpy(&bq, p, sizeof(Biquad5));
            frames_[f][s] = bq;
            p += sizeof(Biquad5);
        }
    }
    std::printf("ZMF1 loaded successfully: model %d, %d frames, %d sections\n",
               modelId_, numFrames_, numSections_);
    return true;
}

// For now we assume coefficients are referenced to sampleRateRef_.
// If runtime SR != ref, your AuthenticEMUZPlane already has per-section
// bilinear transform or freq-warped mapping. If not, keep SR fixed at ref.
void Zmf1Loader::getCoefficients(float morph, float /*sr*/,
                                 std::array<Biquad5,kMaxSections>& out) const noexcept
{
    if (numFrames_ <= 1) {
        for (int s = 0; s < numSections_; ++s) out[s] = frames_[0][s];
        return;
    }

    morph = std::fmin(1.f, std::fmax(0.f, morph));
    const float pos = morph * (float)(numFrames_ - 1);
    const int   i0  = (int)pos;
    const int   i1  = (i0 >= numFrames_ - 1) ? i0 : i0 + 1;
    const float t   = pos - (float)i0;

    for (int s = 0; s < numSections_; ++s) {
        const auto& a = frames_[i0][s];
        const auto& b = frames_[i1][s];
        out[s] = {
            lerp(a.b0, b.b0, t),
            lerp(a.b1, b.b1, t),
            lerp(a.b2, b.b2, t),
            lerp(a.a1, b.a1, t),
            lerp(a.a2, b.a2, t),
        };
    }
}