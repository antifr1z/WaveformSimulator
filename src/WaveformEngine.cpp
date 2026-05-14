#include "WaveformEngine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>
#include <vector>

namespace {

// Compute waveform samples for a contiguous range [startSample, endSample).
// Each thread calls this independently on its own chunk — no synchronization needed.
void computeChunk(
    double*                         output,
    int                             startSample,
    int                             endSample,
    double                          samplingPeriod,
    double                          tailDuration,
    PulseType                       pulseType,
    const PulseConstants&           constants,
    const std::vector<Transition>&  transitions)
{
    // For each sample, we only need to consider transitions within tailDuration.
    // Since transitions are sorted by time, we can use a sliding window approach.

    const int numTransitions = static_cast<int>(transitions.size());
    int startTransactionIndx = 0; 
    int endTransactionIndx = 0;

    for (int s = startSample; s < endSample; ++s) {
        const double t = s * samplingPeriod;
        double sum = 0.0;

        // Skip transitions that are too old for this sample.
        while (startTransactionIndx < numTransitions && t - transitions[startTransactionIndx].timeNs > tailDuration) {
            ++startTransactionIndx;
        }

        // Advance endTransactionIndx to the first transition too far in the future.
        while (endTransactionIndx < numTransitions &&
               transitions[endTransactionIndx].timeNs <= t + tailDuration) {
            ++endTransactionIndx;
        }

         // Iterate from startTransition forward.
        for (int tr = startTransactionIndx; tr < endTransactionIndx; ++tr) {
            const double dt = t - transitions[tr].timeNs;
            const double pulse = evaluatePulse(pulseType, dt, constants);
            sum += transitions[tr].isPositive ? pulse : -pulse;
        }

        output[s] = sum;
    }
}

} // anonymous namespace

// Meyer's singleton — thread-safe, lazy initialization (C++11 guarantees)
WaveformEngine& WaveformEngine::instance()
{
    static WaveformEngine engine;
    return engine;
}

int WaveformEngine::resolveThreadCount(int requested, int numSamples)
{
    int numThreads = requested;

    if (numThreads <= 0) {
        // Query hardware_concurrency once and cache the result
        if (m_hardwareThreads == 0) {
            m_hardwareThreads = static_cast<int>(std::thread::hardware_concurrency());
            if (m_hardwareThreads <= 0) m_hardwareThreads = 1;
        }
        numThreads = m_hardwareThreads;
    }

    // Don't use more threads than samples
    if (numThreads > numSamples) {
        numThreads = std::max(1, numSamples);
    }

    // For very small workloads, single-thread is faster (avoid thread overhead)
    if (numSamples < WaveformEngine::kMinSamplesForThreading) {
        numThreads = 1;
    }

    return numThreads;
}

WaveformResult WaveformEngine::generate(const WaveformParams& params)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    // Parse pattern and detect transitions
    const std::string cleanBits = cleanPattern(params.pattern);
    const auto transitions = detectTransitions(cleanBits, params.bitDuration);

    // Calculate waveform dimensions
    const double duration = static_cast<double>(cleanBits.size()) * params.bitDuration;
    const int numSamples = static_cast<int>(std::ceil(duration / params.samplingPeriod));

    // Allocate output buffer
    std::vector<double> samples(numSamples, 0.0);

    // Reuse cached PulseConstants if pw50 hasn't changed
    if (params.pw50 != m_cachedPw50) {
        m_cachedConstants = PulseConstants(params.pw50);
        m_cachedPw50 = params.pw50;
    }
    const PulseConstants& constants = m_cachedConstants;

    // Determine thread count
    const int numThreads = resolveThreadCount(params.numThreads, numSamples);

    if (numThreads == 1) {
        // Single-threaded path — no thread creation overhead
        computeChunk(
            samples.data(), 0, numSamples,
            params.samplingPeriod, params.tailDuration,
            params.pulseType, constants, transitions);
    } else {
        // Multithreaded path — partition samples evenly across threads
        std::vector<std::thread> threads;
        threads.reserve(numThreads);

        const int chunkSize = numSamples / numThreads;
        const int remainder = numSamples % numThreads;

        int currentStart = 0;
        for (int t = 0; t < numThreads; ++t) {
            // Distribute remainder samples across first few threads
            const int thisChunk = chunkSize + (t < remainder ? 1 : 0);
            const int currentEnd = currentStart + thisChunk;

            threads.emplace_back(
                computeChunk,
                samples.data(), currentStart, currentEnd,
                params.samplingPeriod, params.tailDuration,
                params.pulseType, std::cref(constants), std::cref(transitions));

            currentStart = currentEnd;
        }

        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
    }

    // Find min/max for rendering
    double minVal = 0.0;
    double maxVal = 0.0;
    for (int i = 0; i < numSamples; ++i) {
        if (samples[i] < minVal) minVal = samples[i];
        if (samples[i] > maxVal) maxVal = samples[i];
    }

    // Add small margin if flat
    if (maxVal - minVal < 1e-9) {
        maxVal = 1.0;
        minVal = -1.0;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    const double elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    return WaveformResult{
        std::move(samples),
        numSamples,
        duration,
        minVal,
        maxVal,
        elapsedMs,
        numThreads
    };
}
