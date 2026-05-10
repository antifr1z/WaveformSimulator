#include "GuiRenderer.h"

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr int    kDefaultWindowWidth  = 1024;
constexpr int    kDefaultWindowHeight = 600;
constexpr float  kMarginLeft   = 80.0f;
constexpr float  kMarginRight  = 30.0f;
constexpr float  kMarginTop    = 50.0f;
constexpr float  kMarginBottom = 50.0f;
constexpr int    kGridLinesX   = 8;
constexpr int    kGridLinesY   = 6;
constexpr int    kFontSize     = 12;
constexpr int    kTitleFontSize = 16;

[[nodiscard]] std::string fmtDouble(double val, int prec) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(prec) << val;
    return oss.str();
}

// Try to load a system font. Returns true on success.
bool tryLoadFont(sf::Font& font) {
    // Common Windows font paths
    const char* paths[] = {
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
    };
    for (const auto* path : paths) {
        if (font.loadFromFile(path)) return true;
    }
    return false;
}

} // anonymous namespace

void renderWaveformGui(const WaveformResult& result, const GuiRenderParams& params)
{
    const int winW = (params.windowWidth  > 0) ? params.windowWidth  : kDefaultWindowWidth;
    const int winH = (params.windowHeight > 0) ? params.windowHeight : kDefaultWindowHeight;

    // Build title string
    const std::string title = "Waveform Simulator — " + params.pulseTypeName +
        " (pw50=" + fmtDouble(params.pw50, 1) + "ns)";

    sf::RenderWindow window(
        sf::VideoMode(winW, winH), title,
        sf::Style::Titlebar | sf::Style::Close | sf::Style::Resize);
    window.setFramerateLimit(60);

    // Try to load a font for labels
    sf::Font font;
    const bool hasFont = tryLoadFont(font);

    // Plot area
    const float plotLeft   = kMarginLeft;
    const float plotTop    = kMarginTop;
    const float plotRight  = static_cast<float>(winW) - kMarginRight;
    const float plotBottom = static_cast<float>(winH) - kMarginBottom;
    const float plotW      = plotRight - plotLeft;
    const float plotH      = plotBottom - plotTop;

    // Amplitude range with 5% padding
    const double ampRange = result.maxValue - result.minValue;
    const double ampPad   = ampRange * 0.05;
    const double yMin     = result.minValue - ampPad;
    const double yMax     = result.maxValue + ampPad;

    // Build the waveform vertex array (line strip)
    sf::VertexArray waveform(sf::LineStrip, result.numSamples);
    for (int i = 0; i < result.numSamples; ++i) {
        const float xFrac = static_cast<float>(i) / (result.numSamples - 1);
        const float yFrac = static_cast<float>((result.samples[i] - yMin) / (yMax - yMin));

        const float px = plotLeft + xFrac * plotW;
        const float py = plotBottom - yFrac * plotH;

        waveform[i].position = sf::Vector2f(px, py);
        waveform[i].color = sf::Color(50, 200, 50); // Green waveform
    }

    // Event loop
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
            if (event.type == sf::Event::KeyPressed &&
                event.key.code == sf::Keyboard::Escape) {
                window.close();
            }
        }

        window.clear(sf::Color(20, 20, 30)); // Dark background

        // --- Draw grid lines ---
        sf::VertexArray gridLines(sf::Lines);

        // Vertical grid lines (time)
        for (int i = 0; i <= kGridLinesX; ++i) {
            const float x = plotLeft + plotW * i / kGridLinesX;
            gridLines.append(sf::Vertex(sf::Vector2f(x, plotTop),    sf::Color(50, 50, 60)));
            gridLines.append(sf::Vertex(sf::Vector2f(x, plotBottom), sf::Color(50, 50, 60)));
        }

        // Horizontal grid lines (amplitude)
        for (int i = 0; i <= kGridLinesY; ++i) {
            const float y = plotTop + plotH * i / kGridLinesY;
            gridLines.append(sf::Vertex(sf::Vector2f(plotLeft,  y), sf::Color(50, 50, 60)));
            gridLines.append(sf::Vertex(sf::Vector2f(plotRight, y), sf::Color(50, 50, 60)));
        }

        window.draw(gridLines);

        // --- Draw zero baseline ---
        const float zeroY = plotBottom -
            static_cast<float>((0.0 - yMin) / (yMax - yMin)) * plotH;
        if (zeroY >= plotTop && zeroY <= plotBottom) {
            sf::VertexArray zeroLine(sf::Lines, 2);
            zeroLine[0] = sf::Vertex(sf::Vector2f(plotLeft,  zeroY), sf::Color(100, 100, 120));
            zeroLine[1] = sf::Vertex(sf::Vector2f(plotRight, zeroY), sf::Color(100, 100, 120));
            window.draw(zeroLine);
        }

        // --- Draw plot border ---
        sf::RectangleShape border(sf::Vector2f(plotW, plotH));
        border.setPosition(plotLeft, plotTop);
        border.setFillColor(sf::Color::Transparent);
        border.setOutlineColor(sf::Color(100, 100, 120));
        border.setOutlineThickness(1.0f);
        window.draw(border);

        // --- Draw waveform ---
        window.draw(waveform);

        // --- Draw labels (only if font loaded) ---
        if (hasFont) {
            // Title
            sf::Text titleText(title, font, kTitleFontSize);
            titleText.setFillColor(sf::Color(200, 200, 220));
            titleText.setPosition(plotLeft, 10.0f);
            window.draw(titleText);

            // Y-axis labels
            for (int i = 0; i <= kGridLinesY; ++i) {
                const double val = yMax - (yMax - yMin) * i / kGridLinesY;
                const float y = plotTop + plotH * i / kGridLinesY;

                sf::Text label(fmtDouble(val, 2), font, kFontSize);
                label.setFillColor(sf::Color(160, 160, 180));
                label.setPosition(5.0f, y - 8.0f);
                window.draw(label);
            }

            // X-axis labels
            for (int i = 0; i <= kGridLinesX; ++i) {
                const double timeNs = result.duration * i / kGridLinesX;
                const float x = plotLeft + plotW * i / kGridLinesX;

                sf::Text label(fmtDouble(timeNs, 1), font, kFontSize);
                label.setFillColor(sf::Color(160, 160, 180));
                label.setPosition(x - 12.0f, plotBottom + 8.0f);
                window.draw(label);
            }

            // X-axis title
            sf::Text xTitle("Time (ns)", font, kFontSize);
            xTitle.setFillColor(sf::Color(160, 160, 180));
            xTitle.setPosition(plotLeft + plotW / 2.0f - 30.0f, plotBottom + 28.0f);
            window.draw(xTitle);

            // Info text
            const std::string info =
                "Samples: " + std::to_string(result.numSamples) +
                "  |  Duration: " + fmtDouble(result.duration, 1) + " ns" +
                "  |  Threads: " + std::to_string(result.threadsUsed) +
                "  |  Time: " + fmtDouble(result.computeTimeMs, 3) + " ms";
            sf::Text infoText(info, font, kFontSize - 1);
            infoText.setFillColor(sf::Color(120, 120, 140));
            infoText.setPosition(plotLeft, static_cast<float>(winH) - 15.0f);
            window.draw(infoText);
        }

        window.display();
    }
}
