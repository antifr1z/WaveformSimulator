#pragma once

#include <cmath>
#include <optional>
#include <string>

enum class PulseType {
    Lorentz,
    Gauss,
    Sinc
};

// Named constants for pulse shape formulas
constexpr double kLorentzFactor = 4.0;
constexpr double kGaussDenomFactor = 4.0;
constexpr double kSincBandwidth = 3.791;

[[nodiscard]] inline std::optional<PulseType> pulseTypeFromString(const std::string& name) {
    if (name == "lorentz") return PulseType::Lorentz;
    if (name == "gauss")   return PulseType::Gauss;
    if (name == "sinc")    return PulseType::Sinc;
    return std::nullopt;
}

[[nodiscard]] inline constexpr const char* pulseTypeToString(PulseType type) noexcept {
    switch (type) {
        case PulseType::Lorentz: return "Lorentz";
        case PulseType::Gauss:   return "Gauss";
        case PulseType::Sinc:    return "Sinc";
    }
    return "Unknown";
}

// Precomputed constants for a given pw50 to avoid redundant work in the hot loop.
struct PulseConstants {
    double pw50Sq;          // pw50 * pw50
    double gaussDenom;      // pw50^2 / (4 * ln(2))
    double sincScale;       // kSincBandwidth / pw50

    explicit PulseConstants(double pw50)
        : pw50Sq(pw50 * pw50)
        , gaussDenom(pw50 * pw50 / (kGaussDenomFactor * std::log(2.0)))
        , sincScale(kSincBandwidth / pw50)
    {}
};

// Evaluate the pulse function at time offset t (nanoseconds from transition center).
// Uses precomputed constants for maximum performance.
// This function is called millions of times — keep it branch-minimal and inline.
[[nodiscard]] inline double evaluatePulse(PulseType type, double t, const PulseConstants& c) noexcept {
    switch (type) {
        case PulseType::Lorentz:
            return c.pw50Sq / (kLorentzFactor * t * t + c.pw50Sq);

        case PulseType::Gauss:
            return std::exp(-(t * t) / c.gaussDenom);

        case PulseType::Sinc: {
            if (t == 0.0) return 1.0;
            const double x = c.sincScale * t;
            return std::sin(x) / x;
        }
    }
    return 0.0;
}
