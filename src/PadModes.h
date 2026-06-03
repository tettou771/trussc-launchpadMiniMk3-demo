#pragma once

// =============================================================================
// PadModes - the three interactive modes
// =============================================================================
//   PaintMode      : tap pads to toggle colours (event-driven, no clock)
//   SequencerMode  : 8 rows x 8 steps; a playhead sweeps and triggers ChipKit
//   RippleMode     : taps spawn expanding rings of light + a blip
//
// Each mode is a plain class - no inheritance, no virtuals. tcApp owns one of
// each and picks the active one with a Mode enum + switch.
//
// Timing split: a mode separates "advance one tick of state" (step()/tick(),
// driven by a precise async timer in tcApp) from "draw current state into the
// grid" (render()). Paint has no clock - it only reacts to pad presses. This
// keeps the musical timing (sound + LEDs) off the render frame rate.
//
// === HOW TO ADD A MODE ===
//   1. Copy a class below and rename it.
//   2. Add a value to `enum class Mode` in tcApp.h.
//   3. Add it to tcApp's enter / pad / tick / render switches and pick its
//      clock interval (0 = event-driven, no async timer).
// =============================================================================

#include "PadGrid.h"
#include "ChipKit.h"

#include <array>
#include <cmath>
#include <vector>

// -----------------------------------------------------------------------------
// 1. Paint / mirror   (event-driven, no clock)
// -----------------------------------------------------------------------------
class PaintMode {
public:
    void enter() {
        for (auto& row : on_) row.fill(false);
    }

    void onPad(int col, int row, bool pressed, ChipKit& kit) {
        if (!pressed) return;
        on_[row][col] = !on_[row][col];
        if (on_[row][col]) kit.play(row);
    }

    void render(PadGrid& grid) const {
        static const int colColor[8] = {
            lp::color::Red,   lp::color::Orange, lp::color::Yellow, lp::color::Lime,
            lp::color::Green, lp::color::Cyan,   lp::color::Blue,   lp::color::Purple
        };
        for (int r = 0; r < 8; ++r)
            for (int c = 0; c < 8; ++c)
                grid.set(c, r, on_[r][c] ? colColor[c] : lp::color::Off);
    }

private:
    std::array<std::array<bool, 8>, 8> on_{};
};

// -----------------------------------------------------------------------------
// 2. Step sequencer   (clock: one step() per async tick)
// -----------------------------------------------------------------------------
class SequencerMode {
public:
    void enter() {
        for (auto& row : step_) row.fill(false);
        playCol_ = 7;   // first step() wraps to column 0
    }

    void onPad(int col, int row, bool pressed, ChipKit&) {
        if (pressed) step_[row][col] = !step_[row][col];
    }

    // Advance the playhead one column and trigger the lit cells there.
    void step(ChipKit& kit) {
        playCol_ = (playCol_ + 1) % 8;
        for (int r = 0; r < 8; ++r)
            if (step_[r][playCol_]) kit.play(r);
    }

    void render(PadGrid& grid) const {
        for (int r = 0; r < 8; ++r) {
            for (int c = 0; c < 8; ++c) {
                bool lit  = step_[r][c];
                bool head = (c == playCol_);
                int color = lp::color::Off;
                if (lit && head)      color = lp::color::Green;    // hitting now
                else if (lit)         color = lp::color::Blue;     // programmed
                else if (head)        color = lp::color::DimGreen; // playhead trail
                grid.set(c, r, color);
            }
        }
    }

private:
    std::array<std::array<bool, 8>, 8> step_{};
    int playCol_ = 7;
};

// -----------------------------------------------------------------------------
// 3. Ripples   (clock: one tick() per async frame expands every ring a little)
// -----------------------------------------------------------------------------
class RippleMode {
public:
    void enter() { ripples_.clear(); }

    void onPad(int col, int row, bool pressed, ChipKit& kit) {
        if (!pressed) return;
        ripples_.push_back({col, row, 0.0f});
        kit.play(row);
    }

    // Grow each ring by one step; drop the ones that expanded off the grid.
    void tick() {
        for (auto& rp : ripples_) rp.radius += kStep;
        std::vector<Ripple> alive;
        for (const auto& rp : ripples_)
            if (rp.radius < kMaxR) alive.push_back(rp);
        ripples_.swap(alive);
    }

    void render(PadGrid& grid) const {
        grid.clear();
        static const int ring[] = {
            lp::color::White, lp::color::Cyan, lp::color::Blue,
            lp::color::Purple, lp::color::DimBlue
        };
        constexpr int ringCount = sizeof(ring) / sizeof(ring[0]);
        for (const auto& rp : ripples_) {
            for (int r = 0; r < 8; ++r) {
                for (int c = 0; c < 8; ++c) {
                    double d = std::hypot(double(c - rp.col), double(r - rp.row));
                    if (std::abs(d - rp.radius) < 0.8) {
                        int idx = std::min(int(rp.radius), ringCount - 1);
                        grid.set(c, r, ring[idx]);
                    }
                }
            }
        }
    }

private:
    static constexpr float  kStep = 0.21f;  // cells per tick (~7 cells/s at 30 ms)
    static constexpr double kMaxR = 11.0;   // lifetime in cell-radii

    struct Ripple { int col, row; float radius; };  // plain data
    std::vector<Ripple> ripples_;
};
