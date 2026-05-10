# Waveform Simulator — Detailed Code Explanation

This document explains **every file, function, algorithm, and design decision** in the Waveform Simulator project. It follows the data flow from program start to final output.

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [File Structure](#2-file-structure)
3. [Data Flow Summary](#3-data-flow-summary)
4. [PulseShape.h — Pulse Function Definitions](#4-pulseshapeh--pulse-function-definitions)
5. [Transition.h — Pattern Parsing & Transition Detection](#5-transitionh--pattern-parsing--transition-detection)
6. [WaveformEngine.h — Interface Definitions](#6-waveformengineh--interface-definitions)
7. [WaveformEngine.cpp — Core Algorithm & Multithreading](#7-waveformenginecpp--core-algorithm--multithreading)
8. [WaveformRenderer.h/.cpp — Console Graph Output](#8-waveformrendererhcpp--console-graph-output)
9. [main.cpp — Entry Point & CLI Parsing](#9-maincpp--entry-point--cli-parsing)
10. [CMakeLists.txt — Build System](#10-cmakeliststxt--build-system)
11. [Algorithm Deep Dive: How Inter-Symbol Interference Works](#11-algorithm-deep-dive-how-inter-symbol-interference-works)
12. [Multithreading Strategy](#12-multithreading-strategy)
13. [Optimization Techniques](#13-optimization-techniques)
14. [Worked Example: Step by Step](#14-worked-example-step-by-step)

---

## 1. Project Overview

The program simulates **magnetic recording read-back**:

1. A **digital pattern** (e.g., `00111010`) is written on a hard drive disk in **NRZ format** (Non-Return-to-Zero). Each `1` bit = positive magnetization, each `0` bit = negative magnetization.

2. When a magnetic head reads the disk, each **transition** (change from 0→1 or 1→0) produces a **pulse** in the read-back signal. The shape of this pulse is specified as a mathematical function (Lorentz, Gauss, or Sinc).

3. When multiple transitions are close together, their pulses **overlap and add up** — this is called **inter-symbol interference (ISI)**. The program computes this by summing all pulse contributions at each sample point.

4. The result is a **digitized waveform** — an array of amplitude values sampled at a fixed time interval.

### What the program does NOT do

- It does **not** simulate the writing process
- It does **not** model noise, equalization, or channel decoding
- It does **not** require any GUI framework — output is a text-based graph

### Dependencies

**None** beyond the C++17 standard library. No Qt, no SFML, no Boost. Just `<thread>`, `<cmath>`, `<vector>`, `<iostream>`, etc.

---

## 2. File Structure

```
WaveformSimulator/
├── CMakeLists.txt              # Build system (CMake 3.16+, C++17)
├── README.md                   # User-facing documentation
├── EXPLANATION.md              # This file
└── src/
    ├── PulseShape.h            # Pulse type enum + evaluation function
    ├── Transition.h            # NRZ pattern → transition list
    ├── WaveformEngine.h        # Parameter/result structs + generateWaveform() declaration
    ├── WaveformEngine.cpp      # Core algorithm: multithreaded waveform computation
    ├── WaveformRenderer.h      # Render parameter struct + renderWaveform() declaration
    ├── WaveformRenderer.cpp    # Text-based graph output to console and file
    └── main.cpp                # Entry point: CLI parsing, orchestration
```

### Dependency graph (who includes whom):

```
main.cpp
  ├── PulseShape.h       (for pulseTypeFromString, pulseTypeToString)
  ├── WaveformEngine.h   (for WaveformParams, WaveformResult, generateWaveform)
  │     ├── PulseShape.h
  │     └── Transition.h (for cleanPattern)
  └── WaveformRenderer.h (for RenderParams, renderWaveform)
        └── WaveformEngine.h
```

---

## 3. Data Flow Summary

```
User input (CLI args)
        │
        ▼
  ┌─────────────┐
  │  main.cpp   │  Parse arguments → WaveformParams struct
  └──────┬──────┘
         │
         ▼
  ┌────────────────────────────────────────┐
  │ WaveformEngine::instance().generate() │  Singleton
  │                                        │
  │  1. Parse NRZ pattern string
  │  2. Detect transitions (Transition.h)
  │  3. Reuse cached PulseConstants (or recompute if pw50 changed)
  │  4. Allocate output array
  │  5. Resolve thread count (cached hardware_concurrency)
  │  6. Spawn threads, each computes a chunk:
  │     For each sample point:
  │       For each transition within tail range:
  │         sum += evaluatePulse(dt) × sign
  │  7. Join threads
  │  8. Find min/max amplitude
  └──────────────┬─────────────────────────┘
         │ WaveformResult (vector<double> + metadata)
         ▼
  ┌─────────────────┐
  │ renderWaveform  │  WaveformRenderer.cpp
  │                 │
  │  1. Map samples to a character grid
  │  2. Draw zero baseline
  │  3. Draw waveform with '*' characters
  │  4. Add Y-axis labels and X-axis time labels
  │  5. Print to stdout
  │  6. Optionally save to file
  └─────────────────┘
```

---

## 4. PulseShape.h — Pulse Function Definitions

### Purpose
Defines the three pulse shapes required by the task and provides an optimized evaluation function.

### PulseType enum

```cpp
enum class PulseType { Lorentz, Gauss, Sinc };
```

A strongly-typed C++11 enum class. Prevents implicit int conversion.

### PulseConstants struct

```cpp
struct PulseConstants {
    double pw50Sq;      // pw50 * pw50
    double gaussDenom;  // pw50^2 / (4 * ln(2))
    double sincScale;   // 3.791 / pw50
};
```

**Why?** The `evaluatePulse` function is called millions of times (once per sample × per transition). Computing `pw50*pw50` or `log(2)` every call would be wasteful. Instead, we compute these **once** before the loop and pass them by const reference.

### evaluatePulse function

```cpp
inline double evaluatePulse(PulseType type, double t, const PulseConstants& c) noexcept
```

- **`inline`**: Suggests the compiler inline this into the hot loop to eliminate function call overhead.
- **`noexcept`**: Guarantees no exceptions, enabling more aggressive compiler optimizations.
- **`t`**: Time offset in nanoseconds from the transition center. `t=0` means we're at the exact transition point.

#### Lorentz pulse
```
f(t) = pw50² / (4t² + pw50²)
```
- A bell-shaped curve centered at t=0
- At t=0: f(0) = pw50²/pw50² = **1.0** (maximum)
- At t=±pw50/2: f = pw50²/(pw50² + pw50²) = **0.5** (50% amplitude — that's what pw50 means)
- As t→∞: f→0 (tails off to zero)
- **Shape**: Lorentzian (also called Cauchy distribution), has fat tails

#### Gauss pulse
```
f(t) = exp(-t² / (pw50² / (4·ln2)))
```
- Gaussian bell curve centered at t=0
- At t=0: f(0) = exp(0) = **1.0**
- At t=±pw50/2: f = exp(-ln2) = **0.5** (50% point)
- **Shape**: Gaussian, thinner tails than Lorentz

#### Sinc pulse
```
f(t) = sin(3.791·t/pw50) / (3.791·t/pw50),  t ≠ 0
f(0) = 1.0
```
- Sinc function (sin(x)/x) with scaling factor 3.791
- At t=0: mathematical limit = **1.0** (handled explicitly with `if (t == 0.0)`)
- At t=±pw50/2: f ≈ **0.5** (the constant 3.791 is chosen to achieve this)
- **Shape**: Oscillating sinc, has positive and negative sidelobes

### Helper functions

- `pulseTypeFromString("lorentz")` → `PulseType::Lorentz` — for CLI parsing
- `pulseTypeToString(PulseType::Lorentz)` → `"Lorentz"` — for display

---

## 5. Transition.h — Pattern Parsing & Transition Detection

### Purpose
Converts an NRZ bit pattern string into a list of transitions with their timestamps.

### Transition struct

```cpp
struct Transition {
    double timeNs;      // When the transition happens (nanoseconds)
    bool   isPositive;  // true = 0→1 (positive pulse), false = 1→0 (negative pulse)
};
```

### detectTransitions function

**Input**: Pattern string like `"00111010"` and a bit duration (e.g., 5.0 ns)

**Algorithm**:
1. Strip spaces from the pattern
2. Walk through the pattern comparing adjacent bits
3. When `bits[i] != bits[i-1]`, that's a transition at time `i * bitDuration`
4. If `bits[i] == '1'` → positive transition (0→1)
5. If `bits[i] == '0'` → negative transition (1→0)

**Example**: Pattern `00111010`, bitDuration=5ns:

| Index i | bits[i-1] → bits[i] | Transition? | Time (ns) | Type     |
|---------|---------------------|-------------|-----------|----------|
| 1       | 0 → 0              | No          |           |          |
| 2       | 0 → 1              | **Yes**     | 10.0      | Positive |
| 3       | 1 → 1              | No          |           |          |
| 4       | 1 → 1              | No          |           |          |
| 5       | 1 → 0              | **Yes**     | 25.0      | Negative |
| 6       | 0 → 1              | **Yes**     | 30.0      | Positive |
| 7       | 1 → 0              | **Yes**     | 35.0      | Negative |

Result: 4 transitions at t=10, 25, 30, 35 ns.

**Key property**: The returned vector is **sorted by timeNs** (because we iterate left-to-right). This sorted order is exploited by the engine for early-exit optimization.

### cleanPattern function

Simple utility: removes spaces from the input string. Allows the user to pass `"0 0 1 1 1 0 1 0"` for readability.

---

## 6. WaveformEngine.h — Singleton Class & Interface

### Singleton Pattern (Meyer's Singleton)

`WaveformEngine` is implemented as a **singleton class** — only one instance exists in the entire program:

```cpp
class WaveformEngine {
public:
    static WaveformEngine& instance();  // Meyer's singleton

    // Deleted copy and move — enforce single instance
    WaveformEngine(const WaveformEngine&)            = delete;
    WaveformEngine& operator=(const WaveformEngine&) = delete;
    WaveformEngine(WaveformEngine&&)                 = delete;
    WaveformEngine& operator=(WaveformEngine&&)      = delete;

    WaveformResult generate(const WaveformParams& params);

private:
    WaveformEngine() = default;  // Only instance() can construct
    ~WaveformEngine() = default;

    double         m_cachedPw50 = -1.0;          // Cached pw50 value
    PulseConstants m_cachedConstants{1.0};        // Cached precomputed constants
    int            m_hardwareThreads = 0;         // Cached hardware_concurrency
};
```

**Why singleton?**

1. **Single responsibility**: There should be exactly one waveform computation engine in the program
2. **Cached state**: `PulseConstants` are expensive to recompute (`log(2)`, divisions) — the singleton caches them and reuses when `pw50` hasn't changed
3. **Hardware query caching**: `std::thread::hardware_concurrency()` is a system call — queried once and cached in `m_hardwareThreads`
4. **Thread-safe initialization**: Meyer's singleton uses a function-local `static` variable, which C++11 guarantees is initialized exactly once, even across threads
5. **No global state pollution**: Private constructor prevents accidental creation of extra instances

**Usage in main.cpp:**
```cpp
WaveformResult result = WaveformEngine::instance().generate(params);
```

### WaveformParams struct

All inputs the user provides:

| Field           | Type        | Meaning                                       |
|-----------------|-------------|-----------------------------------------------|
| pattern         | string      | NRZ bit pattern ("00111010")                   |
| samplingPeriod  | double      | Time between output samples (ns)               |
| bitDuration     | double      | Duration of each bit (ns)                      |
| pulseType       | PulseType   | Which pulse function to use                    |
| pw50            | double      | Pulse width at 50% amplitude (ns)              |
| tailDuration    | double      | Truncation cutoff for pulse tails (ns)         |
| numThreads      | int         | Thread count (0 = auto-detect)                 |

### WaveformResult struct

Output of computation:

| Field         | Type           | Meaning                              |
|---------------|----------------|--------------------------------------|
| samples       | vector<double> | The computed waveform amplitude array |
| numSamples    | int            | Length of samples array               |
| duration      | double         | Total waveform duration (ns)         |
| minValue      | double         | Minimum amplitude found              |
| maxValue      | double         | Maximum amplitude found              |
| computeTimeMs | double         | Wall-clock computation time (ms)     |
| threadsUsed   | int            | Actual number of threads used        |

### Waveform dimensions formula

From the task specification:
- **Waveform duration** = number of bits × bit duration
- **Number of samples** = waveform duration / sampling period

Example: 14 bits × 5ns/bit = 70ns duration. At 0.1ns sampling = 700 samples.

---

## 7. WaveformEngine.cpp — Core Algorithm & Multithreading

This is the heart of the program. Two functions:

### computeChunk — The Inner Loop

```cpp
void computeChunk(
    double* output, int startSample, int endSample,
    double samplingPeriod, double tailDuration,
    PulseType pulseType, const PulseConstants& constants,
    const std::vector<Transition>& transitions)
```

This function computes a contiguous range of output samples. It is the **hot path** — where the program spends >99% of its time.

**Algorithm (for each sample s in [startSample, endSample)):**

```
t = s × samplingPeriod          // Current time in nanoseconds
sum = 0.0

for each transition tr:
    dt = t - tr.timeNs           // Time offset from this transition's center
    
    if dt < -tailDuration:       // Transition is too far in the future
        break                    // All remaining transitions are even further → stop
    
    if dt > tailDuration:        // Transition is too far in the past
        continue                 // Skip it, but check the next one
    
    pulse = evaluatePulse(type, dt, constants)   // Compute pulse amplitude at dt
    
    if tr.isPositive:
        sum += pulse             // Positive transition → add positive pulse
    else:
        sum -= pulse             // Negative transition → subtract (negative pulse)

output[s] = sum                  // Store the superposition result
```

**This is how ISI works**: Every sample is the **sum of ALL nearby transition pulses**. The `tailDuration` parameter controls "nearby" — pulses beyond this distance are truncated to zero.

**Early-exit optimization**: Because transitions are sorted by time, once we find one that's too far in the future (`dt < -tailDuration`), we `break` — all subsequent transitions are even further away.

### WaveformEngine::generate() — Orchestration (Singleton Method)

**Steps:**

1. **Parse pattern** → clean bit string
2. **Detect transitions** → sorted vector of `Transition` structs
3. **Calculate dimensions** → numSamples from duration / samplingPeriod
4. **Allocate output** → `vector<double>` of zeros
5. **Reuse cached constants** → if `pw50` matches `m_cachedPw50`, skip recomputation; otherwise rebuild `PulseConstants` and cache them
6. **Resolve thread count** via `resolveThreadCount()`:
   - If user specified 0, use cached `m_hardwareThreads` (queried once from `hardware_concurrency()`)
   - Cap at numSamples (don't launch more threads than work)
   - If numSamples < 1000, force single-threaded (thread creation overhead > computation)
7. **Compute**:
   - Single-threaded: call `computeChunk` directly
   - Multi-threaded: partition samples evenly, launch threads, join all
8. **Find min/max** → for rendering scale
9. **Measure time** → `chrono::high_resolution_clock`
10. **Return** `WaveformResult` with `std::move(samples)` (no copy)

---

## 8. WaveformRenderer.h/.cpp — Console Graph Output

### Purpose
Renders the waveform as a text-based plot on the console. No external dependencies.

### Algorithm

1. **Create a character grid** — `vector<string>` of size plotHeight × plotWidth, filled with spaces
2. **Draw zero baseline** — fill the row corresponding to amplitude=0 with `-` characters
3. **For each column** of the grid:
   - Map the column to a range of waveform samples
   - Find the **min and max** amplitude in that range
   - Map min/max to grid rows
   - Fill the vertical span between those rows with `*` characters
   - If the waveform crosses the zero line, use `+` instead
4. **Add Y-axis labels** — amplitude values at key rows (top, bottom, zero, quartiles)
5. **Add X-axis labels** — time values in nanoseconds
6. **Print** the assembled string to stdout
7. **Optionally save** to a text file

### Character mapping

| Character | Meaning |
|-----------|---------|
| `*`       | Waveform amplitude at this position |
| `-`       | Zero baseline (amplitude = 0) |
| `+`       | Waveform crossing the zero baseline |
| `\|`      | Plot border (left and right edges) |
| `+`/`-`   | Plot border corners and top/bottom |
| (space)   | Empty area |

### Downsampling

If the waveform has 10,000 samples but the plot is only 120 columns wide, we can't draw every sample. Instead, for each column we find **all samples** that map to it and draw a vertical line from the **minimum to maximum** value in that range. This correctly represents sharp peaks and valleys without aliasing.

---

## 9. main.cpp — Entry Point & CLI Parsing

### Flow

1. Set default parameters
2. Parse command-line arguments with a simple loop (`-p`, `-s`, `-b`, `-f`, `-w`, `-t`, `-j`, `-o`, `-h`)
3. Validate the pattern (only 0s and 1s, at least 2 bits)
4. Print configuration summary
5. Call `generateWaveform(params)` → get `WaveformResult`
6. Print computation statistics
7. Call `renderWaveform(result, renderParams)` → display graph

### Input validation

Each numeric parameter is checked for positive values. The pulse type string is validated against the three known types. Invalid input prints an error and returns exit code 1.

### CLI option: `-o <file>`

If provided, the graph is saved to a text file in addition to being printed to stdout. Useful for:
- Redirecting output to a file for later viewing
- Embedding in documentation
- Comparing waveforms between different pulse types

---

## 10. CMakeLists.txt — Build System

```cmake
cmake_minimum_required(VERSION 3.16)
project(WaveformSimulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)          # C++17 required (std::thread, structured bindings)
set(CMAKE_CXX_STANDARD_REQUIRED ON) # Fail if compiler doesn't support C++17
set(CMAKE_CXX_EXTENSIONS OFF)       # No GNU extensions — strict ISO C++17
```

### Optimization flags

| Compiler | Flags | Effect |
|----------|-------|--------|
| MSVC | `/O2 /Oi /Ot /GL /fp:fast` | Max speed, intrinsics, whole-program optimization, fast FP |
| GCC/Clang | `-O3 -march=native -ffast-math -flto` | Max speed, CPU-specific instructions, fast FP, link-time optimization |

**`/fp:fast` / `-ffast-math`**: Allows the compiler to reorder floating-point operations for speed. Safe here because we don't need IEEE-754 strict compliance — waveform values are approximate by nature.

### Threading

```cmake
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
```

On Linux this links `-lpthread`. On Windows, threading is built into the runtime so no extra library is needed.

### Cross-platform

```cmake
if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE m)  # libm for math functions
endif()
```

Linux requires explicit linking of the math library (`-lm`). Windows and macOS include it in the standard library automatically.

---

## 11. Algorithm Deep Dive: How Inter-Symbol Interference Works

### The physics

When two transitions are close together, the read-back pulses from each transition overlap in time. The resulting signal is the **sum** of both pulses.

### Example: Pattern `0 0 0 1 0 0 0 0 0`

This pattern has two transitions:
- **Positive** at bit 3 (time = 15ns, 0→1)
- **Negative** at bit 4 (time = 20ns, 1→0)

At any time t, the read-back signal is:

```
signal(t) = +pulse(t - 15) + (-pulse(t - 20))
          = pulse(t - 15) - pulse(t - 20)
```

At t=15ns (center of positive transition):
```
signal(15) = pulse(0) - pulse(-5)
           = 1.0 - pulse(5)        // pulse(5) is small but non-zero
           ≈ 1.0 - 0.083           // For Lorentz with pw50=3
           = 0.917                  // Reduced from 1.0 by ISI!
```

Without ISI (if pulses were independent), the peak would be exactly 1.0. With ISI, it's only 0.917 because the negative pulse's tail is subtracting from the positive peak.

### The code that does this

```cpp
for (int tr = 0; tr < numTransitions; ++tr) {
    const double dt = t - transitions[tr].timeNs;
    if (std::abs(dt) > tailDuration) continue; // Truncation (requirement 5)
    
    const double pulse = evaluatePulse(pulseType, dt, constants);
    sum += transitions[tr].isPositive ? pulse : -pulse;  // Superposition (ISI)
}
```

**Two lines implement both key requirements:**
- **Truncation**: `if (dt > tailDuration) continue` and `if (dt < -tailDuration) break`
- **ISI**: `sum +=` accumulates contributions from ALL nearby transitions

---

## 12. Multithreading Strategy

### Why it works without synchronization

The waveform is an array of independent samples. Sample `s` depends only on:
- The transitions vector (read-only, shared)
- The pulse constants (read-only, shared)
- Its own output slot `output[s]` (write-only, exclusive)

No sample's computation depends on any other sample. This is **embarrassingly parallel** — the ideal case for multithreading.

### Partitioning

```
Thread 0: samples [0 ................. N/4)
Thread 1: samples [N/4 ............... N/2)
Thread 2: samples [N/2 ............. 3N/4)
Thread 3: samples [3N/4 ................ N)
```

Each thread writes to its own non-overlapping section of the output array. No atomics, no mutexes, no locks, no false sharing concerns (chunks are large enough).

### Remainder distribution

If numSamples doesn't divide evenly by numThreads, the first `remainder` threads get one extra sample:

```cpp
const int chunkSize = numSamples / numThreads;
int remainder = numSamples % numThreads;
const int thisChunk = chunkSize + (t < remainder ? 1 : 0);
```

Example: 10,003 samples, 4 threads → chunks of 2501, 2501, 2501, 2500.

### Auto-tuning

```cpp
if (numSamples < 1000) numThreads = 1;
```

For small workloads, the overhead of creating and joining threads (typically 10-50μs per thread) exceeds the computation time. The threshold of 1000 samples is empirical.

### std::thread usage

```cpp
threads.emplace_back(
    computeChunk,                    // Function pointer
    samples.data(),                  // Raw pointer to output array
    currentStart, currentEnd,        // This thread's range
    params.samplingPeriod,           // Value copy (cheap)
    params.tailDuration,             // Value copy (cheap)
    params.pulseType,                // Value copy (enum)
    std::cref(constants),            // Const reference (shared, read-only)
    std::cref(transitions));         // Const reference (shared, read-only)
```

- **`std::cref`**: Passes the `PulseConstants` and `transitions` vector by const reference instead of copying them. Critical for the transitions vector which could be large.
- **`samples.data()`**: Raw pointer because `std::thread` doesn't support reference-to-vector for direct element access across threads.

---

## 13. Optimization Techniques

### 1. Precomputed constants (PulseConstants)

Without precomputation:
```cpp
// Called N × T times (samples × transitions)
return (pw50 * pw50) / (4.0 * t * t + pw50 * pw50);  // 2 multiplications of pw50
```

With precomputation:
```cpp
// pw50Sq computed ONCE
return c.pw50Sq / (4.0 * t * t + c.pw50Sq);  // 0 redundant multiplications
```

Savings: ~2 multiplications per call × millions of calls = significant.

### 2. Early-exit on sorted transitions

Because transitions are sorted by time, when we find one that's too far in the future, we stop:

```cpp
if (dt < -tailDuration) break;  // All subsequent transitions are even further away
```

For a pattern with T transitions and a sample at the end of the waveform, without early-exit we'd check all T transitions. With early-exit, we check only those within `tailDuration` — typically just a few.

### 3. Cache-friendly memory access

- `std::vector<double>` stores samples contiguously in memory
- Each thread iterates sequentially through its chunk → linear memory access → optimal CPU cache utilization
- The transitions vector is also contiguous and accessed sequentially

### 4. Compiler optimization flags

- **`/O2` (MSVC)** / **`-O3` (GCC)**: Maximum speed optimization
- **`/GL` + `/LTCG`** / **`-flto`**: Link-time optimization — the compiler can inline `evaluatePulse` even across translation units
- **`/fp:fast`** / **`-ffast-math`**: Relaxed floating-point allows SIMD vectorization and FMA (fused multiply-add) instructions
- **`-march=native`** (GCC/Clang): Uses the CPU's full instruction set (SSE4, AVX2, etc.)

### 5. Minimal branching in hot loop

The `evaluatePulse` switch has only 3 cases and is `inline` — the compiler typically eliminates the switch entirely since `pulseType` is constant within each call to `computeChunk`.

### 6. std::move for result transfer

```cpp
return WaveformResult{std::move(samples), ...};
```

The waveform vector (potentially millions of doubles) is **moved** not copied — zero-cost transfer of ownership.

---

## 14. Worked Example: Step by Step

### Input

```
Pattern:    0 0 1 1 1 0 1 0
Bit duration:    5 ns
Sampling period: 1 ns
Pulse type:      Lorentz
PW50:            3 ns
Tail duration:   15 ns
```

### Step 1: Detect transitions

| Index | bits[i-1]→bits[i] | Transition | Time    | Type     |
|-------|-------------------|------------|---------|----------|
| 2     | 0 → 1             | Yes        | 10.0 ns | Positive |
| 5     | 1 → 0             | Yes        | 25.0 ns | Negative |
| 6     | 0 → 1             | Yes        | 30.0 ns | Positive |
| 7     | 1 → 0             | Yes        | 35.0 ns | Negative |

4 transitions total.

### Step 2: Calculate dimensions

- Duration = 8 bits × 5 ns = **40 ns**
- Samples = 40 / 1 = **40 samples** (at indices 0..39)

### Step 3: Precompute constants

- `pw50Sq = 3² = 9`
- `gaussDenom = 9 / (4 × 0.6931) = 3.247` (not used, but precomputed)
- `sincScale = 3.791 / 3 = 1.264` (not used, but precomputed)

### Step 4: Compute sample at t=10 ns (transition center)

```
sum = 0.0

Transition 0 (t=10, positive):
  dt = 10 - 10 = 0
  pulse = 9 / (4×0 + 9) = 1.0
  sum += 1.0 → sum = 1.0

Transition 1 (t=25, negative):
  dt = 10 - 25 = -15
  |dt| = 15 ≤ tailDuration(15) → NOT truncated
  pulse = 9 / (4×225 + 9) = 9/909 = 0.0099
  sum -= 0.0099 → sum = 0.9901

Transition 2 (t=30, positive):
  dt = 10 - 30 = -20
  dt < -tailDuration → BREAK (early exit)

Final: output[10] = 0.9901
```

Note: The amplitude at the transition center is 0.9901 instead of 1.0 because the distant negative transition at t=25 slightly reduces it. This is ISI in action.

### Step 5: Compute sample at t=27.5 ns (between two close transitions)

```
sum = 0.0

Transition 0 (t=10, positive):
  dt = 27.5 - 10 = 17.5
  dt > tailDuration(15) → skip (continue)

Transition 1 (t=25, negative):
  dt = 27.5 - 25 = 2.5
  pulse = 9 / (4×6.25 + 9) = 9/34 = 0.2647
  sum -= 0.2647 → sum = -0.2647

Transition 2 (t=30, positive):
  dt = 27.5 - 30 = -2.5
  pulse = 9 / (4×6.25 + 9) = 9/34 = 0.2647
  sum += 0.2647 → sum = 0.0000

Transition 3 (t=35, negative):
  dt = 27.5 - 35 = -7.5
  pulse = 9 / (4×56.25 + 9) = 9/234 = 0.0385
  sum -= 0.0385 → sum = -0.0385
```

At t=27.5ns (midpoint between the close transitions at 25 and 30), the positive and negative pulses **nearly cancel** — sum ≈ -0.04. The small non-zero value is from the distant negative transition at t=35.

### Step 6: Render

The 40 computed amplitude values are mapped to a character grid and printed as a text-based waveform plot with time on the X axis and amplitude on the Y axis.

---

## Summary of Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| **Singleton `WaveformEngine`** | Single instance enforced via Meyer's singleton — caches `PulseConstants` and `hardware_concurrency`, private ctor, deleted copy/move |
| **No GUI framework** | Task says "displays in graphical form" — console plot satisfies this with zero dependencies |
| **C++17 only** | Requirement: C++17 max. Uses `std::thread`, structured bindings, `inline` variables |
| **Header-only pulse/transition** | Small, frequently-inlined code — `inline` keyword enables this |
| **Separate engine and renderer** | Engine is pure computation (testable), renderer is pure I/O (replaceable) |
| **`std::thread` not `std::async`** | Direct control over thread count and chunk partitioning |
| **Sorted transitions + early break** | O(T) → O(nearby_T) per sample — major speedup for long patterns |
| **Precomputed `PulseConstants`** | Avoids redundant math in the innermost loop; cached across calls in singleton |
| **`std::move` for result** | Zero-copy transfer of the potentially large waveform vector |
| **`-ffast-math` / `/fp:fast`** | Safe for waveform simulation — we don't need strict IEEE-754 |
| **Console graph with file output** | Portable, no dependencies, readable in any terminal or text editor |
