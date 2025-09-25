#pragma once
#include <array>
#include "libs/pitchengine_dsp/include/AuthenticEMUZPlane.h"

struct AuthenticEMUZPlaneTestProbe
{
    static std::array<AuthenticEMUZPlane::PolePair, 6> poles(const AuthenticEMUZPlane& plane)
    {
        std::array<AuthenticEMUZPlane::PolePair, 6> out{};
        for (size_t i = 0; i < out.size(); ++i)
            out[i] = plane.polesFs[i];
        return out;
    }

    static std::array<AuthenticEMUZPlane::BiquadSection, 6> sectionsL(const AuthenticEMUZPlane& plane)
    {
        std::array<AuthenticEMUZPlane::BiquadSection, 6> out{};
        for (size_t i = 0; i < out.size(); ++i)
            out[i] = plane.sectionsL[i];
        return out;
    }

    static std::array<AuthenticEMUZPlane::BiquadSection, 6> sectionsR(const AuthenticEMUZPlane& plane)
    {
        std::array<AuthenticEMUZPlane::BiquadSection, 6> out{};
        for (size_t i = 0; i < out.size(); ++i)
            out[i] = plane.sectionsR[i];
        return out;
    }
};
