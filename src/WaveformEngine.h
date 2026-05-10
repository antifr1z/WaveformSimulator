#pragma once

#include "PulseShape.h"
#include "Transition.h"

#include <vector>
#include <string>

struct WaveformParams {
    std::string pattern;        // NRZ bit pattern (0s and 1s)
    double      samplingPeriod; // Sampling period in nanoseconds
    double      bitDuration;    // Duration of each bit in nanoseconds
    PulseType   pulseType;      // Pulse shape function
    double      pw50;           // Pulse width at 50% amplitude (ns)
    double      tailDuration;   // Pulse truncation cutoff (ns)
    int         numThreads;     // Number of threads (0 = auto)
};

struct WaveformResult {
    std::vector<double> samples;    // Computed waveform amplitude values
    int                 numSamples; // Total number of sample points
    double              duration;   // Total waveform duration in nanoseconds
    double              minValue;   // Minimum amplitude (for rendering)
    double              maxValue;   // Maximum amplitude (for rendering)
    double              computeTimeMs; // Computation time in milliseconds
    int                 threadsUsed;   // Actual thread count used
};

// Singleton class that manages waveform computation.
// Only one engine instance exists in the program — accessed via instance().
class WaveformEngine {
public:
    // Singleton access — Meyer's singleton (thread-safe in C++11+)
    static WaveformEngine& instance();

    // Deleted copy and move — enforce single instance
    WaveformEngine(const WaveformEngine&)            = delete;
    WaveformEngine& operator=(const WaveformEngine&) = delete;
    WaveformEngine(WaveformEngine&&)                 = delete;
    WaveformEngine& operator=(WaveformEngine&&)      = delete;

    // Generate the read-back waveform for the given parameters.
    // Uses multithreaded computation when numThreads > 1.
    [[nodiscard]] WaveformResult generate(const WaveformParams& params);

private:
    WaveformEngine() = default;
    ~WaveformEngine() = default;

    // Cached pulse constants — reused if pw50 hasn't changed
    double                        m_cachedPw50 = -1.0;
    PulseConstants                m_cachedConstants{1.0};

    // Resolved thread count from hardware_concurrency (queried once)
    int                           m_hardwareThreads = 0;

    // Minimum samples before multithreading kicks in
    static constexpr int kMinSamplesForThreading = 1000;

    [[nodiscard]] int resolveThreadCount(int requested, int numSamples);
};
