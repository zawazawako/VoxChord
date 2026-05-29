# VoxChord Source Specification

Last updated: 2026-05-28
Project version: 0.1.40

## 0.1.40 Update - VST3 MIDI input diagnostics

- CMake project version: `0.1.40`.
- Debug GUI pitch subtitle build string: `Build: midi-input-debug-001`.
- `juce_add_plugin` continues to set `NEEDS_MIDI_INPUT TRUE`.
- `VoxChordAudioProcessor::acceptsMidi()` returns `true`; `producesMidi()` and `isMidiEffect()` remain `false`.
- MIDI handling still runs at the start of `processBlock()` before audio input copy, pitch detection, voiced/unvoiced checks, or choir rendering.
- Standalone/VST3 branching remains limited to audio input source selection; MIDI processing uses the same `handleMidi()` path for all wrappers.
- Added `MidiInputDebugSnapshot` with process block count, non-empty MIDI block count, total MIDI event count, and last block MIDI event count.
- `processBlock()` updates the MIDI input debug counters directly from `midiMessages.getNumEvents()` before note-specific handling.
- GUI MIDI debug row now shows `MIDI In: blocks <n> | last <n> | total <n> | nonempty <n>` so VST3 host delivery can be distinguished from internal note handling.
- No DSP pitch-shift behavior, Character behavior, APVTS parameter IDs, or MIDI voice allocation behavior was changed.

## 0.1.39 Update - GUI balance micro-adjustment

- CMake project version: `0.1.39`.
- Debug GUI pitch subtitle build string: `Build: gui-balance-layout-001`.
- No DSP behavior or APVTS parameter behavior was changed.
- Character subcard title is now centered at the top of the Character area.
- Level meter group is horizontally centered within the Level panel so the Input meter and connected `OutL` / `OutR` pair feel more left/right balanced.

## 0.1.38 Update - GUI entry and live layout refinement

- CMake project version: `0.1.38`.
- Debug GUI pitch subtitle build string: `Build: gui-entry-layout-001`.
- No DSP behavior, APVTS parameter IDs, Character DSP, pitch shifter, Mono Out DSP, or mini keyboard behavior was changed.
- Header utility labels now use `Auto Tune` and `Mono Out`.
- Header utility right column keeps the `Input Source` dropdown and `PANIC` button short, with matching right edges.
- Direct numeric entry labels are now available for `Voices`, `Glide`, `Amount`, `Spread`, and `Dry/Wet`; `Input Gain` and `Output` retain their dB entry labels.
- `Voices` direct entry is parsed as an integer and clamped by the existing APVTS `voiceCount` range.
- `Glide`, `Amount`, `Spread`, and `Dry/Wet` direct entry parse percentage text with optional `%`.
- Character `Type` dropdown is made shorter vertically so the `Amount` rotary area can be larger.
- The compact MIDI `Notes` count in the top-right MIDI status row is hidden.
- Harmony height is reduced slightly so the MIDI and Level panels are taller.
- MIDI and Level panels are brought closer together; the MIDI panel extends farther right and the Level panel starts farther left.
- Level meters are narrower and taller; `OutL` and `OutR` are placed directly adjacent as a connected stereo pair.

## 0.1.37 Update - GUI micro layout adjustments

- CMake project version: `0.1.37`.
- Debug GUI pitch subtitle build string: `Build: gui-micro-layout-001`.
- No DSP behavior, APVTS parameter IDs, Character DSP, pitch shifter, Mono Out DSP, or mini keyboard behavior was changed.
- Header `UtilityArea` now arranges `Lead` and `Input Source` on the top row, with `Mono` and a wide `PANIC` button on the second row.
- `Lead Tune` and `Mono Out` are shortened visually to `Lead` and `Mono` to preserve spacing in the compact live header.
- `Input Source` remains visible in the upper-right utility area with a readable dropdown width.
- The Harmony Character subcard now draws its own `Character` title.
- Character mode is labeled `Type` instead of `Char Type`.
- Character amount remains labeled `Amount`, and its knob/card area is enlarged for easier live control.
- Level meter dB values are drawn below the meter fill area instead of over the meter, with slightly stronger text contrast.
- Non-logo GUI labels and section titles were increased slightly for readability without changing layout responsibilities.

## 0.1.36 Update - final GUI controls and direct numeric entry

- CMake project version: `0.1.36`.
- Debug GUI pitch subtitle build string: `Build: gui-final-controls-001`.
- No DSP behavior was changed.
- Header is now split into three explicit areas: `LogoArea`, `GainArea`, and `UtilityArea`.
- `LogoArea` shows the VoxChord title and version/debug subtitle.
- `GainArea` uses the center space to show larger horizontal `Input Gain` and `Output` sliders with editable value labels.
- `UtilityArea` contains `Input Source`, `Lead Tune`, `Mono Out`, and a larger horizontal `PANIC` button.
- `Character` is displayed as a subcard inside the Harmony panel, grouping Character Type and Amount as one visible function.
- `Character Amount` uses a larger control area and an editable percent value label.
- Direct numeric entry is supported for `Input Gain`, `Output`, and `Character Amount`.
- dB entry accepts numeric text with optional `dB`; percentage entry accepts `0-100` text with optional `%` and maps to APVTS `0.0-1.0`.
- Invalid numeric input is ignored and the display reverts to the current APVTS value.
- Out-of-range numeric input is clamped by the existing APVTS parameter range.
- Numeric edits write through the existing APVTS parameter IDs; no parameter IDs were changed.

## 0.1.35 Update - GUI responsibility and sizing correction

- CMake project version: `0.1.35`.
- Debug GUI pitch subtitle build string: `Build: gui-responsibility-fix-001`.
- No DSP behavior was changed.
- Header now focuses on logo/version, Input Gain, Output Gain, Input Source, Lead Tune, Mono Out, and a larger PANIC button.
- Pitch, Last MIDI event, and active note count were removed from the HeaderControlsArea and moved into the MIDI panel.
- MIDI panel status now shows `Pitch`, `Last`, and `Notes`.
- Level panel is now meter-focused; Input Gain and Output controls were removed from it.
- Input and Output meters were enlarged within the Level panel.
- Character display remains grouped, with a wider Character sub-area and a larger Amount control than the previous layout.
- The mini MIDI keyboard remains display-only; black-key drawing continues to use white-key-relative positioning.

## 0.1.34 Update - GUI layout correction

- CMake project version: `0.1.34`.
- Debug GUI pitch subtitle build string: `Build: gui-layout-fix-001`.
- No DSP behavior was changed.
- Header layout is now explicitly split into `LogoArea` and `HeaderControlsArea`.
- HeaderControlsArea contains Input Gain, Output, Input Source, Lead Tune, Mono Out, and PANIC.
- The previous right-column Input / Lead, Level / Output, and Status panel split was removed from the main layout.
- Harmony now uses the wide main panel instead of being squeezed by a right column.
- Bottom area is split into a wide MIDI panel and a right-side Level panel.
- Pitch, Last, and Notes are displayed in the MIDI panel.
- Input/Output meters are grouped in the bottom-right Level panel.
- Mini MIDI keyboard remains display-only and fixed to C2-C6.
- Mini MIDI keyboard black keys are now drawn only for C#, D#, F#, G#, and A#, positioned relative to the white-key layout instead of equal semitone spacing.
- Detailed Debug information remains present but is constrained to the lower MIDI/debug row so it does not cover the meters.

## 0.1.33 Update - GUI layout, Mono Out, and mini MIDI keyboard

- CMake project version: `0.1.33`.
- Debug GUI pitch subtitle build string: `Build: gui-mono-midi-001`.
- Added APVTS bool parameter `monoOutputEnabled` (`Mono Out`, default `false`).
- Mono Out is applied at the final output stage after Dry/Wet mix and Output gain using `0.5 * (left + right)`.
- Mono Out switching is smoothed over approximately `12 ms` to reduce switching clicks.
- Output meters now publish and display separate post-output-gain L/R peaks; when Mono Out is enabled, the displayed L/R values should match after the smoothing transition.
- `LevelMeterState` now stores input peak plus output-left and output-right peak/clip flags.
- GUI is organized into Header, Harmony, MIDI, and Level areas.
- Header is split into LogoArea and HeaderControlsArea.
- HeaderControlsArea contains Input Gain, Output, Input Source, Lead Tune, Mono Out, and PANIC.
- Pitch, Last, and Notes are displayed in the MIDI area.
- Character Type and Amount are visually grouped as a single Character control area.
- Input Source remains on the main GUI inside the Input / Lead area.
- Added a `Mono Out` toggle in the Level / Output area.
- Replaced horizontal meter bars with vertical meter components.
- Added a display-only mini MIDI keyboard covering C2-C6 (`MIDI 36-84`), highlighting active MIDI notes from the existing thread-safe snapshot.
- The mini keyboard does not generate MIDI and does not change voice assignment.
- Physical output channel selection, output 3/4 routing, keyboard click input, and variable keyboard ranges remain unimplemented.

## 0.1.32 Update - Character EQ redesign

- CMake project version: `0.1.32`.
- Debug GUI pitch subtitle build string: `Build: character-eq-001`.
- Replaced the previous one-pole high/mid difference Character tone path with lightweight per-voice biquad EQ.
- Each harmony voice now owns three reusable `CharacterBiquad` filter states in `SimpleChoirEngine::VoicePitchState`.
- Character filter coefficients are configured once per render block for active voices, not per sample.
- Character Type changes reset the per-voice Character filter states to reduce stale filter coloration.
- Character Amount remains processor-smoothed over `20 ms` and still blends clean to processed tone with `clean + (charactered - clean) * amount`.
- Warm uses high-shelf cut around `4200 Hz`, low-mid peaking boost around `350 Hz`, and very light soft saturation.
- Bright uses high-shelf boost around `5500 Hz`, presence peaking boost around `3200 Hz`, and low-mid cleanup around `350 Hz`.
- Vowel uses slot-dependent formant-ish peaking EQ centers: `750`, `1050`, `1350`, `1700`, `2100`, `2500`, `950`, and `1500 Hz`, with mixed boost/cut gains and `Q=1.3`.
- Digital uses high-shelf boost around `4500 Hz`, presence peaking boost around `2400 Hz`, plus light soft saturation.
- Pitch ratio correction, pitch shifter design, input-synced window behavior, Voice 8 support, Tuned Lead, and Character Type list were not changed.
- Existing Character pitch detune, per-slot gain variation, and delay offset behavior are retained.
- Debug Character diagnostics now include `CharIn` and `CharDelta rms/pk/rel`, where `rel` is `DeltaRatioDb`.

## 0.1.31 Update - Character Type internal mode mapping

- CMake project version: `0.1.31`.
- Debug GUI pitch subtitle build string: `Build: character-mode-map-001`.
- Fixed Character Type GUI-to-DSP mode mapping.
- Root cause: JUCE `ComboBoxParameterAttachment` maps by selected item index, not ComboBox item ID. The GUI exposed 4 items while the APVTS `characterMode` choice previously had 5 items (`Clean`, `Warm`, `Bright`, `Vowel`, `Digital`), producing APVTS values `0`, `1`, `3`, and `4` for the visible GUI items.
- APVTS `characterMode` now has only the visible GUI choices: `Warm`, `Bright`, `Vowel`, `Digital`.
- GUI index to DSP internal mode conversion is centralized in `voxchord::characterModeGuiIndexToInternalMode()`.
- Internal mode mapping is now `Warm -> 1`, `Bright -> 2`, `Vowel -> 3`, and `Digital -> 4`.
- `SimpleChoirEngine::sanitizeCharacterMode()` now clamps internal Character modes to `1-4`.
- Clean behavior remains available through Character Amount `0%`, not through a visible Character Type.
- Debug Character mode display should now show internal modes `1/1`, `2/2`, `3/3`, and `4/4` for Warm, Bright, Vowel, and Digital respectively.
- Warm at Amount `100%` should now produce non-zero `CharDelta` while harmony voices are rendering.
- Bright now enters `applyCharacterTone()` `case 2`.

## 0.1.30 Update - Compact Character debug display

- CMake project version: `0.1.30`.
- Debug GUI pitch subtitle build string: `Build: character-debug-compact-001`.
- Debug GUI pitch runtime details are hidden by default via `showDebugPitchRuntimeDetails = false` in `PluginEditor.cpp`.
- Hidden-by-default pitch runtime fields include `RMS`, `Raw`, `Corr`, `Disp`, `RatioIn`, `Conf`, `Voiced`, `Fix`, and `RatioSmooth`.
- Character diagnostics remain visible: `CharMode internal/safe`, `CharAmt raw/sm`, and `CharDelta rms/pk`.
- Self-test summary display remains hidden by default via `showDebugSelfTestSummary = false`.
- The pitch runtime detail code remains in place and can be restored by toggling `showDebugPitchRuntimeDetails`.

## 0.1.29 Update - Debug self-test quiet mode

- CMake project version: `0.1.29`.
- Debug GUI pitch subtitle build string: `Build: debug-runtime-quiet-001`.
- Debug startup self tests are disabled by default via `enableDebugStartupSelfTests = false` in `PluginProcessor.cpp`.
- `runPitchDetectorSelfTest()` and `runPitchShifterSelfTest()` remain in the codebase and can be restored by toggling the flag.
- Debug GUI self-test summary display is disabled by default via `showDebugSelfTestSummary = false` in `PluginEditor.cpp`.
- Pitch shifter self-test summary formatting code remains available for future re-enable.
- Runtime pitch / Character diagnostics remain visible in Debug builds.

## 0.1.28 Update - Character signal-path diagnostics

- CMake project version: `0.1.28`.
- Debug GUI pitch subtitle build string: `Build: character-diagnostics-001`.
- Added Character diagnostics to `PitchState` and the Debug GUI subtitle.
- Debug GUI now displays `CharMode internal/safe`, `CharAmt raw/sm`, and `CharDelta rms/pk`.
- `characterMode raw` is now the DSP internal Character mode after `voxchord::characterModeGuiIndexToInternalMode()` conversion.
- `characterMode sanitized` is the `SimpleChoirEngine::sanitizeCharacterMode()` result used by DSP.
- `characterAmount raw` is the APVTS `character` parameter value used as Character Amount.
- `characterAmount smoothed` is the processor-smoothed value passed to `SimpleChoirEngine`.
- `characterDeltaRms` and `characterDeltaPeak` measure the per-block difference between the pre-character harmony voice sample and the post-`applyCharacterTone()` sample.
- Character diagnostics are measured only on harmony voices; Character does not process the original Dry path or Tuned Lead path.
- Character processing path is: GUI Amount slider -> APVTS `character` -> `PluginProcessor::getCharacter()` -> `characterAmountSmoothed` -> `SimpleChoirEngine::render()` -> `getCharacterPitchRatio()` / `getCharacterGain()` / `getCharacterDelayOffsetSamples()` / `applyCharacterTone()` -> wet harmony output.
- GUI `Char Type` uses APVTS `characterMode`; conversion to DSP internal modes is centralized in `voxchord::characterModeGuiIndexToInternalMode()`.
- `applyCharacterTone()` return value is multiplied by the voice envelope and then written to the wet harmony output.

## 0.1.27 Update - stronger Character coloration

- CMake project version: `0.1.27`.
- Debug GUI pitch subtitle build string: `Build: character-strength-001`.
- Character Amount 100% coloration was strengthened while keeping Amount 0% effectively Clean.
- Warm tone shaping now applies stronger high attenuation and low warmth in `applyCharacterTone()`.
- Bright tone shaping now applies stronger high emphasis in `applyCharacterTone()`.
- Vowel tone shaping now uses an 8-slot `vowelMidBySlot` coefficient table for more varied voice color.
- Digital tone shaping now applies stronger high and mid emphasis in `applyCharacterTone()`.
- Character Amount tone blend now uses `pow(amount, 1.2)` so low Amount values remain controlled while 100% is more obvious.
- Pitch detune, pitchRatio behavior, gain variation, delay offsets, pitch shifter, and input-synced window behavior were not strengthened in this pass.
- The main strengthening target is tone/EQ, preserving pitch accuracy and live stability.

## 0.1.26 Update - Character Type plus Amount

- CMake project version: `0.1.26`.
- Debug GUI pitch subtitle build string: `Build: character-type-amount-001`.
- Character control is now split into `Char Type` and `Amount`.
- `Char Type` is backed by existing `characterMode`; GUI-visible choices are `Warm`, `Bright`, `Vowel`, and `Digital`.
- `Formant-ish` was renamed to `Vowel` for GUI/parameter display.
- Internal `characterMode` index `0` (`Clean`) remains for compatibility, but is not shown in the normal GUI dropdown.
- `Amount` reuses the existing `character` parameter ID instead of adding a new `characterAmount` ID.
- `character` / Amount range remains `0.0-1.0`; default is now `0.0`.
- Amount `0%` is effectively Clean for all Character Types.
- Amount `100%` applies the full selected Character Type.
- Amount scales pitch detune, per-slot gain variation, delay offset, and tone shaping.
- Amount is not used as an empirical pitch-ratio correction; it only scales the existing per-character coloration.
- Processor-side Character Amount smoothing uses the existing APVTS smoothing style with a `20 ms` smoother.
- Existing input-synced window, Tuned Lead, 8 voice support, and MIDI transition de-click architecture remain unchanged.

## 0.1.25 Update - Priority B tuned lead, 8 voices, character modes

- CMake project version: `0.1.25`.
- Debug GUI pitch subtitle build string: `Build: priority-b-001`.
- Added APVTS parameter `leadTuneEnabled` (`Lead Tune`, bool, default `false`).
- Lead Tune uses the existing input-synced delay-window pitch shifter; no new pitch shifter and no empirical ratio correction were added.
- Lead Tune computes chromatic correction from `correctionInputPitchHz` to the nearest equal-tempered MIDI note using `69 + 12 * log2(hz / 440)`.
- When `Lead Tune` is on, the dry side of Dry/Wet uses the tuned lead buffer; when off, it uses the original dry input.
- If the input is unvoiced or `correctionInputPitchHz <= 0`, the tuned lead path crossfades safely back to the original dry input.
- Tuned lead has independent pitch/window/envelope state from MIDI harmony voices, with short attack/release de-clicking.
- Processor-side dry selection between original input and tuned lead is smoothed over `12 ms`.
- `MidiVoiceState::maxVoices` was expanded from `4` to `8`.
- `voiceCount` keeps the same parameter ID and now ranges from `1` to `8`, default `4`.
- Harmony voice arrays, voice snapshot publication, slot display, panning, character offsets, and render loops now support 8 voice slots.
- Added APVTS parameter `characterMode` (`Character`, choice, default `Clean`) with choices `Clean`, `Warm`, `Bright`, `Formant-ish`, and `Digital`.
- Existing `character` parameter ID remains for backward compatibility, but the visible GUI and current DSP mode selection use `characterMode`.
- Character modes use lightweight per-voice one-pole tone shaping plus small per-slot delay/detune/gain differences for the more colored modes.
- Input-synced window continuity, MIDI note transition de-click, Constant Voice Level, Input Gain, ASIO/Standalone/VST3 structure, and existing self-test summaries remain in place.

## 0.1.24 Update - focused live GUI layout

- CMake project version: `0.1.24`.
- Debug GUI pitch subtitle build string: `Build: gui-layout-001`.
- The unused Tune knob was removed from the visible GUI.
- The `tune` APVTS parameter remains for compatibility, but is not exposed in the current GUI layout.
- Main performance controls are now `Voices`, `Glide`, `Character`, `Spread`, and `Dry/Wet`.
- Input Source selection moved to the upper-right control area.
- Input Gain, Output Gain, and PANIC moved to the upper-right control area and use smaller controls than the main performance knobs.
- Input and Output meters are now custom horizontal bar indicators in the lower-right area.
- Meter bars show post-input-gain input level and final output level, with clip state reflected by the bar outline/fill color.

## 0.1.23 Update - priority A live usability pass

- CMake project version: `0.1.23`.
- Debug GUI pitch subtitle build string: `Build: priority-a-001`.
- Release GUI pitch subtitle now shows only `VoxChord v` plus `JucePlugin_VersionString`.
- Debug GUI pitch subtitle keeps runtime pitch / RMS / confidence display; self-test summary display is now disabled by default.
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
- Debug startup self tests are disabled by default; set `enableDebugStartupSelfTests = true` to run `SimpleChoirEngine::runPitchDetectorSelfTest()` and `SimpleChoirEngine::runPitchShifterSelfTest()` once from the processor constructor.
- GUI 共有用の MIDI / pitch / meter 状態は atomic または専用 state 経由で公開する。

`Source/PluginEditor.h`, `Source/PluginEditor.cpp`

- Current visible GUI is organized into Header, Harmony, MIDI, and Level areas.
- Header is split into `LogoArea`, `GainArea`, and `UtilityArea`.
- `GainArea` contains horizontal `Input Gain` and `Output` sliders with direct editable numeric labels.
- `UtilityArea` top row contains `Auto Tune` and a short right-aligned `Input Source`; second row contains `Mono Out` and a short right-aligned `PANIC` button.
- Visible Harmony controls: Voices, Glide, Character subcard, Spread, Dry/Wet.
- Character subcard contains a `Character` title, compact `Type` combo box, and larger `Amount` knob with editable percent value.
- The Tune APVTS parameter remains but the unused Tune knob is hidden.
- Level area uses vertical input and output meters; dB values are drawn below the meter fill.
- MIDI area displays pitch, last MIDI event, active note names, voice slots, and the display-only mini keyboard.
- Timer runs at `30 Hz`.
- Debug GUI subtitle build string is `Build: midi-input-debug-001`.
- Release build subtitle shows only `VoxChord v` plus the plugin version.

- 1 画面のライブ向け GUI。
- Main performance controls: Voice Count, Glide, Character, Spread, Dry/Wet。
- Right-top utility controls: Input Source, Input Gain, Output, PANIC。
- Right-bottom horizontal bar meters: Input, Output。
- MIDI note indicator、voice slot 表示、last MIDI event、pitch debug を持つ。
- Timer は `30 Hz`。
- pitch debug subtitle の現在の Debug build string は `Build: priority-a-001`。
- Release build subtitle shows only `VoxChord v` plus the plugin version.
- Debug GUI self-test summary display is disabled by default; set `showDebugSelfTestSummary = true` to show the pitch shifter self test summary again.
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

- CMake project version: `0.1.40`
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
5. Render wet choir into `wetBuffer` and optional tuned lead into `tunedLeadBuffer` using `SimpleChoirEngine`.
6. Publish pitch debug fields to atomics.
7. Mix dry/wet into host output buffer, using original dry or tuned lead as the dry side depending on `leadTuneEnabled`.
8. Apply smoothed output gain.
9. Publish input/output meters.

Dry/wet and output:

- `Input Gain` is smoothed over `0.02 sec`.
- `Dry/Wet` is smoothed over `0.02 sec`.
- Lead Tune dry-source switching is smoothed over `0.012 sec`.
- `Output` gain is smoothed over `0.02 sec`.
- Input Gain parameter range is `-24.0 dB` to `+24.0 dB`, default `0.0 dB`.
- Output parameter range is `-24.0 dB` to `+6.0 dB`, default `-3.0 dB`.

## Parameters

`voiceCount`

- Type: integer
- Range: `1-8`
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
- Default: `0.0`
- Visible GUI label: `Amount`
- Reused as Character Amount for compatibility.
- Amount `0%` is effectively Clean; Amount `100%` applies the full selected Character Type.
- Scales Character pitch detune, per-slot gain variation, delay offset, and tone shaping.

`characterMode`

- Type: choice
- APVTS / GUI-visible choices: `Warm`, `Bright`, `Vowel`, `Digital`
- APVTS GUI indexes: `0=Warm`, `1=Bright`, `2=Vowel`, `3=Digital`
- DSP internal modes: `1=Warm`, `2=Bright`, `3=Vowel`, `4=Digital`
- Conversion function: `voxchord::characterModeGuiIndexToInternalMode()`
- Default: `Warm`
- Controls lightweight per-voice tone shaping and small colored-mode detune/delay/gain offsets, scaled by `character`.
- Normal Clean behavior is achieved with Amount `0%`.

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

`leadTuneEnabled`

- Type: bool
- Default: `false`
- When enabled, the dry side is replaced by a chromatically tuned lead generated from `correctionInputPitchHz`.
- When disabled, the dry side remains the original input.

`monoOutputEnabled`

- Type: bool
- Default: `false`
- GUI label: `Mono Out`
- When enabled and stereo output is available, final left/right output is replaced by `0.5 * (left + right)` on both channels.
- Mono Out is applied after Dry/Wet mix and Output gain.
- Mono Out switching is smoothed over approximately `12 ms`.
- This does not select physical output channels in VST3 or Standalone; host/device routing remains external.

## MIDI Voice Specification

- Maximum voices: `8`
- Active notes are exposed as an 8-slot snapshot.
- Slot allocation favors empty slots, then uses nearest-note voice stealing with oldest age as the tie breaker.
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
- `characterAmountRaw`: APVTS Character Amount value.
- `characterAmountSmoothed`: processor-smoothed Character Amount value.
- `characterDeltaRms`: per-block RMS difference between clean shifted harmony voice and Character-processed voice.
- `characterDeltaPeak`: per-block peak difference between clean shifted harmony voice and Character-processed voice.
- `characterDeltaRatioDb`: `20 * log10(characterDeltaRms / characterInputRms)`, clamped through JUCE dB conversion.
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

- Debug-only self test code remains available, but startup execution is disabled by default via `enableDebugStartupSelfTests = false`.
- When re-enabled, it runs once from processor constructor.
- Test frequencies: `100`, `150`, `220`, `261.63`, `329.63`, `440`, `523.25`, `600`, `659.25`, `700`, `800`, `880 Hz`.
- Harmonic correction is OFF during self test.
- Self test must not run inside normal real-time `processBlock()`.

Pitch shifter self test:

- Debug-only self test code remains available, but startup execution is disabled by default via `enableDebugStartupSelfTests = false`.
- When re-enabled, it runs once from processor constructor after pitch detector self test.
- Does not use PitchDetector or MIDI voice allocation.
- Uses an internal sine wave, one `VoicePitchState`, Character=0 equivalent, delay offset `0`, and `glideCoefficient=1.0f`.
- Ratio smoothing and glide are fully bypassed by setting `currentPitchRatio` and `targetPitchRatio` to the fixed ratio and calling `renderPitchShiftedSample()` with glide coefficient `1.0f`.
- Measures output frequency from positive-going zero crossings after initial transient skip.
- Stores separate fixed-window and input-synced-window summaries in `PitchShifterSelfTestSummary`.
- GUI debug subtitle self-test summary display is disabled by default via `showDebugSelfTestSummary = false`; when re-enabled it displays fixed-window and input-synced-window summaries independently, including PASS/FAIL, max error cents, worst input Hz, worst ratio, and worst measured Hz.
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

- Character Amount `0%` is clean-equivalent for all Character Types.
- Character Amount `100%` applies the selected Character Type at full strength.
- Tone processing uses per-voice lightweight biquad EQ states.
- Warm: high shelf cut around `4200 Hz`, low-mid peaking boost around `350 Hz`, plus very light soft saturation.
- Bright: high shelf boost around `5500 Hz`, presence boost around `3200 Hz`, and low-mid cleanup around `350 Hz`.
- Vowel: slot-dependent formant-ish peaking EQ using centers `750`, `1050`, `1350`, `1700`, `2100`, `2500`, `950`, `1500 Hz` and mixed boost/cut gains.
- Digital: high shelf boost around `4500 Hz`, presence boost around `2400 Hz`, plus light soft saturation.
- Per-slot pitch detune remains limited to Digital/Vowel Character color via `getCharacterPitchRatio()`.
- Existing per-slot gain and delay variation remain lightweight and Amount-scaled.
- Character DSP does not perform formant shifting, FFT/STFT processing, PSOLA, or pitch ratio correction.

Spread:

- For multiple active voices, pan positions are distributed linearly from left to right and scaled by Spread.
- Single active voice is centered.

## GUI Specification

Window:

- Size: `860 x 540`
- Timer: `30 Hz`
- Dark, single-screen live-oriented UI.

Controls:

- Voice Count
- Glide
- Character Type and Amount grouped as Character
- Spread
- Dry/Wet
- Header utility controls: Auto Tune, Input Source, Mono Out, PANIC button.
- Header gain controls: larger Input Gain and Output sliders with editable dB value labels.
- Voices, Glide, Character Amount, Spread, and Dry/Wet have editable value labels.

Status/debug:

- MIDI notes and pitch summary.
- Display-only mini MIDI keyboard for C2-C6.
- Voice slots.
- Last MIDI event.
- Vertical input and output meters in the Level area.
- Output meter displays stereo L/R post-output peaks; with Mono Out enabled, both channels display the mono result.
- Pitch debug subtitle currently includes:
- Debug: `Build: midi-input-debug-001`
- Release: `VoxChord v<version>`
- Pitch shifter self-test summary is hidden by default; re-enable `showDebugSelfTestSummary` to show `Pitch Shifter SelfTest: PASS/FAIL`.
- `RMS`
- `Raw`
- `Corr`
- `Disp`
- `RatioIn`
- `Conf`
- `Voiced`
- `Fix`
- `RatioSmooth`
- `CharMode internal/safe`
- `CharAmt raw/sm`
- `CharIn`
- `CharDelta rms/pk/rel`

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
