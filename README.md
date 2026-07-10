# VoxChord

VoxChord is a Windows VST3 / Standalone digital choir plugin for live vocal performance.

It takes a monophonic vocal input, listens to incoming MIDI notes, and generates playable harmony voices in real time. The goal is not to be a full vocal production suite, but a lightweight instrument-like effect that can be performed from a MIDI keyboard.

Current version: `0.5.0` beta.

## What It Does

- Converts a single vocal input into MIDI-controlled harmony voices.
- Runs as both VST3 and Standalone.
- Provides up to 8 harmony voice slots, selected with the `Voices` dropdown.
- Two selectable harmony engines: **High Quality** (TD-PSOLA, default) and Classic (windowed shifter).
- Optional `Auto Tune` lead that snaps the dry vocal to the nearest chromatic note.
- Offers live controls for Glide, Character, Spread, Dry/Wet, Input Gain, Output, Mono Out, and Panic.
- Shows MIDI note status and input/output levels on one screen.
- Keeps processing low-latency and lightweight for live use.

## Current Status

`0.5.0` is a beta. The plugin is usable for testing and performance experiments, but pitch detection and pitch shifting are still lightweight/experimental and should not be treated as finished studio-grade processing.

Known practical notes:

- Monophonic vocal input is assumed. Polyphonic audio input is not supported.
- VST3 uses the host-provided left/input channel as the vocal input.
- Standalone can choose Auto, Input 1, Input 2, or Mix 1+2.
- The two engines do not match in wet level: Classic sits roughly 7 dB below High Quality. This is expected — rebalance with `Dry/Wet` and `Output` when switching.
- The `Retune` parameter (Lead Tune snap speed) has no on-screen knob by design. It defaults to instant hard-tune and is reachable through host automation.

## Basic Use

1. Insert VoxChord as an audio effect on a vocal track, or launch the Standalone app.
2. Route a monophonic vocal signal into the plugin.
3. Send MIDI notes to VoxChord from a MIDI keyboard or MIDI track.
4. Raise `Dry/Wet` to hear the generated choir voices.
5. Adjust `Voices`, `Glide`, `Character`, `Spread`, `Input Gain`, and `Output` for the performance.
6. Use `PANIC` if MIDI notes become stuck or the harmony state needs to be reset.

## Main Controls

- `Voices`: Number of active harmony voices (1-8).
- `Glide`: Pitch glide between MIDI notes: `None`, `Weak`, or `Strong`.
- `Character Type`: Selects the harmony voice colour: Warm, Bright, Vowel, or Digital.
- `Character Amount`: Controls how strongly the selected Character is applied.
- `Spread`: Sets stereo width for multiple harmony voices.
- `Dry/Wet`: Mixes the dry (or tuned lead) signal with the harmony output.
- `Input Gain`: Adjusts input level before detection and harmony generation.
- `Output`: Adjusts final output level.
- `Auto Tune`: Replaces the dry side with a chromatically tuned lead.
- `High Quality`: Selects the PSOLA harmony engine (on by default). Turn it off for the lighter Classic shifter.
- `Mono Out`: Sums final stereo output to mono.
- `PANIC`: Clears active MIDI voices, stuck notes, and clip indicators.

Knobs and the header sliders return to their default on double-click. Clicking a level meter clears its clip indicator.

## Meters And MIDI Display

- The Level area shows input and stereo output meters, with peak-hold markers and latching clip indicators.
- The MIDI area shows active MIDI notes and a display-only mini keyboard.
- Debug builds additionally show MIDI delivery counters and pitch diagnostics; Release builds show neither.

## Latency

- The dry path is never delayed.
- With `High Quality` enabled, the wet harmony voices are delayed by a constant ~23.7 ms at 44.1 kHz. Classic adds only its own window delay (~9-23 ms depending on input pitch).
- The tuned lead always uses the low-latency windowed shifter, regardless of the engine setting.
- No latency is reported to the host: this is a live-first design, and the wet choir layer does not need phase alignment with the dry signal.

## Dependencies

VoxChord expects these repositories as siblings of the project folder:

```text
C:\dev\VST_Dev\VoxChord
C:\dev\VST_Dev\JUCE                  # framework
C:\dev\VST_Dev\melatonin_blur        # cached drop/inner shadows (GUI)
C:\dev\VST_Dev\melatonin_inspector   # component inspector (Debug builds only)
```

Clone them into the parent directory before building. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for their licences.

## Build Notes

VoxChord uses JUCE and CMake. Configure and build with Visual Studio on Windows:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
cmake --build build --config Release
```

The CMake project builds both VST3 and Standalone targets.

Debug VST3 builds identify as `VoxChord_dbg`, while Release builds identify as `VoxChord`. `melatonin_inspector` is linked into Debug builds only and is toggled with `F12`.

## Documentation Split

- `README.md`: User-facing overview, basic operation, and build notes.
- `SPEC.md`: Current implementation details for developers.
- `TESTLOG.md`: Version-by-version changes, test results, and user verification history.

## Limitations

- Pitch detection is still experimental for real vocal material.
- The pitch shifter is designed for low latency, not offline/studio-grade transparency.
- Retune-to-a-new-note is floored at roughly 27 ms by the pitch detector's own analysis latency.
- No internal reverb, delay, preset browser, chord detection, scale engine, or AI voice conversion is implemented.
- Standalone device settings persistence is not implemented as a VoxChord-specific feature yet.

## Licence

VoxChord is released under the GNU Affero General Public License v3.0. See [LICENSE](LICENSE), and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for third-party components.

## Demo

Preview demo video: https://youtu.be/ppXFUsPrM2E
