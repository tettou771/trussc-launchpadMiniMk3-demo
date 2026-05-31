#pragma once

#include "tcBaseApp.h"
#include "LaunchpadMk3.h"
#include "PadGrid.h"
#include "PadModes.h"
#include "ChipKit.h"

using namespace tc;
using namespace tcx;
using namespace std;

// =============================================================================
// Launchpad Mini Mk3 demo with three switchable modes.
//
//   Device : tap pads to interact; LEFT / RIGHT arrows switch mode.
//   Screen : the 8x8 grid is mirrored 1:1 - a pure visualization of the MIDI
//            I/O. Everything is driven by the device; there is no mouse/keyboard
//            input (this is a MIDI in/out sample).
//
// The active mode is just an enum. tcApp owns one of each mode and dispatches
// to it through a small switch (see modeEnter / modePad / modeUpdate). No
// inheritance, no pointers - easy to read and extend.
// =============================================================================

// The three modes. To add one: add a value here, add a member + a case in
// each of modeEnter / modePad / modeUpdate (tcApp.cpp).
enum class Mode { Paint, Sequencer, Ripple };

class tcApp : public App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void cleanup() override;

private:
    // Dispatch to the active mode (one switch each, all in tcApp.cpp).
    void modeEnter();
    void modePad(int col, int row, bool pressed);
    void modeUpdate(double t);

    const char* modeName() const;
    void switchMode(int delta);
    void pushGridToDevice();

    // Screen layout helpers for the mirror (row 0 is the BOTTOM row).
    float cellSize() const;
    float gridOriginX() const;
    float gridOriginY() const;

    LaunchpadMk3 lp_;
    ChipKit      kit_;
    PadGrid      grid_;
    PadGrid      lastSent_;   // for diffing what we push to the device

    // One instance of each mode; `mode_` selects which is active.
    Mode          mode_ = Mode::Paint;
    PaintMode     paint_;
    SequencerMode sequencer_;
    RippleMode    ripple_;

    bool started_ = false;          // MIDI init done (gated on midiReady())
    bool deviceConnected_ = false;
};
