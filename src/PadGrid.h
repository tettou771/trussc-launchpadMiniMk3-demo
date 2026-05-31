#pragma once

// =============================================================================
// PadGrid - the 8x8 colour buffer shared between modes, device and screen
// =============================================================================
// Modes write palette indices into a PadGrid; the app pushes it to the
// Launchpad (diffed) and mirrors it on screen via screenColor().
// =============================================================================

#include "LaunchpadMk3.h"
#include <TrussC.h>

#include <array>

using namespace tc;

class PadGrid {
public:
    std::array<std::array<int, 8>, 8> color{};  // color[row][col], 0 = off

    void clear() {
        for (auto& row : color) row.fill(lp::color::Off);
    }
    void set(int col, int row, int c) {
        if (LaunchpadMk3::inRange(col, row)) color[row][col] = c;
    }
    int get(int col, int row) const {
        return LaunchpadMk3::inRange(col, row) ? color[row][col] : lp::color::Off;
    }
};

// Approximate on-screen RGB for the palette indices this example uses.
inline Color screenColor(int idx) {
    switch (idx) {
        case lp::color::Off:      return Color(0.10f, 0.10f, 0.12f);
        case lp::color::White:    return Color(1.0f, 1.0f, 1.0f);
        case lp::color::Red:      return Color(1.0f, 0.15f, 0.15f);
        case lp::color::Orange:   return Color(1.0f, 0.55f, 0.10f);
        case lp::color::Yellow:   return Color(1.0f, 0.95f, 0.20f);
        case lp::color::Lime:     return Color(0.65f, 1.0f, 0.20f);
        case lp::color::Green:    return Color(0.20f, 1.0f, 0.25f);
        case lp::color::Mint:     return Color(0.30f, 1.0f, 0.65f);
        case lp::color::Cyan:     return Color(0.20f, 0.85f, 1.0f);
        case lp::color::Blue:     return Color(0.25f, 0.40f, 1.0f);
        case lp::color::Purple:   return Color(0.65f, 0.30f, 1.0f);
        case lp::color::Pink:     return Color(1.0f, 0.35f, 0.75f);
        case lp::color::DimBlue:  return Color(0.12f, 0.18f, 0.45f);
        case lp::color::DimGreen: return Color(0.10f, 0.35f, 0.15f);
        default:                  return Color(0.5f, 0.5f, 0.5f);
    }
}
