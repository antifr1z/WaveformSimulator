#pragma once

#include "WaveformEngine.h"

#include <string>

struct GuiRenderParams {
    double      samplingPeriod;
    double      bitDuration;
    std::string pulseTypeName;
    double      pw50;
    int         windowWidth;   // 0 = default (1024)
    int         windowHeight;  // 0 = default (600)
};

// Display the waveform in an SFML GUI window.
// Blocks until the user closes the window.
void renderWaveformGui(const WaveformResult& result, const GuiRenderParams& params);
