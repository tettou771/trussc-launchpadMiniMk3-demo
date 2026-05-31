# trussc-launchpadMiniMk3-demo

A small [TrussC](https://github.com/TrussC-org/TrussC) demo for the Novation
**Launchpad Mini [MK3]**. It is a real example of the MIDI add-on
**[tcxMidi](https://github.com/TrussC-org/TrussC/tree/main/addons/tcxMidi)**.

Press the pads: the screen mirrors the 8×8 grid, the device LEDs light up, and
TrussC's ChipSound plays chiptune blips.

![paint / sequencer / ripple]()

## What you need

- A **Novation Launchpad Mini Mk3**
- TrussC (with the `trusscli` tool)
- The `tcxMidi` add-on (it ships with TrussC and is listed in `addons.make`,
  so it is found automatically)

## Build & run

```bash
git clone git@github.com:tettou771/trussc-launchpadMiniMk3-demo.git
cd trussc-launchpadMiniMk3-demo
trusscli update
trusscli run
```

Plug in the Launchpad first. The app switches it to Programmer mode and connects
on its own. With no device it still starts, but the screen stays empty (the pads
come from MIDI).

## Three modes

Switch modes with the device's **◀ / ▶ arrow buttons**.

| Mode | What it does |
|------|--------------|
| **PAINT** | Tap a pad to toggle it on/off (one color per column). Plays a note. |
| **SEQUENCER** | 8 rows = tracks, 8 columns = steps. A playhead sweeps and plays the lit steps. Tap pads to program it. |
| **RIPPLE** | Tap a pad and a ring of light spreads out, with a sound. |

## Other devices

This demo is **for the Launchpad Mini Mk3 only**, because each device speaks MIDI
differently:

- **Launchpad Mini Mk1 / Mk2** — different pad note layout, and no Programmer-mode
  SysEx. Good as a reference, but needs changes to work.
- **Launchpad X / Pro Mk3** — close (11–88 layout), but the Programmer-mode SysEx
  has a different device-ID byte. Edit the SysEx in `LaunchpadMk3.h`.
- **Other pads (APC mini / MPD / Push …)** — totally different. Not usable as-is.

> Making something for another controller? First run tcxMidi's **`example-basic`**
> to see which notes / CCs your device sends, then build from there.
> `example-basic` works with any MIDI device.

## How it works (notes)

- The Mk3 exposes two USB ports (DAW / MIDI). Programmer-mode pads and LEDs use
  the **MIDI** port, so the demo opens that one.
- On start it sends the Programmer-mode SysEx and on exit returns to Live mode.
- LED color = note velocity (Mk3 color palette index).
- **SysEx is required, so this is native-only.** It does not run on the Web
  (Web MIDI has no SysEx). tcxMidi's generic examples do run on the Web.

Device-specific code (port choice, Programmer mode, note layout, palette) lives
in `LaunchpadMk3.h`. The modes are plain classes in `PadModes.h`, picked by a
small enum + switch in `tcApp`. Good starting points to hack on.

## License

MIT
