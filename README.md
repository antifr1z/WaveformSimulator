# Waveform Simulator

A high-performance C++17 console application that generates and displays digitized read-back waveforms simulating magnetic head signals from hard drive media.

**Zero external dependencies** — only the C++17 standard library is required.

## Overview

In magnetic recording, digital patterns (sequences of 1s and 0s in NRZ format) are written to disk media. When a magnetic head reads the media, each magnetization transition produces a pulse in the read-back signal. This program computes and graphically displays the resulting waveform, including inter-symbol interference (ISI) effects when transitions are close together.

### Magnetic Recording Basics

```
NRZ Pattern:     0 0 1 1 1 0 1 0 0 0 1 1 1

                      _____   _       ____
Magnetization:   ____|     |_| |_____|

                     /\              /\
Read-back:       ___/  \__  /\  ____/  \__
                        \/  \/
```

- **Positive transition** (0→1): produces a positive pulse
- **Negative transition** (1→0): produces a negative pulse
- **Inter-symbol interference**: nearby pulses sum together, distorting the waveform

## Features

- **Three pulse shape functions**: Lorentz, Gauss, and Sinc
- **Inter-symbol interference**: full superposition of all transition pulses
- **Multithreaded computation**: parallel waveform generation using `std::thread`
- **Text-based graphical display**: console plot with axes, labels, and zero baseline
- **File output**: optionally save the graph to a text file
- **Cross-platform**: builds on both Windows and Linux
- **C++17 only**: no external dependencies — pure standard library
- **Optimized**: cache-friendly layout, precomputed constants, early-exit loops, thread-based parallelism

## Requirements

### Compiler
- **C++17** capable compiler
  - Windows: MSVC 2019+ or MinGW-w64 8+
  - Linux: GCC 8+ or Clang 7+

### Dependencies
- **CMake** 3.16+
- No other dependencies

## Building

### Windows

```powershell
cmake -B build
cmake --build build --config Release
```

### Linux

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Build Output

The executable is located at `build/WaveformSimulator` (Linux) or `build/Release/WaveformSimulator.exe` (Windows).

## Usage

```
WaveformSimulator [options]
```

### Options

| Option | Description | Default |
|--------|-------------|---------|
| `-p <pattern>` | NRZ bit pattern (string of 0s and 1s) | `00111010001110` |
| `-s <ns>` | Sampling period in nanoseconds | `0.1` |
| `-b <ns>` | Bit duration in nanoseconds | `5.0` |
| `-f <type>` | Pulse function: `lorentz`, `gauss`, or `sinc` | `lorentz` |
| `-w <ns>` | PW50 — pulse width at 50% amplitude | `3.0` |
| `-t <ns>` | Tail duration (pulse truncation cutoff) | `15.0` |
| `-j <N>` | Number of threads (0 = auto-detect) | `0` |
| `-o <file>` | Save graph to a text file | (none) |
| `-h` | Show help | |

### Examples

```bash
# Default Lorentz pulse
./WaveformSimulator -p "00111010001110" -f lorentz -w 3.0

# Gauss pulse with tight sampling
./WaveformSimulator -p "0001000" -f gauss -s 0.05 -b 5.0 -w 2.5

# Sinc pulse, 8 threads
./WaveformSimulator -p "00111010" -f sinc -w 4.0 -j 8

# Long pattern showing ISI effects
./WaveformSimulator -p "0010100010001110101000101" -f lorentz -w 2.0 -t 20.0

# Save graph to file
./WaveformSimulator -p "00111010" -f lorentz -w 3.0 -o waveform.txt
```

## Architecture

```
WaveformSimulator/
├── CMakeLists.txt               # Build system (CMake 3.16+, C++17)
├── README.md                    # This file
├── EXPLANATION.md               # Detailed code explanation
└── src/
    ├── main.cpp                 # CLI parsing, orchestration
    ├── PulseShape.h             # Pulse function definitions (Lorentz, Gauss, Sinc)
    ├── Transition.h             # Transition detection from NRZ pattern
    ├── WaveformEngine.h         # Engine interface (params/result structs)
    ├── WaveformEngine.cpp       # Multithreaded waveform computation
    ├── WaveformRenderer.h       # Renderer interface
    └── WaveformRenderer.cpp     # Text-based graphical output
```

### Core Algorithm

1. **Parse** the NRZ pattern string
2. **Detect transitions** — find all 0→1 and 1→0 boundaries
3. **Generate waveform** — for each sample point, sum contributions from all transitions within tail range
4. **Render** — display the waveform as a text-based graph on the console

### Multithreading Strategy

The waveform array is partitioned into equal chunks, one per thread. Each thread computes its chunk independently — no synchronization needed during computation because:
- The input data (transitions vector) is read-only and shared
- Each thread writes to its own non-overlapping section of the output array
- No atomics, no mutexes, no false sharing

```
Thread 0: samples [0 ... N/4)        ← independent
Thread 1: samples [N/4 ... N/2)      ← independent
Thread 2: samples [N/2 ... 3N/4)     ← independent
Thread 3: samples [3N/4 ... N)       ← independent
```

### Optimization Techniques

| Technique | Description |
|-----------|-------------|
| **Early-exit loop** | Sorted transitions allow `break` when past tail range |
| **Precomputed constants** | `pw50²`, `4·ln(2)`, `3.791/pw50` computed once |
| **Cache-friendly layout** | Contiguous `std::vector<double>` for waveform data |
| **Thread-local computation** | Zero synchronization overhead |
| **Compiler flags** | `/O2 /GL /fp:fast` (MSVC), `-O3 -march=native -ffast-math -flto` (GCC/Clang) |
| **Auto-tuning** | Skips thread creation for small workloads (<1000 samples) |

### Pulse Functions

**Lorentz:**
```
f(t) = pw50² / (4t² + pw50²)
```

**Gauss:**
```
f(t) = exp(-t² / (pw50² / (4·ln2)))
```

**Sinc:**
```
f(t) = sin(3.791·t/pw50) / (3.791·t/pw50),  t ≠ 0
f(0) = 1
```

Where `pw50` is the pulse width at 50% amplitude in nanoseconds.

## Graphical Output

The console displays a text-based waveform plot:
- **`*` characters** — waveform amplitude
- **`-` characters** — zero baseline
- **`+` characters** — waveform crossing zero
- **Y-axis labels** — amplitude values
- **X-axis labels** — time in nanoseconds

The graph can also be saved to a text file with the `-o` option.

## Performance

Approximate computation times (pattern=100 bits, bitDuration=5ns, sampling=0.05ns, tailDuration=15ns):

| Threads | Samples | Time |
|---------|---------|------|
| 1 (auto) | 700 | ~0.01ms |
| 28 (auto) | 10,000 | ~4.7ms |

Speedup scales near-linearly with thread count for large waveforms.

## Documentation

See [EXPLANATION.md](EXPLANATION.md) for a detailed walkthrough of every file, function, algorithm, and design decision — including worked examples with full arithmetic.

## License

This project is provided as a coding task demonstration.
