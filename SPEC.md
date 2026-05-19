# VoxChord Source Specification

Last updated: 2026-05-19
Project version: 0.1.23

## 0.1.23 Update - priority A live usability pass

- CMake project version: `0.1.23`.
- Debug GUI pitch subtitle build string: `Build: priority-a-001`.
- Release GUI pitch subtitle now shows only `VoxChord v` plus `JucePlugin_VersionString`.
- Debug GUI pitch subtitle keeps the detailed self-test / pitch / RMS / confidence display.
- Added APVTS parameter `inputGainDb`.
- Input Gain range is `-24.0 dB` to `+24.0 dB`, default `0.0 dB`, step `0.1 dB`.
- Input Gain is applied after Input Source selection and before Pitch Detector, Harmony render input, dry path, and input metering.
- Input Gain uses a `20 ms` smoothed gain value.
- Wet voice gain now uses constant voice-level style mixing with `baseVoiceGain = 0.45`, instead of dividing each voice by active voice count.
- MIDI voice stealing now chooses the active voice whose current MIDI note is closest to the incoming note, with oldest age as the tie breaker.
- Existing input-synced window, MIDI de-click envelope, and ratio smoothing behavior remain in place.

## 0.1.22 Update - MIDI note transition de-click

- CMake project version: `0.1.22`.
- GUI pitch debug build string: `Build: midi-declick-001`.
- MIDI note transitions now use short de-click smoothing while preserving the input-synced window pitch shifter.
- Per-voice envelope state was added: `envelopeGain`, `targetEnvelopeGain`, `leftGain`, `rightGain`, `monoGain`, and `delayOffsetSamples`.
- New active voices fade in with `voiceAttackSeconds = 0.008`.
- Released voices remain rendered until their envelope fades out with `voiceReleaseSeconds = 0.012`, instead of being removed from the wet render immediately.
- `targetRatio` changes now use log-domain smoothing via `noteTransitionRatioSmoothingSeconds = 0.008` when Glide is effectively off.
- Existing active voice note changes keep phase/window state rather than resetting `phaseA`, `phaseB`, or active grain window lengths.
- Active input-synced grains still keep their own `windowSamplesA/B` until phase wrap.
- No empirical ratio correction is applied.

## 0.1.21 Update - click-safe input-synced window continuity

- CMake project version: `0.1.21`.
- GUI pitch debug build string: `Build: input-synced-window-continuity-001`.
- `correctionInputPitchHz` remains the pitch used for MIDI target ratio calculation.
- A separate `windowPitchHz` is now used for input-synced window length calculation.
- `windowPitchHz` is smoothed more slowly than `correctionInputPitchHz` using `windowPitchSmoothingSeconds = 0.15`.
- Each voice now stores separate `windowSamplesA` and `windowSamplesB`.
- `renderPitchShiftedSample()` now calculates `delayA = baseDelay + phaseA * windowSamplesA` and `delayB = baseDelay + phaseB * windowSamplesB`.
- New target window length is not applied immediately to an active read window.
- `windowSamplesA` and `windowSamplesB` are updated only when their own phase wraps, so a grain keeps a constant window length while it is playing.
- Each grain-boundary update is limited by `maxWindowChangeRatioPerGrain = 1.25` and `maxWindowChangeSamplesPerGrain = 512`.
- No empirical ratio correction is applied.

## 0.1.20 Update - separated pitch shifter summaries and default-candidate input-sync

- CMake project version: `0.1.20`.
- GUI pitch debug build string: `Build: input-synced-window-002`.
- `PitchShifterSelfTestSummary` now stores separate `fixedWindow` and `inputSyncedWindow` summaries.
- The GUI debug subtitle reports fixed-window and input-synced-window self test status independently.
- Input-synced window mode is now the render-path default candidate, controlled by `useInputSyncedPitchWindowByDefault`.
- Window tuning constants are grouped in `SimpleChoirEngine`: `fixedPitchWindowSeconds`, `inputSyncedPitchWindowCycles`, `inputSyncedMinWindowSamples`, and `inputSyncedMaxWindowSamples`.
- Input-synced self test coverage now spans input `100/150/220/330/440/660/880 Hz` with ratios `0.5/0.75/1.0/1.5/2.0`.
- Fixed-window representative tests remain for regression comparison.
- No empirical ratio correction is applied.

## 0.1.19 Update - input-synced pitch window prototype

- CMake project version: `0.1.19`.
- GUI pitch debug build string: `Build: input-synced-window-001`.
- `SimpleChoirEngine` now computes an experimental input-synced pitch shifter window length from the current ratio input pitch:
- `periodSamples = sampleRate / correctionInputPitchHz`
- `windowSamples = clamp(round(6.0 * periodSamples), 256, 4096)`
- If `correctionInputPitchHz <= 0`, the window falls back to the existing fixed `18 ms` length clamped to `256-4096`.
- `renderPitchShiftedSample()` now receives `windowSamples` explicitly; the ratio formula remains `phaseDelta = (1 - ratio) / windowSamples`.
- The implementation does not add any empirical ratio correction.
- The delay buffer allocation now reserves enough space for the maximum window length, not only the default fixed 18 ms window.
- `PitchShifterSelfTest` now includes additional input-synced window cases for `440/660/880 Hz * 0.5` and `440 Hz * 2.0`, while keeping the fixed-window cases for comparison.
- Self test DBG output now includes `windowMode`, active `pitchWindowSamples`, fixed `fixedPitchWindowSamples`, `inputPeriodSamples`, and `inputSyncedWindowCycles`.

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
- Debug ビルドでは processor 生成時に `SimpleChoirEngine::runPitchDetectorSelfTest()` と `SimpleChoirEngine::runPitchShifterSelfTest()` を一度実行する。
- GUI 共有用の MIDI / pitch / meter 状態は atomic または専用 state 経由で公開する。

`Source/PluginEditor.h`, `Source/PluginEditor.cpp`

- 1 画面のライブ向け GUI。
- 7 sliders: Voice Count, Tune, Glide, Character, Spread, Dry/Wet, Output。
- Input Source selector: Auto, Input 1, Input 2, Mix 1+2。
- MIDI note indicator、voice slot 表示、last MIDI event、pitch debug、input/output meter、PANIC button を持つ。
- Timer は `30 Hz`。
- pitch debug subtitle の現在の Debug build string は `Build: priority-a-001`。
- Release build subtitle shows only `VoxChord v` plus the plugin version.
- Debug builds show a pitch shifter self test summary in the GUI debug subtitle.
- pitch debug は `Raw`, `Corr`, `Disp`, `RatioIn`, `Conf`, `Voiced`, `Fix`, `RatioSmooth` を表示する。

`Source/SimpleChoirEngine.h`, `Source/SimpleChoirEngine.cpp`

- VoxChord の wet choir DSP 本体。
- dry input を mono 化して delay buffer に書き込み、最大 4 voice の pitch-shifted wet signal を stereo 出力する。
- pitch detection、pitch state tracking、per-voice pitch ratio、character detune/gain/delay、spread pan を扱う。
- 新しい 2 段 pitch shift は実装していない。既存の 1 段 pitch shift の `pitchRatio` 入力を改善する方針。

`Source/VoxChordParameters.h`, `Source/VoxChordParameters.cpp`

- APVTS parameter layout を定義する。
- Parameter ID は後方互換のため原則変更しない。
- 現在の parameter IDs: `voiceCount`, `tune`, `glide`, `character`, `spread`, `dryWet`, `outputLevel`, `inputSource`。

## Build Configuration

- CMake project version: `0.1.23`
- Plugin formats: `VST3`, `Standalone`
- JUCE path: `../JUCE`
- Linked JUCE module: `juce::juce_audio_utils`
- Compile definitions:
- `JUCE_ASIO=1`
- `JUCE_WEB_BROWSER=0`
- `JUCE_USE_CURL=0`
- `JUCE_VST3_CAN_REPLACE_VST2=0`

## Audio Processor Specification

Input/output:

- Audio input: stereo default, mono/stereo layouts are allowed.
- Audio output: stereo primary. Mono/stereo output layouts are allowed.
- MIDI input: required.
- MIDI output: disabled.
- Standalone input source can select Auto, Input 1, Input 2, or Mix 1+2.
- Input Gain is applied after input source selection.
- VST3 always uses ch0/L as the vocal input and does not expose physical input channel routing.

Audio block flow:

1. Apply pending Panic request.
2. Parse MIDI messages and update `MidiVoiceState`.
3. Select mono vocal input and copy it into stereo `dryBuffer`.
4. Apply smoothed Input Gain to the selected mono input.
5. Render wet choir into `wetBuffer` using `SimpleChoirEngine`.
6. Publish pitch debug fields to atomics.
7. Mix dry/wet into host output buffer.
8. Apply smoothed output gain.
9. Publish input/output meters.

Dry/wet and output:

- `Input Gain` is smoothed over `0.02 sec`.
- `Dry/Wet` is smoothed over `0.02 sec`.
- `Output` gain is smoothed over `0.02 sec`.
- Input Gain parameter range is `-24.0 dB` to `+24.0 dB`, default `0.0 dB`.
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

`inputGainDb`

- Type: float dB
- Range: `-24.0-24.0 dB`
- Default: `0.0 dB`
- Smoothed input gain after input source selection.
- Affects Pitch Detector, Harmony render input, dry path, and input meter consistently.

`inputSource`

- Type: choice
- Choices: `Auto`, `Input 1`, `Input 2`, `Mix 1+2`
- Default: `Auto`
- Intended mainly for Standalone use.
- In Standalone, it controls which physical input channel becomes VoxChord's mono vocal input.
- In VST3, it is ignored by DSP; DAW routing is respected and ch0/L is used.

## MIDI Voice Specification

- Maximum voices: `4`
- Active notes are exposed as a 4-slot snapshot.
- Slot allocation favors empty slots, then replaces oldest active voice.
- When all slots are occupied, voice stealing favors the active voice whose current MIDI note is nearest to the incoming note; oldest age is used as a tie breaker.
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

Pitch shifter self test:

- Debug only, run once from processor constructor after pitch detector self test.
- Does not use PitchDetector or MIDI voice allocation.
- Uses an internal sine wave, one `VoicePitchState`, Character=0 equivalent, delay offset `0`, and `glideCoefficient=1.0f`.
- Ratio smoothing and glide are fully bypassed by setting `currentPitchRatio` and `targetPitchRatio` to the fixed ratio and calling `renderPitchShiftedSample()` with glide coefficient `1.0f`.
- Measures output frequency from positive-going zero crossings after initial transient skip.
- Stores separate fixed-window and input-synced-window summaries in `PitchShifterSelfTestSummary`.
- GUI debug subtitle displays fixed-window and input-synced-window `Pitch Shifter SelfTest` summaries independently, including PASS/FAIL, max error cents, worst input Hz, worst ratio, and worst measured Hz.
- GUI debug subtitle also displays the worst measured actual ratio.
- If a self test mode has not run, GUI displays `Fixed: NOT RUN` or `InputSync: NOT RUN`.
- Test cases:
- `440 Hz * 1.0 -> 440 Hz`
- `660 Hz * 1.0 -> 660 Hz`
- `880 Hz * 1.0 -> 880 Hz`
- `220 Hz * 2.0 -> 440 Hz`
- `440 Hz * 2.0 -> 880 Hz`
- `440 Hz * 1.5 -> 660 Hz`
- `440 Hz * 0.5 -> 220 Hz`
- `660 Hz * 0.5 -> 330 Hz`
- `880 Hz * 0.5 -> 440 Hz`
- Input-synced window coverage:
- inputs `100/150/220/330/440/660/880 Hz`
- ratios `0.5/0.75/1.0/1.5/2.0`
- Debug output reports expected frequency, measured frequency, error cents, +/-10 cents result, `pitchWindowSamples`, `minimumDelaySamples`, and whether ratio smoothing/glide were disabled.
- Debug output also reports `windowMode`, `fixedPitchWindowSamples`, `inputPeriodSamples`, and `inputSyncedWindowCycles`.
- Debug output also reports actual ratio, actual/target ratio, `phaseDelta`, delay step per sample, and theoretical read speed.
- Debug output also reports measured delay step A/B, measured read step A/B, phase wrap count A/B, and actual wrap interval samples A/B.
- For selected cases, debug output reports spectral diagnostics: top 5 spectral peaks, expected frequency bin magnitude, measured frequency bin magnitude, and the peak/frequency source used for the measured result.
- Spectral diagnostics currently run for `440 Hz * 0.5`, `660 Hz * 0.5`, `880 Hz * 0.5`, and `440 Hz * 2.0`.
- Spectral analysis uses a Hann-windowed Goertzel scan over approximately `20-4000 Hz` on the tail of the self test output.
- The phase/delay model under investigation is `phaseDelta = (1 - ratio) / pitchWindowSamples`, `delay = baseDelay + phase * pitchWindowSamples`, and `readPosition = writePosition - delay`; therefore `delayStep = 1 - ratio` and theoretical read speed is `ratio`.
- Phase is updated after delay calculation and delay-line reads in `renderPitchShiftedSample()`.
- `readDelayLine()` uses `readPosition = writeIndex - delaySamples`, wraps it into the circular delay buffer, then uses `floor(readPosition)` and `index + 1` linear interpolation.

## Harmony DSP Specification

Input source selection:

- The selected mono input is used consistently for dry path, wet choir render input, pitch detector input, and input meter.
- Input Gain is applied to the selected mono input before these shared downstream paths.
- Standalone `Input 1`: use input channel 0.
- Standalone `Input 2`: use input channel 1 when present, otherwise fall back to channel 0.
- Standalone `Mix 1+2`: use `0.5 * (ch0 + ch1)` when channel 1 is present, otherwise channel 0.
- Standalone `Auto`: compare current block peak of ch0 and ch1 and use the louder channel; if ch1 is missing, use ch0.
- VST3: always use ch0/L, regardless of `inputSource`.

Wet rendering:

- Input is read from the selected mono vocal input.
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
- Input-synced pitch window is the current default candidate when a valid smoothed `windowPitchHz` is present: `clamp(round(6.0 * sampleRate / windowPitchHz), 256, 4096)`.
- `correctionInputPitchHz` is still used directly for `pitchRatio = targetMidiHz / correctionInputPitchHz`; `windowPitchHz` is only for grain/window length calculation.
- `windowPitchHz` uses slower log-frequency smoothing than the harmony ratio input to avoid abrupt window length changes.
- Each voice stores `windowSamplesA` and `windowSamplesB`; active grains do not change their window length mid-grain.
- New target window lengths are adopted only when each corresponding phase wraps.
- Per-wrap window length changes are limited by ratio and sample-count clamps to reduce read-position discontinuities.
- `useInputSyncedPitchWindowByDefault` can switch the render path back to fixed-window behavior for comparison.
- Adjustable constants: `fixedPitchWindowSeconds = 0.018`, `inputSyncedPitchWindowCycles = 6.0`, `inputSyncedMinWindowSamples = 256`, `inputSyncedMaxWindowSamples = 4096`, `windowPitchSmoothingSeconds = 0.15`, `maxWindowChangeRatioPerGrain = 1.25`, `maxWindowChangeSamplesPerGrain = 512`.
- If no valid pitch is available, the active pitch window falls back to the fixed `18 ms` value.
- `minimumDelaySamples` is approximately `4 ms`, limited to `32-1024`.
- No empirical ratio correction is applied.
- No second auto-tune pitch shifter is present.

Ratio smoothing:

- MIDI target ratio transitions use log-domain smoothing.
- If Glide is effectively off, the transition time is approximately `8 ms`.
- If Glide is active, the ratio coefficient uses the slower of the MIDI de-click coefficient and the Glide coefficient.
- The purpose is to reduce note-change clicks while avoiding excessive lag in input-pitch tracking.

Voice envelope:

- Each wet voice has a lightweight attack/release envelope.
- Attack time: approximately `8 ms`.
- Release time: approximately `12 ms`.
- Note Off and voice-limit release do not remove the DSP voice immediately; the voice remains rendered until its envelope is below `0.0001`.
- Note changes on an existing slot do not reset phase/window state.

Voice gain:

- Current wet voice gain uses constant voice-level style mixing.
- Per voice base gain is `baseVoiceGain = 0.45`.
- Per-voice gain is not divided by active voice count.
- Total wet level can rise as voice count increases; final level remains controllable via Output.

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
- Debug: `Build: priority-a-001`
- Release: `VoxChord v<version>`
- `Pitch Shifter SelfTest: PASS/FAIL`
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
