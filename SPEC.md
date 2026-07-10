# VoxChord Source Specification

Last updated: 2026-07-08
Project version: 0.3.1
Status: 0.3 experimental (dev-0.3 branch, low-pitch instability diagnostics)

This document describes the current VoxChord 0.3.1 implementation on `dev-0.3`. The `main` branch remains frozen at 0.2.0.

Documentation split:

- `README.md`: User-facing overview, basic operation, and build notes.
- `SPEC.md`: Developer-facing implementation details.
- `TESTLOG.md`: Version-by-version changes, self-test results, and user verification history.

## Current Baseline

- VoxChord 0.2.0 is the first beta baseline.
- Target formats are VST3 and Standalone on Windows 11 via JUCE and CMake.
- The plugin is a low-latency, MIDI-controlled digital choir effect for monophonic vocal input.
- VST3 and Standalone share the same `AudioProcessor` implementation.
- Release builds identify as `VoxChord`; Debug VST3 metadata identifies as `VoxChord_dbg` to distinguish host scans.
- Plugin IDs, manufacturer IDs, APVTS parameter IDs, and current DSP behavior are treated as stable for this beta baseline.

## Build Configuration

- CMake project version: `0.3.1`
- Main target: `VoxChord`
- Diagnostic tooling target (Debug/test only, no plugin/DSP impact): `VoxChordOfflineTest` (console app; sources `tests/OfflineDiagnosticsMain.cpp` + `Source/SimpleChoirEngine.cpp` + `Source/MidiVoiceState.cpp`, links `juce::juce_audio_basics`). Drives `SimpleChoirEngine::render()` headlessly with a steady sine + a held MIDI note and prints the D1 diagnostics per target note.
- Plugin formats: `VST3`, `Standalone`
- JUCE path: `../JUCE`
- Linked JUCE module: `juce::juce_audio_utils`
- MIDI input: enabled via `NEEDS_MIDI_INPUT TRUE`
- MIDI output: disabled
- VST3 replacement: `JUCE_VST3_CAN_REPLACE_VST2=0`
- ASIO support: `JUCE_ASIO=1`
- Web browser and curl: disabled
- Reported processing latency: `0 samples`

## Source File Structure

`Source/LevelMeterState.h`

- Stores input and output peak/clip state with atomics.
- Audio thread publishes block peaks with `publish()`.
- GUI thread reads snapshots through getters.
- Clip threshold is `0.999f`.

`Source/MidiVoiceState.h`, `Source/MidiVoiceState.cpp`

- Owns MIDI note allocation and active-note snapshots.
- Maximum active voices: `8`.
- Active note snapshot uses fixed-size storage and `-1` for inactive slots.
- Note On updates an existing matching note, otherwise uses an empty slot, otherwise steals a voice.
- Voice stealing favors the active voice whose current MIDI note is nearest to the incoming note; oldest age is used as a tie breaker.
- Note Off releases matching voices.
- All Notes Off, All Sound Off, Reset All Controllers, and Panic reset active voices.
- `enforceVoiceLimit()` releases slots above the current Voice Count parameter.

`Source/PluginProcessor.h`, `Source/PluginProcessor.cpp`

- Implements `VoxChordAudioProcessor`.
- Owns APVTS, MIDI state, choir DSP, dry/wet buffers, pitch/debug snapshots, and level meters.
- Accepts MIDI, does not produce MIDI, and is not a MIDI effect.
- Handles Panic through an atomic request consumed at the start of `processBlock()`.
- Selects mono vocal input, applies input gain, renders wet harmony and optional tuned lead, mixes dry/wet, applies output gain, then publishes meters.
- VST3 uses channel 0/L as the vocal input; Standalone can use the `inputSource` selector.
- Debug startup self tests are disabled by default. They can be re-enabled in code for pitch detector or pitch shifter diagnostics.

`Source/PluginEditor.h`, `Source/PluginEditor.cpp`

- Implements the live-oriented single-window GUI.
- Main areas: Header, Harmony, MIDI, and Level.
- Header contains logo/version, Input Gain, Output, Auto Tune, Input Source, Mono Out, High Quality, and PANIC.
- Harmony contains Voices, Glide, Character, Spread, and Dry/Wet.
- Character groups Type and Amount as one visible function.
- MIDI area shows active notes, a display-only mini keyboard, and current MIDI delivery counters in Debug builds.
- Level area shows vertical input and stereo output meters.
- GUI refresh timer runs at `30 Hz`.

`Source/SimpleChoirEngine.h`, `Source/SimpleChoirEngine.cpp`

- Implements wet choir rendering, pitch detection, pitch tracking, pitch shifting, per-voice envelopes, Character processing, and stereo Spread.
- Uses mono delay-line input and stereo wet output.
- Current pitch shifter is a lightweight delay-window design with input-synced window length when a valid smoothed pitch is available.
- Character processing uses per-voice lightweight biquad EQ, soft saturation, and per-slot detune/gain/delay offsets; mode identities (0.3.8): **Warm** = dark/thick (high shelf 3.5 kHz -9 dB, low-mid 300 Hz +4.5 dB, stronger saturation, light slot detune x0.4), **Bright** = hard/airy (high shelf 6 kHz +7 dB, presence 3.2 kHz +4.5 dB, low cut 280 Hz -4 dB, no detune), **Vowel** = moving-mouth choir (per-slot formant peaks, gain x2.2, Q 2.5, centers swept +/-18% by slow per-slot LFOs 0.08-0.22 Hz, full slot detune), **Digital** = robotic/lo-fi (sample-hold decimator up to 10 samples, hard saturation, high shelf 4.5 kHz +6 dB, zero detune).
- Debug self-test functions remain in the codebase but are not run during normal realtime processing.

`Source/VoxChordParameters.h`, `Source/VoxChordParameters.cpp`

- Defines the APVTS parameter layout.
- Parameter IDs are compatibility-sensitive and should not be renamed without an explicit migration plan.
- Current parameter IDs: `voiceCount`, `tune`, `glide`, `character`, `characterMode`, `spread`, `dryWet`, `outputLevel`, `inputGainDb`, `inputSource`, `leadTuneEnabled`, `monoOutputEnabled`, `psolaEnabled`.

`Source/VoxChordDebugPluginName.h`

- Debug-only helper for VST3 wrapper and manifest-helper compilation.
- Undefines JUCE's default `JucePlugin_Name` and `JucePlugin_Desc` macros and redefines them as `VoxChord_dbg`.
- Keeps Release metadata as `VoxChord` while allowing Debug VST3 builds to appear separately in hosts.

## Audio Processor Flow

Input/output:

- Audio input: mono or stereo layouts are allowed.
- Audio output: stereo is primary; mono and stereo output layouts are allowed.
- MIDI input is required.
- MIDI output is disabled.
- Standalone input source choices: Auto, Input 1, Input 2, Mix 1+2.
- VST3 ignores `inputSource` and uses ch0/L from the host-provided audio input.

Per-block flow:

1. Apply pending Panic request.
2. Parse MIDI messages and update `MidiVoiceState`.
3. Select mono vocal input.
4. Apply smoothed Input Gain.
5. Copy the selected input into the stereo dry buffer.
6. Render wet choir and optional tuned lead with `SimpleChoirEngine`.
7. Publish pitch/debug fields to atomics.
8. Mix dry/wet, optionally using tuned lead as the dry-side source.
9. Apply smoothed Output gain.
10. Apply Mono Out if enabled.
11. Publish input and output meter peaks.

Smoothing:

- Input Gain: approximately `20 ms`.
- Dry/Wet: approximately `20 ms`.
- Output: approximately `20 ms`.
- Lead Tune dry-source switching: approximately `12 ms`.
- Mono Out switching: approximately `12 ms`.

## Parameters

`voiceCount`

- Type: integer
- Range: `1-8`
- Default: `4`
- Controls active MIDI voice slot limit.

`tune`

- Type: float percent
- Range: `0.0-1.0`
- Default: `1.00` (instant hard-tune snap)
- Retune Speed (display name `Retune`): sets the Lead Tune ratio snap time — 90% settle from ~200 ms (0%) to ~1 ms (100%), `settleMs = 1 + 199*(1 - tune)^2`. Independent of Glide.
- The parameter ID stays `tune`; only the display name changed.
- Intentionally has no on-screen knob (one-screen UI principle). The default is the intended live behaviour; the value remains host-automatable for anyone who wants a gentler correction.
- The practical retune-to-a-new-note time is floored by the pitch detector's own latency (~27 ms at the current YIN frame), so tune values above ~0.6 mainly affect the smoothing character rather than the total settle time.

`glide`

- Type: float percent
- Range: `0.0-1.0`
- Default: `0.15`
- Exposed in the GUI as a three-entry dropdown (`None` 0.0, `Weak` 0.15, `Strong` 0.50). The parameter itself stays continuous, so automation may set any value; the dropdown shows the nearest entry.
- Controls MIDI target pitch glide.

`character`

- Type: float percent
- Range: `0.0-1.0`
- Default: `0.0`
- GUI label: `Amount`
- Amount `0%` is clean-equivalent.
- Amount `100%` applies the selected Character Type at full strength.
- Scales Character pitch detune, gain variation, delay offset, and tone shaping.

`characterMode`

- Type: choice
- GUI choices: `Warm`, `Bright`, `Vowel`, `Digital`
- GUI indexes: `0=Warm`, `1=Bright`, `2=Vowel`, `3=Digital`
- DSP internal modes: `1=Warm`, `2=Bright`, `3=Vowel`, `4=Digital`
- Conversion function: `voxchord::characterModeGuiIndexToInternalMode()`
- Default: `Warm`

`spread`

- Type: float percent
- Range: `0.0-1.0`
- Default: `0.55`
- Controls stereo pan spread across active wet voices.

`dryWet`

- Type: float percent
- Range: `0.0-1.0`
- Default: `0.50`
- Mixes dry input or tuned lead with wet choir output.

`outputLevel`

- Type: float dB
- Range: `-24.0-6.0 dB`
- Default: `-3.0 dB`
- Smoothed final output gain.

`inputGainDb`

- Type: float dB
- Range: `-24.0-24.0 dB`
- Default: `0.0 dB`
- Smoothed input gain after input source selection.
- Affects pitch detection, harmony input, dry path, and input meter consistently.

`inputSource`

- Type: choice
- Choices: `Auto`, `Input 1`, `Input 2`, `Mix 1+2`
- Default: `Auto`
- Used by Standalone input selection.
- Ignored by VST3 DSP routing.

`leadTuneEnabled`

- Type: bool
- GUI label: `Auto Tune`
- Default: `false`
- Replaces the dry side with a chromatically tuned lead when enabled.

`monoOutputEnabled`

- Type: bool
- GUI label: `Mono Out`
- Default: `false`
- Replaces stereo output with `0.5 * (left + right)` on both channels after Dry/Wet and Output gain.

`psolaEnabled`

- Type: bool
- GUI label: `High Quality`
- Default: `true` (TD-PSOLA harmony shifter)
- Selects the TD-PSOLA shifter bank for the wet harmony voices. Unchecking it falls back to the Classic windowed shifter, which is retained. See "Engine Modes" below. The parameter ID stays `psolaEnabled` for state compatibility.

## Engine Modes

The wet pitch shifter stage exists in two selectable implementations; pitch detection, MIDI voice management, envelopes, Character tone/gain/pitch offsets, and Spread are shared by both. The tuned **Lead** always uses the window shifter regardless of engine mode (it replaces the dry monitor signal, where the window shifter's lower, pitch-synced latency and snappier retune feel more natural); the PSOLA bank covers the harmony voices only.

- **Classic** (`psolaEnabled = false`): windowed dual-tap delay-line shifter. Pitch ratio clamp `0.25..8`. No added wet-path latency beyond the shifter's own window delay (~9-23 ms depending on input pitch).
- **High Quality / PSOLA** (`psolaEnabled = true`, **default**): per-voice TD-PSOLA bank (4 harmony voices), configuration plan B50 (`Source/SimpleChoirEngine.h`: `psolaMinF0Hz = 90`, `psolaRightHalfFactor = 0.5`, grain cap 1 period). Wet-path delay is a constant ~23.7 ms @44.1 kHz; the dry path is unaffected and no latency is reported to the host (live-first design; the wet choir layer does not require phase alignment with the dry signal). Pitch ratio clamp is wider (`1/16..8`), so deep downshifts below ratio 0.25 reach pitch instead of clamping. The PSOLA path additionally uses: pitch-mark peak search on a ~700 Hz lowpassed guide (robust against formant ripple), a +/-8%-per-block period slew limit, fractional-sample grain placement, per-grain loudness compensation (duty/ratio based, cap +6.8 dB) plus a slow (~250 ms) input-RMS-parity loudness normalization (clamp 0.5..2.8, open loop) that keeps wet RMS within ~1 dB of unison across ratios and vocal timbres, and a per-voice tracking highpass at 0.6 * target f0 (sub-fundamental output is artifact energy by construction).
- The PSOLA bank runs every block in both modes (total CPU cost ~0.7% of one core) so A/B switching is instant and the shifters stay warm. Character per-slot delay offsets are not applied on the PSOLA path; all other Character components apply in both modes.
- **Voiced/unvoiced split (PSOLA path only)**: unvoiced input (consonants, breath; detector hysteresis on `voiced`/`confidence`, on > 0.75, off < 0.55) is not pitch-shifted — the shifters crossfade (~15 ms) to a latency-matched dry copy, so consonants stay natural in the harmony. The loudness normalization freezes while unvoiced.
- **Wet level differs between the two engines** (Classic sits roughly 7 dB below High Quality at unison, because Classic uses a fixed per-voice gain while PSOLA normalizes to input-RMS parity). This is accepted as specified behaviour, not a defect: switching engines is expected to change the wet balance, and Dry/Wet plus Output compensate.
- Both engines are retained. High Quality is the default; unchecking it selects Classic.

## MIDI Voice Behavior

- Maximum voices: `8`.
- Voice Count can restrict the number of active slots below the maximum.
- Repeated Note On for the same MIDI note updates the existing voice.
- Note Off releases matching active voices.
- All Notes Off, All Sound Off, Reset All Controllers, and Panic reset MIDI voices.
- MIDI handling is independent of audio input level and voiced/unvoiced detection.
- Standalone and VST3 use the same MIDI processing path.

## Pitch State

Primary fields:

- `inputRmsDb`: frame RMS in dBFS.
- `rawPitchHz`: direct pitch detector result.
- `correctedPitchHz`: harmonic-corrected pitch result.
- `displayStablePitchHz`: smoothed GUI/debug pitch.
- `correctionInputPitchHz`: pitch used for harmony ratio calculation (median + fast-attack smoothed).
- `tunePitchHz`: harmonic-corrected pitch with no median/smoothing, used only by the Lead Tune (Retune) path so the tuner snaps directly; `0` when unvoiced.
- `confidence`: pitch confidence derived from CMNDF.
- `voiced`: voiced/unvoiced state after RMS/confidence filtering and hold handling.
- `ratioSmoothingCoefficient`: current per-voice pitch-ratio smoothing coefficient.

Compatibility aliases:

- `stablePitchHz`: display stable pitch alias.
- `harmonyPitchHz`: correction input pitch alias.

D1 low-pitch diagnostics (observation only, does not affect DSP behavior):

- `windowPitchHz`: smoothed pitch used for grain/window length calculation.
- `representativeVoiceMidiNote`: MIDI note of the lowest-target active voice, or `-1` if none.
- `representativeGrainWindowSamples`: that voice's current grain window length in samples.
- `representativePitchRatioRaw` / `representativePitchRatioClamped`: that voice's target-to-input pitch ratio before and after the `0.25-8.0` clamp.
- `outputPeriodToWindowRatio`: `(sampleRate / targetHz) / grainWindowSamples` for the representative voice; values approaching or exceeding `1.0` indicate the output period no longer fits inside the grain window.
- `ratioClampHitCount`: cumulative count of active-voice pitch ratios that would fall outside `0.25-8.0` before clamping. Resets on Panic and on engine reset.
- `wetZeroCrossingHz` / `wetZeroCrossingCentsDeviation`: lightweight zero-crossing frequency estimate of the representative voice's wet output and its cents deviation from that voice's MIDI target. Accurate for the sine-wave self test; not reliable for real vocal input.

Character diagnostics:

- `characterAmountRaw`: APVTS Character Amount value.
- `characterAmountSmoothed`: processor-smoothed Character Amount value.
- `characterDeltaRms`: per-block RMS difference between clean shifted harmony and Character-processed harmony.
- `characterDeltaPeak`: per-block peak difference between clean shifted harmony and Character-processed harmony.
- `characterDeltaRatioDb`: dB ratio between Character delta RMS and Character input RMS.

## Pitch Detector

- Algorithm: YIN-style difference function with CMNDF.
- Frame length: `2048 samples`.
- Hop size: `512 samples`.
- Intended detection range: approximately `70-900 Hz`. Note: the coded `minFrequencyHz` constant is actually `80 Hz`, not `70 Hz`; this discrepancy is unresolved as of 0.3.0 and affects interpretation of low-frequency self-test results near 70-80 Hz.
- Uses RMS and confidence checks for voiced/unvoiced decisions.
- Low-RMS or low-confidence input becomes invalid rather than blindly reusing raw pitch.
- A short hold is used to keep display and ratio input stable through brief dropouts.
- Harmonic correction can test likely octave/fifth-family candidates such as raw/2, raw/3, raw*2, and raw*3.
- Corrected pitch enters smoothing/median-style stabilization before being exposed for display and harmony ratio use.
- Debug self-test (`runPitchDetectorSelfTest`) runs two passes: the original 12-frequency pass (80-880 Hz) with harmonic correction OFF, and a low-frequency pass (`50/60/65/70/80/90/100/110/130 Hz`) with harmonic correction ON to observe correction activation near the detection floor.

## Harmony DSP

Input source selection:

- The selected mono input is used consistently for dry path, wet choir render input, pitch detector input, and input meter.
- Input Gain is applied before these shared downstream paths.
- Standalone Auto compares current block peaks of ch0 and ch1 and uses the louder channel when both exist.
- Standalone Mix 1+2 uses `0.5 * (ch0 + ch1)` when both channels exist.
- VST3 always uses ch0/L.

Wet rendering:

- Delay buffer input is mono.
- Wet output is stereo.
- Active MIDI notes determine target notes.
- `pitchRatio = targetMidiHz / correctionInputPitchHz`.
- If no valid correction input pitch is available, pitch ratio falls back to `1.0`.
- Pitch ratio is clamped to `0.25-8.0`.

Pitch shifter:

- Current implementation is a lightweight delay-window pitch shifter.
- Each voice uses two crossfaded phase windows.
- Window function is Hann-like.
- Fixed fallback window is approximately `18 ms`.
- Input-synced window is used when a valid smoothed `windowPitchHz` is available.
- Input-synced target window is approximately `6` input pitch cycles, clamped to `256-4096 samples`.
- `correctionInputPitchHz` is used for pitch ratio; `windowPitchHz` is used only for grain/window length calculation.
- Active grain window lengths are not changed mid-grain.
- New window lengths are adopted at grain boundaries.
- Per-grain window-length changes are clamped to reduce clicks.
- Minimum delay is approximately `4 ms`, clamped to `32-1024 samples`.
- No empirical high-frequency or ratio correction coefficient is applied.

Ratio smoothing and de-clicking:

- MIDI target ratio transitions use log-domain smoothing.
- Minimum transition time is approximately `8 ms` when Glide is effectively off.
- Glide can extend transition time up to approximately `500 ms`.
- Existing phase/window state is preserved on note changes where possible.

Voice envelope:

- Wet voices use lightweight attack/release envelopes.
- Attack time: approximately `8 ms`.
- Release time: approximately `12 ms`.
- Released voices continue rendering until the envelope falls below the internal cutoff.

Voice gain:

- Base wet voice gain is `0.45`.
- Gain is not divided by active voice count.
- Total wet level can rise with voice count and is controlled by Output.

Character:

- Character Amount `0%` is clean-equivalent for all Character Types.
- Warm: high shelf cut around `4200 Hz`, low-mid boost around `350 Hz`, and very light soft saturation.
- Bright: high shelf boost around `5500 Hz`, presence boost around `3200 Hz`, and low-mid cleanup around `350 Hz`.
- Vowel: slot-dependent formant-like peaking EQ centers around `750`, `950`, `1050`, `1350`, `1500`, `1700`, `2100`, and `2500 Hz`.
- Digital: high shelf boost around `4500 Hz`, presence boost around `2400 Hz`, and light soft saturation.
- Per-slot pitch detune, gain variation, and delay variation are Amount-scaled.
- Character applies to harmony voices only; it does not process the original dry path or tuned lead path.

Spread:

- Multiple active voices are distributed linearly from left to right and scaled by Spread.
- A single active voice is centered.

## GUI

Window:

- Size: `860 x 540`.
- Single-screen, dark, live-oriented layout.
- Timer refresh rate: `30 Hz`.

Header:

- Logo/version area.
- Input Gain and Output sliders with editable dB value labels.
- Auto Tune toggle.
- Input Source dropdown.
- Mono Out toggle.
- High Quality toggle (engine selector). Its label text ends exactly at the left edge of the `Input` label text.
- PANIC button. The Input Source box and PANIC form a right-hand column and share the same size.
- The gain sliders and the three toggles sit left of that column as one block.

Harmony:

- Voices and Glide are stacked dropdowns in a single column (Voices `1`-`8`; Glide `None` = 0%, `Weak` = 15%, `Strong` = 50%). `glide` remains a continuous `0.0-1.0` parameter, so host automation can set intermediate values; the dropdown then displays the nearest entry without writing back.
- Character Type and Amount.
- Spread.
- Dry/Wet.
- Character Amount, Spread, and Dry/Wet support direct numeric entry.
- All knobs and the header sliders reset to their parameter default on double-click.
- All controls have hover tooltips.
- There is no Tune/Retune knob by design; the `tune` (Retune) parameter is host-automatable only and defaults to full hard-tune.

MIDI:

- Active MIDI note names.
- Display-only mini keyboard covering C2-C6. In Release builds the keyboard fills the remaining height of the MIDI card.
- Debug builds only: MIDI delivery counters below the keyboard (VST3 routing diagnostics) and the D1 pitch readout above it. Release builds show neither.

Level:

- Vertical input meter and connected stereo Output L/R meters. Output meters show post-output-gain values; with Mono Out enabled both show the mono result after smoothing.
- Meter ballistics (message thread, driven by the 30 Hz editor timer): instant attack, ~250 ms release, plus a peak-hold marker held for 1.5 s and then decaying.
- Bar colour comes from a fixed level-mapped gradient over the meter's full `-60..0 dB` range (green up to about -15 dB, amber around -8 dB, red at 0 dB), so a given height always has the same colour.
- Clipping latches: the meter draws a red cap, a red outline, and shows `CLIP` instead of the dB value. Clicking any meter clears the latched clip flags (PANIC also clears them).

Debug display:

- Debug subtitle uses `VoxChord v<version> | Build: <diagnostic-build-id>` when a diagnostic build identifier is set (currently `lead-retune-001`).
- Release subtitle uses `VoxChord v<version>`.
- Pitch self-test summary and detailed pitch runtime fields are hidden by default.
- Character and pitch diagnostic code remains available for future debugging.
- D1 low-pitch diagnostics (`pitchDebugLabel`, Debug-only) are shown in the band above the mini keyboard in the MIDI area, at a legible font. The readout front-loads the key shifter indicators — pitch ratio raw/clamped, output-period-to-window ratio (Per/Win), and wet zero-crossing frequency/cents deviation — followed by representative voice note, grain window length, and ratio clamp hit count. In Release builds this band is empty (the readout is not shown and takes no MIDI counters space).

## Real-Time Safety Rules

- Do not allocate memory inside `processBlock()` for normal audio work.
- Do not perform file I/O inside `processBlock()`.
- Do not lock from the audio thread.
- GUI must read audio-thread data through atomics or state snapshots.
- Heavy/debug self tests must not run inside the normal audio callback.
- Any future DSP feature that adds latency must explicitly update this spec and the processor latency reporting.

## Known Current Limitations

- Pitch detection quality remains experimental for real vocal material.
- `Retune` (`tune` parameter) is functional (Lead Tune snap speed) and defaults to `1.00`; by design it has no on-screen knob and is reachable via host automation only.
- Debug MIDI counters and the D1 pitch readout are Debug-only; Release builds show neither.
- Pitch shifter is lightweight and low-latency oriented, not high-quality offline pitch shifting.
- Standalone device settings persistence has not been implemented as a VoxChord-specific feature.
- No internal reverb, delay, preset browser, scale/chord detection, or AI voice conversion is implemented.

## Maintenance Rule

When editing source code:

1. Update the relevant implementation in `Source/`.
2. Update this `SPEC.md` if file structure, class responsibility, parameter behavior, DSP behavior, GUI behavior, or known limitations changed.
3. Update `TESTLOG.md` when user-confirmed behavior or test status changes.
4. Commit code and matching spec/log updates together unless the user explicitly says not to commit.
5. Push only when requested or when following the established project workflow.
