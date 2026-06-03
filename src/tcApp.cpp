#include "TrussC.h"
#include "tcApp.h"

void tcApp::setup() {
    kit_.setup();

    // Wire device input. A pad event drives the active mode; the arrows switch.
    lp_.onPad = [this](int col, int row, bool pressed, int /*vel*/) {
        modePad(col, row, pressed);
    };
    lp_.onArrow = [this](lp::Arrow arrow, bool pressed) {
        if (!pressed) return;
        if (arrow == lp::Arrow::Right) switchMode(+1);
        else if (arrow == lp::Arrow::Left) switchMode(-1);
    };

    // Force a full LED refresh on the first frame.
    for (auto& row : lastSent_.color) row.fill(-1);

    modeEnter();
    // The device is connected in update() once midiReady() (deferred on the web).
    // Note: web MIDI is requested without sysex, so Programmer mode (LEDs) is
    // native-only; on the web this stays a screen/mouse demo.
}

void tcApp::update() {
    if (!started_ && midiReady()) {
        started_ = true;
#if defined(__EMSCRIPTEN__)
        // Web MIDI is granted without sysex, so Programmer mode can't be entered
        // (sending 0xF0 throws NotAllowedError). The web build can't drive it.
        logNotice("launchpad") << "web build: Launchpad needs sysex (native only)";
#else
        deviceConnected_ = lp_.connect();  // default match handles win/mac/linux names
        if (deviceConnected_) {
            logNotice("launchpad") << "Connected: programmer mode on";
            lp_.setArrowLed(lp::Arrow::Left,  lp::color::DimBlue);
            lp_.setArrowLed(lp::Arrow::Right, lp::color::DimBlue);
        } else {
            logWarning("launchpad") << "No Launchpad found (connect one over USB and relaunch)";
        }
#endif
    }

    lp_.update();
    modeUpdate(getElapsedTime());
    if (deviceConnected_) pushGridToDevice();
}

void tcApp::draw() {
    clear(0.08f);

    const float cs = cellSize();
    const float ox = gridOriginX();
    const float oy = gridOriginY();

    // Header.
    setColor(1.0f);
    drawBitmapString("Launchpad Mini Mk3 - mode: " + string(modeName()), 20, 26);

    // Draw the 8x8 grid (row 0 at the bottom).
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            float x = ox + c * cs;
            float y = oy + (7 - r) * cs;
            setColor(screenColor(grid_.get(c, r)));
            drawRect(x, y, cs - 4, cs - 4);
        }
    }

    // Footer / help.
    setColor(0.6f);
    float fy = oy + 8 * cs + 24;
    drawBitmapString(deviceConnected_ ? "Device connected - press pads; LEFT/RIGHT arrows switch mode"
                                      : "No device - connect a Launchpad Mini Mk3",
                     20, fy);
    drawBitmapString("[1] Paint   [2] Sequencer   [3] Ripple", 20, fy + 18);
}

void tcApp::cleanup() {
    lp_.disconnect();  // returns the device to Live mode
}

// -----------------------------------------------------------------------------
// Mode dispatch - one switch per action, all in one place
// -----------------------------------------------------------------------------
void tcApp::modeEnter() {
    switch (mode_) {
        case Mode::Paint:     paint_.enter();     break;
        case Mode::Sequencer: sequencer_.enter(); break;
        case Mode::Ripple:    ripple_.enter();    break;
    }
}

void tcApp::modePad(int col, int row, bool pressed) {
    switch (mode_) {
        case Mode::Paint:     paint_.onPad(col, row, pressed, kit_);     break;
        case Mode::Sequencer: sequencer_.onPad(col, row, pressed, kit_); break;
        case Mode::Ripple:    ripple_.onPad(col, row, pressed, kit_);    break;
    }
}

void tcApp::modeUpdate(double t) {
    switch (mode_) {
        case Mode::Paint:     paint_.update(t, grid_, kit_);     break;
        case Mode::Sequencer: sequencer_.update(t, grid_, kit_); break;
        case Mode::Ripple:    ripple_.update(t, grid_, kit_);    break;
    }
}

const char* tcApp::modeName() const {
    switch (mode_) {
        case Mode::Paint:     return "PAINT";
        case Mode::Sequencer: return "SEQUENCER";
        case Mode::Ripple:    return "RIPPLE";
    }
    return "?";
}

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
void tcApp::switchMode(int delta) {
    // Cycle through the enum values (Paint=0 .. Ripple=2). delta is +1 / -1.
    constexpr int count = 3;
    int next = ((int)mode_ + delta + count) % count;
    mode_ = static_cast<Mode>(next);
    modeEnter();
    logNotice("launchpad") << "mode: " << modeName();
}

void tcApp::pushGridToDevice() {
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            int color = grid_.get(c, r);
            if (color != lastSent_.color[r][c]) {
                lp_.setPad(c, r, color);
                lastSent_.color[r][c] = color;
            }
        }
    }
}

float tcApp::cellSize() const {
    float avail = std::min((float)getWindowWidth() - 40.0f,
                           (float)getWindowHeight() - 110.0f);
    return avail / 8.0f;
}
float tcApp::gridOriginX() const {
    return (getWindowWidth() - cellSize() * 8.0f) * 0.5f;
}
float tcApp::gridOriginY() const {
    return 44.0f;
}
