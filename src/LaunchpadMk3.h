#pragma once

// =============================================================================
// LaunchpadMk3 - thin wrapper around a Novation Launchpad Mini [MK3]
// =============================================================================
// Isolates all the device-specific bits (Programmer mode SysEx, the note/CC
// layout, the colour palette) so the rest of the example only ever talks in
// (col, row) cells and Arrow buttons.
//
// Programmer-mode layout (after enterProgrammerMode()):
//   - 8x8 pads : Note,  note = 11 + row*10 + col  (row 0 = bottom, col 0 = left)
//   - top row  : CC 91..98  (the round arrow/logo buttons)
//   - right col: Note 19,29,...,89  (scene launch buttons; unused here)
//
// Lighting: send the pad's Note On (static colour) / the button's CC with a
// velocity that is an index into the Mk3 colour palette (0 = off).
// =============================================================================

#include <tcxMidi.h>
#include <tcPlatform.h>   // trussc::Platform::isWindows()

#include <array>
#include <functional>
#include <string>
#include <vector>

using namespace tcx;

namespace lp {

// Top-row round buttons (CC numbers in Programmer mode).
enum class Arrow { Up = 91, Down = 92, Left = 93, Right = 94 };

// A handful of Mk3 palette indices (velocity = colour).
namespace color {
    constexpr int Off    = 0;
    constexpr int White  = 3;
    constexpr int Red     = 5;
    constexpr int Orange  = 9;
    constexpr int Yellow  = 13;
    constexpr int Lime     = 17;
    constexpr int Green   = 21;
    constexpr int Mint     = 25;
    constexpr int Cyan    = 37;
    constexpr int Blue    = 45;
    constexpr int Purple  = 49;
    constexpr int Pink    = 53;
    constexpr int DimBlue  = 47;
    constexpr int DimGreen = 19;
}

} // namespace lp

class LaunchpadMk3 {
public:
    // pressed=false means released; vel is the raw note/CC value.
    std::function<void(int col, int row, bool pressed, int vel)> onPad;
    std::function<void(lp::Arrow arrow, bool pressed)>           onArrow;

    // Open the Launchpad and switch it into Programmer mode.
    //
    // The Mk3 exposes TWO port pairs: a "DAW" interface (Session/DAW control)
    // and a "MIDI" interface. Per Novation's programmer reference, Programmer
    // mode pad/button messages and LED control travel on the *MIDI* interface,
    // so we deliberately pick that one - opening the DAW port would light
    // nothing and receive only the side buttons.
    bool connect(const std::string& match = "LPMiniMK3") {
        int inIdx  = pickProgrammerPort(MidiIn::listDevices(),  match);
        int outIdx = pickProgrammerPort(MidiOut::listDevices(), match);
        if (inIdx < 0 || outIdx < 0) return false;
        if (!in_.openPort(inIdx) || !out_.openPort(outIdx)) {
            in_.closePort();
            out_.closePort();
            return false;
        }
        enterProgrammerMode();
        clear();
        // Event-driven: dispatch on libremidi's input thread (lowest jitter),
        // not by polling once per frame. onPad/onArrow run on that thread.
        inListener_ = in_.onMessage.listen([this](MidiMessage& m) { dispatch(m); });
        return true;
    }

    // Pick the Mk3's Programmer-mode port (the standalone "MIDI" interface, not
    // the "DAW" one). The two ports are named differently per platform, so we
    // branch on the OS rather than guessing from one heuristic:
    //
    //   macOS / Linux : ports are "LPMiniMK3 DAW" and "LPMiniMK3 MIDI"
    //                   -> pick the one with "MIDI" and without "DAW".
    //   Windows       : BOTH ports carry "MIDI" and neither carries "DAW".
    //                   The 1st (DAW) port is the bare "LPMiniMK3 MIDI"; the real
    //                   Programmer port is the 2nd, wrapped in parentheses as
    //                   "MIDIIN2 (LPMiniMK3 MIDI)" / "MIDIOUT2 (LPMiniMK3 MIDI)".
    //                   -> pick the one containing "(LPMiniMK3".
    static int pickProgrammerPort(const std::vector<MidiDeviceInfo>& devices,
                                  const std::string& match) {
        int fallback = -1;
        for (const auto& d : devices) {
            if (!isDeviceMatch(d.name, match)) continue;
            if (fallback < 0) fallback = d.portNumber;
            if (trussc::Platform::isWindows()) {
                if (d.name.find("(LPMiniMK3") != std::string::npos) return d.portNumber;
            } else {
                if (d.name.find("MIDI") != std::string::npos &&
                    d.name.find("DAW") == std::string::npos) return d.portNumber;
            }
        }
        return fallback;
    }

    // Match the caller's hint plus the known device identifiers, since the
    // Launchpad Mini Mk3 enumerates as "LPMiniMK3" on win/mac but as
    // "Launchpad Mini MK3" on Linux (ALSA uses the USB product string).
    static bool isDeviceMatch(const std::string& name, const std::string& match) {
        return name.find(match) != std::string::npos ||
               name.find("LPMiniMK3") != std::string::npos ||
               name.find("Launchpad") != std::string::npos;
    }

    // Return to the normal (Live) layout and close the ports.
    void disconnect() {
        inListener_ = tc::EventListener{};  // unsubscribe
        if (out_.isOpen()) {
            clear();
            enterLiveMode();
        }
        in_.closePort();
        out_.closePort();
    }

    bool isConnected() const { return in_.isOpen() && out_.isOpen(); }

    // Dispatch one incoming message to onPad / onArrow. Runs on libremidi's
    // input thread (event-driven), so the callbacks must guard shared state.
    void dispatch(const MidiMessage& m) {
        if (m.isControlChange()) {
            int cc = m.getControl();
            if (cc >= 91 && cc <= 98 && onArrow) {
                onArrow(static_cast<lp::Arrow>(cc), m.getValue() > 0);
            }
        } else if (m.getStatus() == MidiStatus::NoteOn ||
                   m.getStatus() == MidiStatus::NoteOff) {
            int col, row;
            if (noteToCell(m.getPitch(), col, row) && onPad) {
                onPad(col, row, m.isNoteOn(), m.getVelocity());
            }
        }
    }

    // -------------------------------------------------------------------------
    // Lighting
    // -------------------------------------------------------------------------
    void setPad(int col, int row, int color) {
        if (!inRange(col, row)) return;
        out_.sendNoteOn(1, padNote(col, row), color);
    }

    void setArrowLed(lp::Arrow arrow, int color) {
        out_.sendControlChange(1, static_cast<int>(arrow), color);
    }

    void clear() {
        for (int r = 0; r < 8; ++r)
            for (int c = 0; c < 8; ++c)
                setPad(c, r, lp::color::Off);
        for (int cc = 91; cc <= 94; ++cc)
            out_.sendControlChange(1, cc, lp::color::Off);
    }

    static bool inRange(int col, int row) {
        return col >= 0 && col < 8 && row >= 0 && row < 8;
    }

private:
    static int padNote(int col, int row) { return 11 + row * 10 + col; }

    static bool noteToCell(int note, int& col, int& row) {
        col = note % 10 - 1;
        row = note / 10 - 1;
        return inRange(col, row);
    }

    // SysEx: F0 00 20 29 02 0D 0E <mode> F7   (mode: 1 = Programmer, 0 = Live)
    void enterProgrammerMode() { sendModeSysex(1); }
    void enterLiveMode()       { sendModeSysex(0); }
    void sendModeSysex(unsigned char mode) {
        out_.sendSysex({0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x0E, mode, 0xF7});
    }

    MidiIn  in_;
    MidiOut out_;
    tc::EventListener inListener_;  // onMessage subscription (RAII)
};
