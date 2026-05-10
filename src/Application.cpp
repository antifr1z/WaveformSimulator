#include "Application.h"

#include "PulseShape.h"

#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>

void Application::printUsage(const char* programName) const
{
    std::cout
        << "Usage: " << programName << " [options]\n"
        << "\n"
        << "Waveform Simulator — generates magnetic head read-back waveforms.\n"
        << "\n"
        << "Options:\n"
        << "  -p <pattern>   NRZ bit pattern (0s and 1s)       [00111010001110]\n"
        << "  -s <ns>        Sampling period (nanoseconds)     [0.1]\n"
        << "  -b <ns>        Bit duration (nanoseconds)        [5.0]\n"
        << "  -f <type>      Pulse function: lorentz|gauss|sinc [lorentz]\n"
        << "  -w <ns>        PW50 pulse width at 50% (ns)     [3.0]\n"
        << "  -t <ns>        Tail duration / truncation (ns)   [15.0]\n"
        << "  -j <N>         Number of threads (0=auto)        [0]\n"
        << "  -g             Show graphical SFML window          [off]\n"
        << "  -o <file>      Save graph to text file           [none]\n"
        << "  -h             Show this help\n"
        << "\n"
        << "Examples:\n"
        << "  " << programName << " -p \"00111010\" -f lorentz -w 3.0\n"
        << "  " << programName << " -p \"0001000\" -f gauss -s 0.05 -b 5.0 -w 2.5\n"
        << "  " << programName << " -p \"00111010\" -f sinc -w 4.0 -j 8\n"
        << "  " << programName << " -p \"00111010\" -f lorentz -w 3.0 -o waveform.txt\n";
}

void Application::printConfig(const std::string& cleanBits) const
{
    // Output format matches task specification inputs (items 1-5):
    //  1. Digital pattern in NRZ format
    //  2. Waveform sampling period (ns)
    //  3. Pattern bit duration (ns)
    //  4. Read-back pulse shape function
    //  5. Pulse tail duration (ns)
    const double duration = static_cast<double>(cleanBits.size()) * m_params.bitDuration;
    const int numSamples = static_cast<int>(std::ceil(duration / m_params.samplingPeriod));

    std::cout << "=== Waveform Simulator ===\n"
              << "\n"
              << "Inputs:\n"
              << "  1. Pattern (NRZ):          " << cleanBits << " (" << cleanBits.size() << " bits)\n"
              << "  2. Sampling period:        " << m_params.samplingPeriod << " ns\n"
              << "  3. Bit duration:           " << m_params.bitDuration << " ns\n"
              << "  4. Pulse shape:            " << pulseTypeToString(m_params.pulseType)
              << "  (pw50 = " << m_params.pw50 << " ns)\n"
              << "  5. Tail duration:          " << m_params.tailDuration << " ns\n"
              << "\n"
              << "Derived:\n"
              << "  Waveform duration:         " << duration << " ns\n"
              << "  Number of samples:         " << numSamples << "\n"
              << "  Threads:                   " << (m_params.numThreads == 0 ? "auto" : std::to_string(m_params.numThreads)) << "\n"
              << "\n";
}

void Application::printResult(const WaveformResult& result) const
{
    std::cout << " done.\n"
              << "\n"
              << "Results:\n"
              << "  Samples computed:  " << result.numSamples << "\n"
              << "  Amplitude range:   [" << std::fixed << std::setprecision(6)
              << result.minValue << ", " << result.maxValue << "]\n"
              << std::defaultfloat
              << "  Threads used:      " << result.threadsUsed << "\n"
              << "  Compute time:      " << std::fixed << std::setprecision(4)
              << result.computeTimeMs << " ms\n"
              << std::defaultfloat
              << "\n";
}

bool Application::validatePattern() const
{
    const std::string cleanBits = cleanPattern(m_params.pattern);
    if (cleanBits.size() < 2) {
        std::cerr << "Error: pattern must contain at least 2 bits.\n";
        return false;
    }
    for (char c : cleanBits) {
        if (c != '0' && c != '1') {
            std::cerr << "Error: pattern must contain only 0s and 1s (got '" << c << "').\n";
            return false;
        }
    }
    return true;
}

std::optional<int> Application::parseArgs(int argc, char* argv[])
{
    // Set defaults
    m_params.pattern        = "00111010001110";
    m_params.samplingPeriod = 0.1;
    m_params.bitDuration    = 5.0;
    m_params.pulseType      = PulseType::Lorentz;
    m_params.pw50           = 3.0;
    m_params.tailDuration   = 15.0;
    m_params.numThreads     = 0;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];

        if (std::strcmp(arg, "-h") == 0 || std::strcmp(arg, "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        }

        if (std::strcmp(arg, "-g") == 0) {
            m_guiMode = true;
            continue;
        }

        if (i + 1 >= argc) {
            std::cerr << "Error: option " << arg << " requires a value.\n";
            return 1;
        }

        const char* val = argv[++i];

        if (std::strcmp(arg, "-p") == 0) {
            m_params.pattern = val;
        } else if (std::strcmp(arg, "-s") == 0) {
            m_params.samplingPeriod = std::atof(val);
            if (m_params.samplingPeriod <= 0.0) {
                std::cerr << "Error: sampling period must be > 0.\n";
                return 1;
            }
        } else if (std::strcmp(arg, "-b") == 0) {
            m_params.bitDuration = std::atof(val);
            if (m_params.bitDuration <= 0.0) {
                std::cerr << "Error: bit duration must be > 0.\n";
                return 1;
            }
        } else if (std::strcmp(arg, "-f") == 0) {
            const auto pulseType = pulseTypeFromString(val);
            if (!pulseType) {
                std::cerr << "Error: unknown pulse type: " << val << "\n"
                          << "Valid pulse types: lorentz, gauss, sinc\n";
                return 1;
            }
            m_params.pulseType = *pulseType;
        } else if (std::strcmp(arg, "-w") == 0) {
            m_params.pw50 = std::atof(val);
            if (m_params.pw50 <= 0.0) {
                std::cerr << "Error: PW50 must be > 0.\n";
                return 1;
            }
        } else if (std::strcmp(arg, "-t") == 0) {
            m_params.tailDuration = std::atof(val);
            if (m_params.tailDuration <= 0.0) {
                std::cerr << "Error: tail duration must be > 0.\n";
                return 1;
            }
        } else if (std::strcmp(arg, "-j") == 0) {
            m_params.numThreads = std::atoi(val);
            if (m_params.numThreads < 0) {
                std::cerr << "Error: thread count must be >= 0.\n";
                return 1;
            }
        } else if (std::strcmp(arg, "-o") == 0) {
            m_outputFile = val;
        } else {
            std::cerr << "Error: unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    return std::nullopt; // Success — proceed to run()
}

int Application::run()
{
    if (!validatePattern()) {
        return 1;
    }

    const std::string cleanBits = cleanPattern(m_params.pattern);
    printConfig(cleanBits);

    // Compute waveform via singleton engine
    std::cout << "Computing waveform..." << std::flush;
    WaveformResult result = WaveformEngine::instance().generate(m_params);
    printResult(result);

    // Display waveform as text-based graph
    RenderParams renderParams;
    renderParams.samplingPeriod = m_params.samplingPeriod;
    renderParams.bitDuration    = m_params.bitDuration;
    renderParams.plotWidth      = 0;
    renderParams.plotHeight     = 0;
    renderParams.outputFile     = std::move(m_outputFile);

    renderWaveform(result, renderParams);

    // Launch SFML GUI window if -g was specified
    if (m_guiMode) {
        GuiRenderParams guiParams;
        guiParams.samplingPeriod = m_params.samplingPeriod;
        guiParams.bitDuration    = m_params.bitDuration;
        guiParams.pulseTypeName  = pulseTypeToString(m_params.pulseType);
        guiParams.pw50           = m_params.pw50;
        guiParams.windowWidth    = 0;
        guiParams.windowHeight   = 0;

        std::cout << "Opening GUI window... (close window or press Esc to exit)\n";
        renderWaveformGui(result, guiParams);
    }

    return 0;
}
