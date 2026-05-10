#include "WaveformRenderer.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

// Default plot dimensions
constexpr int kDefaultWidth  = 120;
constexpr int kDefaultHeight = 30;
constexpr int kYLabelWidth   = 8;  // Characters reserved for Y-axis labels
constexpr int kXLabelCount   = 8;  // Number of time labels on X axis

constexpr double kAmplitudePadding = 0.05;

// Map a waveform amplitude value to a row index (0 = top, height-1 = bottom).
[[nodiscard]] int mapToRow(double value, double minVal, double maxVal, int height) {
    if (maxVal - minVal < 1e-12) return height / 2;
    const double normalized = (value - minVal) / (maxVal - minVal);
    int row = height - 1 - static_cast<int>(normalized * (height - 1) + 0.5);
    if (row < 0) row = 0;
    if (row >= height) row = height - 1;
    return row;
}

[[nodiscard]] std::string formatValue(double val, int width) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << val;
    std::string s = oss.str();
    // Right-align within width
    while (static_cast<int>(s.size()) < width) s = " " + s;
    return s;
}

[[nodiscard]] std::string formatTime(double ns) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << ns;
    return oss.str();
}

} // anonymous namespace

void renderWaveform(const WaveformResult& result, const RenderParams& renderParams)
{
    const int plotWidth  = (renderParams.plotWidth  > 0) ? renderParams.plotWidth  : kDefaultWidth;
    const int plotHeight = (renderParams.plotHeight > 0) ? renderParams.plotHeight : kDefaultHeight;

    // Amplitude range with padding
    const double ampRange = result.maxValue - result.minValue;
    const double ampPad   = ampRange * kAmplitudePadding;
    const double yMin     = result.minValue - ampPad;
    const double yMax     = result.maxValue + ampPad;

    // Build a 2D character grid (height x width), initialized to spaces
    std::vector<std::string> grid(plotHeight, std::string(plotWidth, ' '));

    // Find the zero-line row
    const int zeroRow = mapToRow(0.0, yMin, yMax, plotHeight);

    // Draw zero baseline with dashes
    if (yMin < 0.0 && yMax > 0.0) {
        for (int col = 0; col < plotWidth; ++col) {
            grid[zeroRow][col] = '-';
        }
    }

    // Downsample waveform to fit plotWidth columns.
    // For each column, pick the sample that maps closest to that column,
    // but also track min/max within the column's sample range for vertical lines.
    for (int col = 0; col < plotWidth; ++col) {
        // Range of samples that map to this column
        const int sampleStart = static_cast<int>(
            static_cast<double>(col) / plotWidth * result.numSamples);
        const int sampleEnd = static_cast<int>(
            static_cast<double>(col + 1) / plotWidth * result.numSamples);

        if (sampleStart >= result.numSamples) continue;

        // Find min and max value in this column's sample range
        double colMin = result.samples[sampleStart];
        double colMax = result.samples[sampleStart];
        for (int s = sampleStart + 1; s < sampleEnd && s < result.numSamples; ++s) {
            if (result.samples[s] < colMin) colMin = result.samples[s];
            if (result.samples[s] > colMax) colMax = result.samples[s];
        }

        const int rowMin = mapToRow(colMax, yMin, yMax, plotHeight); // max value = top row (lower index)
        const int rowMax = mapToRow(colMin, yMin, yMax, plotHeight); // min value = bottom row (higher index)

        // Draw vertical span for this column
        for (int row = rowMin; row <= rowMax; ++row) {
            if (row == zeroRow && grid[row][col] == '-') {
                grid[row][col] = '+'; // Waveform crossing zero line
            } else {
                grid[row][col] = '*';
            }
        }
    }

    // Build the output string
    std::ostringstream out;

    // Top border
    out << std::string(kYLabelWidth, ' ') << "+"
        << std::string(plotWidth, '-') << "+\n";

    // Plot rows with Y-axis labels
    for (int row = 0; row < plotHeight; ++row) {
        // Y-axis label: show value at a few key rows
        const bool showLabel = (row == 0) ||
                               (row == plotHeight - 1) ||
                               (row == zeroRow) ||
                               (row == plotHeight / 4) ||
                               (row == plotHeight * 3 / 4);

        if (showLabel) {
            const double val = yMax - (yMax - yMin) * row / (plotHeight - 1);
            out << formatValue(val, kYLabelWidth - 1) << " |";
        } else {
            out << std::string(kYLabelWidth - 1, ' ') << " |";
        }

        out << grid[row] << "|\n";
    }

    // Bottom border
    out << std::string(kYLabelWidth, ' ') << "+"
        << std::string(plotWidth, '-') << "+\n";

    // X-axis time labels
    out << std::string(kYLabelWidth + 1, ' ');
    for (int i = 0; i <= kXLabelCount; ++i) {
        const double timeNs = result.duration * i / kXLabelCount;
        const int col = plotWidth * i / kXLabelCount;
        const std::string label = formatTime(timeNs);

        // Position the label centered on the column
        const int labelPos = col - static_cast<int>(label.size()) / 2;
        const int currentPos = static_cast<int>(out.str().size())
            - static_cast<int>(out.str().rfind('\n')) - 1;
        while (currentPos < kYLabelWidth + 1 + labelPos) {
            out << ' ';
            // Recalculate (simple padding approach)
            break;
        }

        // Simple approach: just space-separate the labels
        if (i > 0) out << "  ";
        out << label;
    }
    out << "\n";

    // X-axis label
    const int centerPad = kYLabelWidth + 1 + plotWidth / 2 - 5;
    out << std::string(std::max(0, centerPad), ' ') << "Time (ns)\n";

    const std::string graphStr = out.str();

    // Print to stdout
    std::cout << graphStr;

    // Optionally save to file
    if (!renderParams.outputFile.empty()) {
        std::ofstream file(renderParams.outputFile);
        if (file.is_open()) {
            file << graphStr;
            std::cout << "\nGraph saved to: " << renderParams.outputFile << "\n";
        } else {
            std::cerr << "Warning: could not write to " << renderParams.outputFile << "\n";
        }
    }
}
