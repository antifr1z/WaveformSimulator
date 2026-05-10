#pragma once

#include <string>
#include <vector>
#include <stdexcept>

struct Transition {
    double timeNs;      // Time of the transition in nanoseconds
    bool   isPositive;  // true = 0->1 (positive pulse), false = 1->0 (negative pulse)
};

// Parse an NRZ pattern string and detect all transitions.
// Pattern must contain only '0' and '1' characters (spaces are stripped).
// Returns a sorted vector of transitions with their timestamps.
[[nodiscard]] inline std::vector<Transition> detectTransitions(const std::string& pattern, double bitDuration) {
    // Strip spaces and validate
    std::string bits;
    bits.reserve(pattern.size());
    for (char c : pattern) {
        if (c == ' ') continue;
        if (c != '0' && c != '1') {
            throw std::invalid_argument(std::string("Invalid character in pattern: '") + c + "'");
        }
        bits.push_back(c);
    }

    if (bits.size() < 2) {
        throw std::invalid_argument("Pattern must contain at least 2 bits");
    }

    std::vector<Transition> transitions;
    transitions.reserve(bits.size()); // Upper bound

    for (size_t i = 1; i < bits.size(); ++i) {
        if (bits[i] != bits[i - 1]) {
            transitions.push_back({
                static_cast<double>(i) * bitDuration,
                bits[i] == '1' // 0->1 = positive
            });
        }
    }

    return transitions;
}

// Strip spaces from the pattern and return the clean bit string.
[[nodiscard]] inline std::string cleanPattern(const std::string& pattern) {
    std::string bits;
    bits.reserve(pattern.size());
    for (char c : pattern) {
        if (c != ' ') bits.push_back(c);
    }
    return bits;
}
