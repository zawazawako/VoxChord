# VoxChord Source Specification

Last updated: 2026-05-18
Project version: 0.1.12

このファイルは `Source/` 以下のファイル構造と実装仕様を記録する。今後、ソースコードを編集した場合は、git commit とあわせてこの `SPEC.md` に変更内容を反映する。

`TESTLOG.md` はユーザー確認結果とテスト履歴、`SPEC.md` は現在の実装仕様を記録する。

## Source File Structure

`Source/LevelMeterState.h`

- 入出力ピークとクリップ状態を `std::atomic` で保持する。
- audio thread から `publish()` でピーク値を公開し、GUI thread から getter で読む。
- clip threshold は `0.999f`。

`Source/MidiVoiceState.h`, `Source/MidiVoiceState.cpp`

- MIDI note を最大 4 voice の slot に割り当てる。
- `NoteSnapshot` は `std::array<int, 4>` で、inactive slot は `-1`。
- Note On は既存同一 note を更新し、空き slot 優先、空きがなければ最古 voice を置換する。
- Note Off は該当 note の voice を解放する。
- All Notes Off / All Sound Off / Reset All Controllers は全 voice を reset する。
- `enforceVoiceLimit()` により Voice Count より上の slot は強制解放する。

`Source/PluginProcessor.h`, `Source/PluginProcessor.cpp`

- JUCE `AudioProcessor` 本体。
- input bus は mono、output bus は stereo を基本とし、bus layout として mono/stereo を許可する。
- MIDI input は有効、MIDI output は無効。
- APVTS で MVP パラメータを保持する。
- `processBlock()` の主な流れは、panic 処理、MIDI 処理、dry buffer コピー、wet choir render、pitch debug state publish、dry/wet mix、meter publish。
- `setLatencySamples(0)` を設定している。
- Debug ビルドでは processor 生成時に `SimpleChoirEngine::runPitchDetectorSelfTest()` を一度実行する。
- GUI 共有用の MIDI / pitch / meter 状態は atomic または専用 state 経由で公開する。

`Source/PluginEditor.h`, `Source/PluginEditor.cpp`

- 1 画面のライブ向け GUI。
- 7 sliders: Voice Count, Tune, Glide, Character, Spread, Dry/Wet, Output。
- MIDI note indicator、voice slot 表示、last MIDI event、pitch debug、input/output meter、PANIC button を持つ。
- Timer は `30 Hz`。
- pitch debug subtitle の現在の build string は `Build: hard-tune-tracking-001`。
- pitch debug は `Raw`, `Corr`, `Disp`, `RatioIn`, `Conf`, `Voiced`, `Fix`, `RatioSmooth` を表示する。

`Source/SimpleChoirEngine.h`, `Source/SimpleChoirEngine.cpp`

- VoxChord の wet choir DSP 本体。
- dry input を mono 化して delay buffer に書き込み、最大 4 voice の pitch-shifted wet signal を stereo 出力する。
- pitch detection、pitch state tracking、per-voice pitch ratio、character detune/gain/delay、spread pan を扱う。
- 新しい 2 段 pitch shift は実装していない。既存の 1 段 pitch shift の `pitchRatio` 入力を改善する方針。

`Source/VoxChordParameters.h`, `Source/VoxChordParameters.cpp`

- APVTS parameter layout を定義する。
- Parameter ID は後方互換のため原則変更しない。
- 現在の parameter IDs: `voiceCount`, `tune`, `glide`, `character`, `spread`, `dryWet`, `outputLevel`。

## Build Configuration

- CMake project version: `0.1.12`
- Plugin formats: `VST3`, `Standalone`
- JUCE path: `../JUCE`
- Linked JUCE module: `juce::juce_audio_utils`
- Compile definitions:
- `JUCE_WEB_BROWSER=0`
- `JUCE_USE_CURL=0`
- `JUCE_VST3_CAN_REPLACE_VST2=0`

## Audio Processor Specification

Input/output:

- Audio input: mono primary. Stereo input layouts are allowed and copied as stereo dry buffer when present.
- Audio output: stereo primary. Mono/stereo output layouts are allowed.
- MIDI input: required.
- MIDI output: disabled.

Audio block flow:

1. Apply pending Panic request.
2. Parse MIDI messages and update `MidiVoiceState`.
3. Copy host input into internal `dryBuffer`.
4. Render wet choir into `wetBuffer` using `SimpleChoirEngine`.
5. Publish pitch debug fields to atomics.
6. Mix dry/wet into host output buffer.
7. Apply smoothed output gain.
8. Publish input/output meters.

Dry/wet and output:

- `Dry/Wet` is smoothed over `0.02 sec`.
- `Output` gain is smoothed over `0.02 sec`.
- Output parameter range is `-24.0 dB` to `+6.0 dB`, default `-3.0 dB`.

## Parameters

`voiceCount`

- Type: integer
- Range: `1-4`
- Default: `4`
- Controls active MIDI voice slot limit.

`tune`

- Type: float percent
- Range: `0.0-1.0`
- Default: `0.80`
- Current DSP behavior: UI and APVTS parameter remain, but hard tune tracking currently ignores this value internally.

`glide`

- Type: float percent
- Range: `0.0-1.0`
- Default: `0.15`
- Controls MIDI target ratio glide via `getGlideCoefficient()`.

`character`

- Type: float percent
- Range: `0.0-1.0`
- Default: `0.35`
- Controls per-voice detune, gain offset, and delay offset.

`spread`

- Type: float percent
- Range: `0.0-1.0`
- Default: `0.55`
- Controls stereo pan spread across active wet voices.

`dryWet`

- Type: float percent
- Range: `0.0-1.0`
- Default: `0.50`
- Mixes dry input and wet choir output.

`outputLevel`

- Type: float dB
- Range: `-24.0-6.0 dB`
- Default: `-3.0 dB`
- Smoothed final output gain.

## MIDI Voice Specification

- Maximum voices: `4`
- Active notes are exposed as a 4-slot snapshot.
- Slot allocation favors empty slots, then replaces oldest active voice.
- Repeated Note On for the same MIDI note updates velocity/frequency and age.
- Panic button requests audio-thread reset via atomic flag.
- All Notes Off / All Sound Off / Reset All Controllers reset MIDI voices.

## Pitch State Specification

`PitchState` fields:

- `inputRmsDb`: frame RMS in dBFS.
- `rawPitchHz`: direct pitch detector result.
- `correctedPitchHz`: harmonic-corrected pitch result.
- `displayStablePitchHz`: GUI/debug stable pitch. Smoothness and readability are prioritized.
- `correctionInputPitchHz`: pitch used for harmony ratio calculation.
- `stablePitchHz`: compatibility alias for display stable pitch.
- `harmonyPitchHz`: compatibility alias for correction input pitch.
- `ratioSmoothingCoefficient`: current per-voice pitch ratio smoothing coefficient.
- `confidence`: pitch confidence, currently `1.0 - CMNDF value`.
- `voiced`: voiced/unvoiced state after RMS/confidence filtering and hold handling.
- `harmonicCorrectionMode`: `0` none, `2` raw/2, `3` raw/3, `-2` raw*2, `-3` raw*3.

Important separation:

- GUI/debug should use `displayStablePitchHz` when a smooth visual pitch is desired.
- Harmony ratio must use `correctionInputPitchHz`.
- `stablePitchHz` and `harmonyPitchHz` remain for compatibility but should not be treated as the primary conceptual names in new code.

## Pitch Detector Specification

Algorithm:

- YIN-style difference function and CMNDF.
- Frame length: `2048 samples`
- Hop size: `512 samples`
- Detection range: `80-900 Hz`
- RMS gate: `-45.0 dBFS`
- Confidence threshold: `0.75`
- Very high confidence threshold: `0.9`
- Hold time: `100 ms`

Lag range:

- `minLag = floor(sampleRate / maxFrequencyHz)`
- `maxLag = ceil(sampleRate / minFrequencyHz)`

YIN selection:

- First threshold crossing is followed to a local minimum.
- If no threshold crossing exists, fallback chooses a best lag with tolerance that avoids unnecessary octave-down selection.

Harmonic correction:

- Internal flag `harmonicCorrectionEnabled`, default `true`.
- Candidates: raw, raw/2, raw/3, raw*2, raw*3.
- Correction is only accepted when close enough to `displayStablePitchHz`.
- Correction candidate must repeat for `2` frames.
- If high-confidence raw pitch remains stable for `3` frames, raw can unlock from an over-corrected stable pitch.

Median/stability:

- Corrected pitch enters a log-frequency median filter.
- Median window size: `5`
- Display stable smoothing alpha: `0.2`
- Jump rejection threshold: `350 cents`

Self test:

- Debug only, run once from processor constructor.
- Test frequencies: `100`, `150`, `220`, `261.63`, `329.63`, `440`, `523.25`, `600`, `659.25`, `700`, `800`, `880 Hz`.
- Harmonic correction is OFF during self test.
- Self test must not run inside normal real-time `processBlock()`.

## Harmony DSP Specification

Wet rendering:

- Input is read as mono. Stereo input is averaged for DSP pitch analysis/render input.
- Delay buffer is mono.
- Wet output is stereo.
- Active MIDI notes determine target notes.
- `pitchRatio = targetMidiHz / correctionInputPitchHz`
- If `correctionInputPitchHz <= 0.0f`, pitch ratio returns `1.0f`.
- Pitch ratio is limited to `0.25-8.0`.

Pitch shifter:

- Current implementation is a lightweight delay-window pitch shifter.
- Two phase windows per voice: `phaseA`, `phaseB`.
- Window function: Hann-like `0.5 - 0.5*cos(2*pi*phase)`.
- `pitchWindowSamples` is approximately `18 ms`, limited to `256-4096`.
- `minimumDelaySamples` is approximately `4 ms`, limited to `32-1024`.
- No second auto-tune pitch shifter is present.

Ratio smoothing:

- Current minimum ratio smoothing alpha: `0.35`.
- If Glide is active, ratio coefficient is `min(0.35, glideCoefficient)`.
- The purpose is to reduce zipper noise while avoiding excessive lag in input-pitch tracking.

Glide:

- MIDI target pitch glide is distinct from input pitch tracking.
- Glide coefficient uses a time range from approximately `5 ms` to `500 ms`.

Tune:

- Current DSP treats Tune as unused/hard-tune fixed.
- The parameter remains in UI/APVTS for compatibility and future reinterpretation as Tightness.

Character:

- Per-slot detune cents: `[-14, 10, -9, 18] * character`
- Per-slot gain offset: `[-0.07, 0.05, -0.04, 0.06] * character`
- Per-slot delay offset ms: `[0, 4, 8, 12] * character`

Spread:

- For multiple active voices, pan positions are distributed linearly from left to right and scaled by Spread.
- Single active voice is centered.

## GUI Specification

Window:

- Size: `800 x 470`
- Timer: `30 Hz`
- Dark, single-screen live-oriented UI.

Controls:

- Voice Count
- Tune
- Glide
- Character
- Spread
- Dry/Wet
- Output
- PANIC button

Status/debug:

- MIDI notes and pitch summary.
- Voice slots.
- Last MIDI event.
- Input and output meters.
- Pitch debug subtitle currently includes:
- `Build: hard-tune-tracking-001`
- `RMS`
- `Raw`
- `Corr`
- `Disp`
- `RatioIn`
- `Conf`
- `Voiced`
- `Fix`
- `RatioSmooth`

## Real-Time Safety Rules

- Do not allocate memory inside `processBlock()` for normal audio work.
- Do not perform file I/O inside `processBlock()`.
- Do not lock from audio thread.
- GUI must read audio-thread data through atomic/state snapshots.
- Heavy/debug self tests must not run inside the normal audio callback.

## Known Current Limitations

- Build and runtime verification for version `0.1.12` are pending user confirmation.
- Pitch detection quality is still experimental for real vocal material.
- `Tune` parameter exists but is currently ignored by DSP.
- GUI debug subtitle is dense and may become visually cramped.
- Pitch shifter is lightweight and low-latency oriented, not high-quality offline pitch shifting.
- No standalone device settings persistence has been implemented yet.

## Maintenance Rule

When editing source code:

1. Update the relevant implementation in `Source/`.
2. Update this `SPEC.md` if the file structure, class responsibility, parameter behavior, DSP behavior, GUI behavior, or known limitations changed.
3. Update `TESTLOG.md` when user-confirmed behavior or test status changes.
4. Commit the code and matching spec/log updates together.
5. Push when requested or when following the established project workflow.
