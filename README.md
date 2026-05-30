# VoxChord

VoxChord is a Windows VST3 / Standalone digital choir plugin for live vocal performance.

It takes a monophonic vocal input, listens to incoming MIDI notes, and generates playable harmony voices in real time. The goal is not to be a full vocal production suite, but a lightweight instrument-like effect that can be performed from a MIDI keyboard.

Current version: `0.2.0` beta initial.

## What It Does

- Converts a single vocal input into MIDI-controlled harmony voices.
- Runs as both VST3 and Standalone.
- Provides up to 8 harmony voice slots, limited by the `Voices` control.
- Offers live controls for Glide, Character, Spread, Dry/Wet, Input Gain, Output, Auto Tune, Mono Out, and Panic.
- Shows MIDI note status and input/output levels on one screen.
- Keeps processing low-latency and lightweight for live use.

## Current Status

`0.2.0` is the first beta baseline. The plugin is usable for testing and performance experiments, but pitch detection and pitch shifting are still lightweight/experimental and should not be treated as finished studio-grade processing.

Known practical notes:

- Monophonic vocal input is assumed.
- Polyphonic audio input is not supported.
- VST3 uses the host-provided left/input channel as the vocal input.
- Standalone can choose Auto, Input 1, Input 2, or Mix 1+2.
- The `Tune` parameter still exists internally, but the visible Tune knob is hidden and the current DSP behaves as a hard-tuned path.

## Basic Use

1. Insert VoxChord as an audio effect on a vocal track, or launch the Standalone app.
2. Route a monophonic vocal signal into the plugin.
3. Send MIDI notes to VoxChord from a MIDI keyboard or MIDI track.
4. Raise `Dry/Wet` to hear the generated choir voices.
5. Adjust `Voices`, `Glide`, `Character`, `Spread`, `Input Gain`, and `Output` for the performance.
6. Use `PANIC` if MIDI notes become stuck or the harmony state needs to be reset.

## Main Controls

- `Voices`: Maximum number of active harmony voices.
- `Glide`: Smooths MIDI note changes and pitch transitions.
- `Character Type`: Selects the harmony voice color: Warm, Bright, Vowel, or Digital.
- `Character Amount`: Controls how strongly the selected Character is applied.
- `Spread`: Sets stereo width for multiple harmony voices.
- `Dry/Wet`: Mixes original/tuned lead and harmony output.
- `Input Gain`: Adjusts input level before detection and harmony generation.
- `Output`: Adjusts final output level.
- `Auto Tune`: Replaces the dry side with a tuned lead when enabled.
- `Mono Out`: Sums final stereo output to mono.
- `PANIC`: Clears active MIDI voices and stuck notes.

## Meters And MIDI Display

- The Level area shows input and stereo output meters.
- The MIDI area shows active MIDI notes and a display-only mini keyboard.
- Debug builds may show additional MIDI delivery counters for VST3 routing diagnostics.

## Build Notes

VoxChord uses JUCE and CMake.

Expected local layout:

```text
C:\dev\VST_Dev\VoxChord
C:\dev\VST_Dev\JUCE
```

Configure and build with Visual Studio on Windows:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
cmake --build build --config Release
```

The CMake project builds both VST3 and Standalone targets.

Debug VST3 builds are intended to identify as `VoxChord_dbg`, while Release builds identify as `VoxChord`.

## Documentation Split

- `README.md`: User-facing overview, basic operation, and build notes.
- `SPEC.md`: Current implementation details for developers.
- `TESTLOG.md`: Version-by-version changes, test results, and user verification history.

## Limitations

- Pitch detection is still experimental for real vocal material.
- The pitch shifter is designed for low latency, not offline/studio-grade transparency.
- No internal reverb, delay, preset browser, chord detection, scale engine, or AI voice conversion is implemented.
- Standalone device settings persistence is not implemented as a VoxChord-specific feature yet.

## Demo

Preview demo video: https://youtu.be/ppXFUsPrM2E
