#pragma once

#include "WaveformEngine.h"

#include <string>

struct RenderParams {
    double samplingPeriod;
    double bitDuration;
    int    plotWidth;    // Console columns for the plot area (0 = auto)
    int    plotHeight;   // Console rows for the plot area
    std::string outputFile; // If non-empty, also write the graph to this file
};

// Display the waveform as a text-based graph on stdout.
// If renderParams.outputFile is non-empty, also saves the graph to that file.
void renderWaveform(const WaveformResult& result, const RenderParams& renderParams);
