#pragma once

// =============================================================================
// ChipKit - 8 pre-built ChipSound voices, one per grid row
// =============================================================================
// A C-major pentatonic spread across the 8 rows (low at the bottom). Modes
// just call play(row) to make a chiptune blip.
// =============================================================================

#include <TrussC.h>

#include <array>

using namespace tc;

class ChipKit {
public:
    std::array<Sound, 8> voices;

    void setup() {
        // C pentatonic-ish, rows bottom -> top.
        const float hz[8] = {
            261.63f, 311.13f, 349.23f, 392.00f,
            466.16f, 523.25f, 622.25f, 698.46f
        };
        for (int i = 0; i < 8; ++i) {
            // Low per-voice volume so overlapping rows (sequencer / ripples)
            // don't sum into clipping. Square waves are harmonically rich, so
            // a modest sustain keeps it from getting harsh.
            ChipSoundNote note{Wave::Square, hz[i], 0.18f, 0.18f};
            note.attack  = 0.005f;
            note.decay   = 0.04f;
            note.sustain = 0.35f;
            note.release = 0.06f;
            voices[i] = note.build();
        }
    }

    void play(int row) {
        if (row < 0 || row > 7) return;
        voices[row].stop();
        voices[row].play();
    }
};
