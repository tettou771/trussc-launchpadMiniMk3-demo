#pragma once

// =============================================================================
// PadModes - the three interactive modes
// =============================================================================
//   PaintMode      : tap pads to toggle colours (a simple paint / mirror)
//   SequencerMode  : 8 rows x 8 steps; a playhead sweeps and triggers ChipKit
//   RippleMode     : taps spawn expanding rings of light + a blip
//
// Each mode is a plain class - no inheritance, no virtuals. A mode only:
//   - reacts to a pad press in onPad()
//   - draws itself by writing palette indices into the PadGrid in update()
//   - makes sound through the ChipKit it is handed
// tcApp owns one of each and picks the active one with a Mode enum + switch.
//
// === HOW TO ADD A MODE ===
//   1. Copy one of the classes below and rename it.
//   2. Add a value to `enum class Mode` in tcApp.h.
//   3. Add a member + a `case` in tcApp's modeEnter / modePad / modeUpdate.
// =============================================================================

#include "PadGrid.h"
#include "ChipKit.h"

#include <cmath>
#include <vector>

// -----------------------------------------------------------------------------
// 1. Paint / mirror
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

    void update(double, PadGrid& grid, ChipKit&) {
        // Column-based rainbow so the painting is colourful.
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
// 2. Step sequencer
// -----------------------------------------------------------------------------
class SequencerMode {
public:
    void enter() {
        for (auto& row : step_) row.fill(false);
        playCol_ = 0;
        nextStepT_ = -1.0;  // (re)initialised on first update
    }

    void onPad(int col, int row, bool pressed, ChipKit&) {
        if (pressed) step_[row][col] = !step_[row][col];
    }

    void update(double t, PadGrid& grid, ChipKit& kit) {
        if (nextStepT_ < 0.0) nextStepT_ = t;  // first frame

        // Advance the playhead at a fixed tempo.
        if (t >= nextStepT_) {
            playCol_ = (playCol_ + 1) % 8;
            nextStepT_ += stepSeconds_;
            // Trigger every lit cell in the new column.
            for (int r = 0; r < 8; ++r)
                if (step_[r][playCol_]) kit.play(r);
        }

        // Draw: programmed steps + the playhead column overlay.
        for (int r = 0; r < 8; ++r) {
            for (int c = 0; c < 8; ++c) {
                bool lit = step_[r][c];
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
    int    playCol_     = 0;
    double nextStepT_   = -1.0;
    double stepSeconds_ = 0.14;  // ~107 BPM in 16ths
};

// -----------------------------------------------------------------------------
// 3. Ripples
// -----------------------------------------------------------------------------
class RippleMode {
public:
    void enter() { ripples_.clear(); }

    void onPad(int col, int row, bool pressed, ChipKit& kit) {
        if (!pressed) return;
        ripples_.push_back({col, row, lastT_});
        kit.play(row);
    }

    void update(double t, PadGrid& grid, ChipKit&) {
        lastT_ = t;
        grid.clear();

        static const int ring[] = {
            lp::color::White, lp::color::Cyan, lp::color::Blue,
            lp::color::Purple, lp::color::DimBlue
        };
        constexpr int ringCount = sizeof(ring) / sizeof(ring[0]);
        constexpr double speed = 7.0;   // cells per second
        constexpr double maxR  = 11.0;  // lifetime in cell-radii

        for (const auto& rp : ripples_) {
            double radius = (t - rp.startT) * speed;
            for (int r = 0; r < 8; ++r) {
                for (int c = 0; c < 8; ++c) {
                    double d = std::hypot(double(c - rp.col), double(r - rp.row));
                    if (std::abs(d - radius) < 0.8) {
                        int idx = std::min(int(radius), ringCount - 1);
                        grid.set(c, r, ring[idx]);
                    }
                }
            }
        }

        // Drop ripples that have expanded past the grid.
        std::vector<Ripple> alive;
        for (const auto& rp : ripples_)
            if ((t - rp.startT) * speed < maxR) alive.push_back(rp);
        ripples_.swap(alive);
    }

private:
    struct Ripple { int col, row; double startT; };  // plain data
    std::vector<Ripple> ripples_;
    double lastT_ = 0.0;
};
