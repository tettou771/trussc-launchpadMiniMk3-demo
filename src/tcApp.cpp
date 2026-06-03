#include "TrussC.h"
#include "tcApp.h"

// =============================================================================
// Threading model
// -----------------------------------------------------------------------------
// Two threads besides the engine touch the mode/grid state:
//   - libremidi's input thread (onPad / onArrow), and
//   - the async scheduler thread (asyncTick), which clocks the sequencer /
//     ripple at precise times instead of the render frame rate.
// `mtx_` guards everything they share with draw()/update().
//
// One rule makes it deadlock-free: **cancelAllAsyncTimers() is only ever called
// without holding mtx_.** It waits for an in-flight asyncTick to finish, and
// that tick needs mtx_ - so holding mtx_ while cancelling would deadlock.
// =============================================================================

void tcApp::setup() {
    kit_.setup();

    // Device input fires on libremidi's input thread. The handlers below lock
    // mtx_ internally (modePad) or manage it themselves (switchMode), so the
    // lambdas must NOT hold the lock here.
    lp_.onPad = [this](int col, int row, bool pressed, int /*vel*/) {
        modePad(col, row, pressed);
    };
    lp_.onArrow = [this](lp::Arrow arrow, bool pressed) {
        if (!pressed) return;
        if (arrow == lp::Arrow::Right) switchMode(+1);
        else if (arrow == lp::Arrow::Left) switchMode(-1);
    };

    for (auto& row : lastSent_.color) row.fill(-1);  // force a full LED refresh

    modeEnter();  // enter the default mode (Paint) + start its clock (none)
    // The device is connected in update() once midiReady() (deferred on the web).
}

void tcApp::update() {
    if (started_ || !midiReady()) return;
    started_ = true;

#if defined(__EMSCRIPTEN__)
    // Web MIDI is granted without sysex, so Programmer mode can't be entered.
    logNotice("launchpad") << "web build: Launchpad needs sysex (native only)";
#else
    std::lock_guard<std::mutex> lock(mtx_);
    deviceConnected_ = lp_.connect();  // default match handles win/mac/linux names
    if (deviceConnected_) {
        logNotice("launchpad") << "Connected: programmer mode on";
        lp_.setArrowLed(lp::Arrow::Left,  lp::color::DimBlue);
        lp_.setArrowLed(lp::Arrow::Right, lp::color::DimBlue);
        for (auto& row : lastSent_.color) row.fill(-1);  // force full refresh
        flushLeds();                                     // push the current grid now
    } else {
        logWarning("launchpad") << "No Launchpad found (connect one over USB and relaunch)";
    }
#endif
}

void tcApp::draw() {
    std::lock_guard<std::mutex> lock(mtx_);  // grid_/mode_ are touched off-thread

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
    cancelAllAsyncTimers();  // stop the clock (no mtx_ held) before tearing down
    lp_.disconnect();        // return the device to Live mode
}

// -----------------------------------------------------------------------------
// Orchestration (locking lives here; the dispatch helpers below assume the lock)
// -----------------------------------------------------------------------------
void tcApp::modeEnter() {
    cancelAllAsyncTimers();   // stop the previous mode's clock (NO mtx_ held)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        grid_.clear();
        enterActive();
        renderActive();
        flushLeds();
    }
    startActiveClock();       // schedule the new mode's precise clock (if any)
    logNotice("launchpad") << "mode: " << modeName();
}

void tcApp::modePad(int col, int row, bool pressed) {
    std::lock_guard<std::mutex> lock(mtx_);
    padActive(col, row, pressed);
    renderActive();
    flushLeds();
}

// Runs on the scheduler thread, once per precise clock tick.
void tcApp::asyncTick() {
    std::lock_guard<std::mutex> lock(mtx_);
    tickActive();
    renderActive();
    flushLeds();
}

void tcApp::switchMode(int delta) {
    cancelAllAsyncTimers();   // stop the old clock first (NO mtx_ held)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        constexpr int count = 3;
        mode_ = static_cast<Mode>(((int)mode_ + delta + count) % count);
    }
    modeEnter();
}

// -----------------------------------------------------------------------------
// Per-mode dispatch - one switch each (all assume mtx_ is held)
// -----------------------------------------------------------------------------
void tcApp::enterActive() {
    switch (mode_) {
        case Mode::Paint:     paint_.enter();     break;
        case Mode::Sequencer: sequencer_.enter(); break;
        case Mode::Ripple:    ripple_.enter();    break;
    }
}

void tcApp::padActive(int col, int row, bool pressed) {
    switch (mode_) {
        case Mode::Paint:     paint_.onPad(col, row, pressed, kit_);     break;
        case Mode::Sequencer: sequencer_.onPad(col, row, pressed, kit_); break;
        case Mode::Ripple:    ripple_.onPad(col, row, pressed, kit_);    break;
    }
}

void tcApp::tickActive() {
    switch (mode_) {
        case Mode::Paint:     break;                       // event-driven, no clock
        case Mode::Sequencer: sequencer_.step(kit_); break;
        case Mode::Ripple:    ripple_.tick();        break;
    }
}

void tcApp::renderActive() {
    switch (mode_) {
        case Mode::Paint:     paint_.render(grid_);     break;
        case Mode::Sequencer: sequencer_.render(grid_); break;
        case Mode::Ripple:    ripple_.render(grid_);    break;
    }
}

void tcApp::startActiveClock() {
    switch (mode_) {
        case Mode::Sequencer: callEveryAsync(0.14, [this] { asyncTick(); }); break;  // ~107 BPM 16ths
        case Mode::Ripple:    callEveryAsync(0.03, [this] { asyncTick(); }); break;  // ring growth
        case Mode::Paint:     break;                                                 // no clock
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

// Diff grid_ against what the device last showed and send only the changes.
// Caller holds mtx_ (so LED output is serialized across threads).
void tcApp::flushLeds() {
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

// -----------------------------------------------------------------------------
// Screen layout helpers
// -----------------------------------------------------------------------------
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
