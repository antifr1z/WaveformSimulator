#pragma once

#include "GuiRenderer.h"
#include "WaveformEngine.h"
#include "WaveformRenderer.h"

#include <optional>
#include <string>

// Application class — encapsulates CLI parsing, validation, and orchestration.
// Keeps main.cpp minimal and clean.
class Application {
public:
    // Parse command-line arguments.
    // Returns exit code on completion (0 = help shown), or std::nullopt to proceed.
    [[nodiscard]] std::optional<int> parseArgs(int argc, char* argv[]);

    // Run the waveform simulation pipeline: compute + render.
    [[nodiscard]] int run();

private:
    void printUsage(const char* programName) const;
    void printConfig(const std::string& cleanBits) const;
    void printResult(const WaveformResult& result) const;

    [[nodiscard]] bool validatePattern() const;

    WaveformParams m_params{};
    std::string    m_outputFile;
    bool           m_guiMode = false;
};
