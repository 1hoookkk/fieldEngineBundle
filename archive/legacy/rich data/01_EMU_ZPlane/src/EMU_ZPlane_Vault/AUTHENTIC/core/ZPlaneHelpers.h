#pragma once
#include <complex>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ZPlaneHelpers {

// Convert a pole (complex) to stable biquad denominator coefficients
inline void polesToDenormCoeffs(std::complex<double> p, double& a1, double& a2)
{
    double r = std::abs(p);
    double theta = std::atan2(p.imag(), p.real());
    const double rMax = 0.999999;
    if (r > rMax) r = rMax;
    a1 = -2.0 * r * std::cos(theta);
    a2 = r * r;
}

// Convert fc (Hz) and Q into a complex pole pair: p = r * exp(j*theta)
// Uses bandwidth-based radius: r = exp(-pi * fc/(Q*fs))
inline std::complex<double> poleFromFcQ(double fc, double Q, double fs)
{
    if (fc <= 0.0) return std::complex<double>(0.0, 0.0);
    double theta = 2.0 * M_PI * fc / fs;

    // Avoid division by zero Q
    double r = 0.999999;
    if (Q > 0.0) {
        double bw = fc / Q; // Hz
        r = std::exp(-M_PI * bw / fs);
        if (r >= 0.999999) r = 0.999999;
        if (r <= 0.0) r = 1e-6;
    }
    return std::polar(r, theta);
}

// Interpolate two complex poles in polar coords, t in [0,1]
inline std::complex<double> interpPole(const std::complex<double>& p0, const std::complex<double>& p1, double t)
{
    double r0 = std::abs(p0), r1 = std::abs(p1);
    double th0 = std::atan2(p0.imag(), p0.real());
    double th1 = std::atan2(p1.imag(), p1.real());
    double dth = th1 - th0;
    if (dth > M_PI) dth -= 2.0 * M_PI;
    if (dth < -M_PI) dth += 2.0 * M_PI;
    double r = (1.0 - t) * r0 + t * r1;
    double th = th0 + t * dth;
    const double rMax = 0.999999;
    if (r > rMax) r = rMax;
    return std::polar(r, th);
}

// Enhanced biquad coefficient calculation with numerator (feed-forward) coefficients
inline void calculateLowpassCoeffs(double fc, double Q, double fs,
                                 double& b0, double& b1, double& b2,
                                 double& a1, double& a2)
{
    if (fc <= 0.0 || Q <= 0.0 || fs <= 0.0) {
        // Bypass state
        b0 = 1.0; b1 = 0.0; b2 = 0.0;
        a1 = 0.0; a2 = 0.0;
        return;
    }

    const double omega = 2.0 * M_PI * fc / fs;
    const double sin_w = std::sin(omega);
    const double cos_w = std::cos(omega);
    const double alpha = sin_w / (2.0 * Q);
    const double a0 = 1.0 + alpha;

    if (std::abs(a0) < 1e-15) {
        // Near zero a0 - bypass state
        b0 = 1.0; b1 = 0.0; b2 = 0.0;
        a1 = 0.0; a2 = 0.0;
        return;
    }

    // Standard RBJ lowpass coefficients
    const double nb0 = (1.0 - cos_w) * 0.5;
    const double nb1 = (1.0 - cos_w);
    const double nb2 = (1.0 - cos_w) * 0.5;
    const double na1 = -2.0 * cos_w;
    const double na2 = 1.0 - alpha;

    // Normalize
    b0 = nb0 / a0;
    b1 = nb1 / a0;
    b2 = nb2 / a0;
    a1 = na1 / a0;
    a2 = na2 / a0;

    // Validate coefficients
    if (!(std::isfinite(b0) && std::isfinite(b1) && std::isfinite(b2) &&
          std::isfinite(a1) && std::isfinite(a2))) {
        // Fallback to bypass
        b0 = 1.0; b1 = 0.0; b2 = 0.0;
        a1 = 0.0; a2 = 0.0;
    }
}

} // namespace ZPlaneHelpers