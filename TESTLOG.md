# VoxChord Test Log

## 0.4.1 UI pass: Release cleanup, better level meters, usability tweaks

Date: 2026-07-10

Scope: Branch `exp/d3-psola`. GUI-only iteration (no DSP, no parameter changes) using the `vst-ui` capture loop. VERSION `0.4.0` -> `0.4.1`.

Changes (`PluginEditor.h/.cpp`):

- **Release cleanup**: the MIDI delivery counter row below the mini keyboard (`midiStatusLabel`) is now Debug-only (component not added, text not built in Release). The empty D1 band above the keyboard is also gone in Release. The mini keyboard now fills the remaining MIDI card height, so the panel reads as a keyboard rather than a debug console.
- **Level meters**: added ballistics (instant attack, ~250 ms release) and a 1.5 s peak-hold marker that then decays, replacing the raw per-block peak that flickered. The fill now uses a fixed level-mapped gradient over the whole `-60..0 dB` range (green to about -15 dB, amber near -8 dB, red at 0 dB) instead of a gradient that rescaled with the level, so a given bar height always has the same colour. Clipping draws a red cap + red outline and swaps the dB readout for `CLIP`. The `-6/-12` scale ticks are brightened relative to `-24/-48`.
- **Usability** (no CPU cost): double-clicking any knob or header slider returns it to its parameter default; clicking any meter clears the latched clip flags (previously only PANIC did).

CPU: all meter state updates happen in `setLevel()` on the existing 30 Hz editor timer (a few float ops per meter); `paint()` adds one clipped rounded-rect fill, one 2 px line and, when clipped, one small rect. No audio-thread work was touched.

Verification (agent, `vst-ui` capture loop, 3 captures):

- Debug capture: v0.4.1 subtitle, D1 row and MIDI counters still present (Debug-only behaviour intact), meters show gradient + peak-hold markers.
- Release capture (first pass): MIDI counters and D1 band gone, keyboard enlarged, no clipping/overlap, subtitle `VoxChord v0.4.1` fits. Meter gradient turned amber-ish too early (~-23 dB).
- Release capture (second pass, after moving the gradient stops to 0.75/0.88): bar reads saturated green at -40 dB, peak-hold markers visible. All checks pass: intended changes present, no overflow/overlap, alignment and contrast fine, no layout regressions elsewhere.
- Final capture: `scratchpad/ui-check-04-release.png` (see also `ui-check-01.png` = before, `ui-check-02-debug.png`).
- Plugin Debug + Release built (VST3 + Standalone), exit 0, only pre-existing warnings.

Also noted (not a defect): the Standalone restores its saved session, so its `Retune` showed `200 ms` (stored value) rather than the new `1 ms` default; APVTS defaults apply to new instances only.

Not verifiable from a static capture (user verification pending):

1. Meter ballistics and peak-hold decay in motion with real audio; whether the release time feels right.
2. Clip behaviour: drive the input to clipping, confirm the red cap + `CLIP` text, then click a meter to clear it.
3. Double-click reset on each knob returns the expected default.
4. DAW-hosted rendering (scaling, resizing) of the new meter and the Release MIDI panel.

## 0.4.0 Beta freeze: High Quality (PSOLA) as the default engine (directions/0709_1)

Date: 2026-07-09

Scope: Branch `exp/d3-psola`. D6 integration / beta freeze. Following the listening gate, the user decided: (1) present PSOLA as **"High Quality"** and make it the default engine, (2) **accept the Classic/PSOLA wet level difference as specified behaviour** (no level matching), (3) **keep the Classic window engine** — unchecking High Quality selects it. No DSP logic changed. VERSION `0.3.12` -> `0.4.0` (beta, mirroring the `0.2.0` beta-initial convention).

Changes:

- `VoxChordParameters.cpp`: `psolaEnabled` display name `PSOLA Engine` -> `High Quality`, default `false` -> `true`. **Parameter ID unchanged** (`psolaEnabled`), so saved states stay compatible.
- `PluginEditor.h/.cpp`: toggle label `PSOLA` -> `High Quality`. `resized()` reallocates the header: `logoArea` 220 -> 200 px, `utilityArea` 296 -> 316 px (the gain column width is untouched); bottom utility row re-split to `PANIC 86 / High Quality 116 / Mono Out (remainder)` so the longer label fits.
- SPEC: Engine Modes section is no longer marked experimental; default is High Quality; the ~7 dB Classic-vs-PSOLA wet level difference is documented as accepted behaviour; both engines are retained.

Verification (agent):

- Plugin Debug + Release built (VST3 + Standalone), all exit 0; only pre-existing warnings. Debug VST3 still reports `VoxChord_dbg`.
- Harness rerun: all probes identical to 0.3.11 (D4 noise passthrough 0.986, vowel f0 -1.9 c; retune step response 193.9 / 63.9 / 31.4 / 26.7 ms) — confirms the DSP paths were untouched and only the default/labels moved.

Not verified by agent:

- **GUI layout was changed but not visually inspected** (the agent cannot see the rendered editor). The `High Quality` label at 116 px and `Mono Out` in the remaining width are calculated to fit, but overlap/clipping must be confirmed on screen. In Debug builds the subtitle (`VoxChord v0.4.0 | Build: lead-retune-001`) may now clip in the narrower 200 px logo column; Release subtitle (`VoxChord v0.4.0`) should fit.

User verification pending:

1. Screenshot / visual check of the header row: `High Quality` and `Mono Out` labels not clipped or overlapping `PANIC`.
2. A fresh instance defaults to High Quality checked; unchecking it audibly switches to the Classic engine with no glitch.
3. Existing saved sessions keep their stored `psolaEnabled` value (APVTS defaults apply to new instances only).
4. General beta pass: Panic, note on/off, Character modes, Spread, Lead Tune, Standalone startup.

## 0.3.12 Retune default 100%, no Retune knob (user decision)

Date: 2026-07-09

Scope: Branch `exp/d3-psola`. User confirmed the 0.3.11 Auto Tune rework behaves as intended, and decided **not** to add a Retune knob — exposing it would violate the project's one-screen / simple-UI principle. Retune instead defaults to full hard-tune. No DSP logic changed. VERSION `0.3.11` -> `0.3.12`.

Changes:

- `VoxChordParameters.cpp`: `tune` (display name `Retune`) default `0.80` -> `1.00` = instant snap (~1 ms retune smoothing; total retune-to-note still floored by the ~27 ms YIN detection latency). Parameter ID unchanged.
- `PluginEditor.cpp`: Debug readout fallback value for `tune` aligned to `1.0` (only used if the parameter lookup fails).
- SPEC: `tune` default and the "no on-screen knob" note reworded from a known limitation to a deliberate design choice (host-automatable only).

Build status:

- Built by agent: harness Release, plugin Debug + Release (VST3/Standalone), all exit 0. Only pre-existing warnings (harfbuzz C4819; existing unused-local/unused-parameter warnings in `PluginProcessor.cpp` / `SimpleChoirEngine.cpp`, none from this change).
- Retune step-response measurements are unchanged from 0.3.11 (the probe drives `tune` explicitly; only the plugin default moved).

User verification pending:

1. Fresh plugin instance defaults to full hard-tune on Lead Tune (previously ~9 ms snap).
2. No clicks on rapid pitch changes at the harder default.
3. Existing saved sessions keep their stored `tune` value (APVTS defaults apply to new instances only) — confirm nothing regressed in an old project.

## 0.3.11 Lead Tune rework: window lead + Retune Speed (directions/0708_9, supersedes 0703_2)

Date: 2026-07-09

Scope: Branch `exp/d3-psola`. User feedback: PSOLA-mode Auto Tune felt less intuitive than the window shifter; wanted the window feel kept but a stronger auto-tune. This iteration makes the tuned lead always use the window shifter (both engine modes) and revives the `tune` parameter as Retune Speed (the long-pending D2 work, `0703_2.md`, now superseded by `0708_9.md`). Harmony voices are unchanged. VERSION `0.3.10` -> `0.3.11`.

Root-cause analysis (4 points): the lead replaces the dry monitor signal, so (1) the PSOLA lead's fixed 23.7 ms latency was felt directly as monitoring delay; (2) PSOLA's formant preservation + AGC made the correction too transparent ("no grip"); (3) the D4 voiced/unvoiced split dropped the lead to dry on consonants/breath; (4) `tune` was DSP-inert so the snap speed was tied to Glide smoothing.

Changes:

- `SimpleChoirEngine`: the tuned lead always uses `renderPitchShiftedSample` (window shifter); removed `psolaLeadShifter` / `psolaLeadHighpassFilter` / `psolaLeadCurrentRatio` and the lead PSOLA processing. `psolaScratch` shrank to `maxVoices + 1` channels. PSOLA bank now covers the 4 harmony voices only.
- Pitch path split: new `PitchState::tunePitchHz` = harmonic-corrected pitch with no median/smoothing (0 when unvoiced), published through a new processor atomic. The display/harmony path (`correctionInputPitchHz`, median + fast-attack) is untouched — verified by diffing `updateStablePitch`/`updateCorrectionInputPitch`/`applyMedianFilter` (unchanged) and by the harness harmony/shift tables being identical to 0.3.10.
- Retune Speed: `getLeadRetuneCoefficient(tune, sr)` gives a per-sample log-domain ratio smoothing coefficient with 90% settle `1 + 199*(1-tune)^2` ms; the lead uses it instead of the Glide-derived coefficient. `tune` is now consumed (was `ignoreUnused`). Lead target now uses `tunePitchHz`.
- GUI/params: `tune` APVTS display name `Tune` -> `Retune` (ID unchanged). Debug line adds a Retune section (effective ms, tune pitch + divergence vs display pitch, lead target note); Debug build id `d3-psola-ab-001` -> `lead-retune-001`.
- Harness: new retune step-response probe (continuous-phase sine steps 212->216 Hz across the G#3/A3 boundary with Lead Tune on; measures 90% settle of the lead output frequency).

Measured (Release harness):

- Retune step response (measured 90% settle vs theoretical retune-only): Retune 0% -> 193.9 ms (200), 50% -> 63.9 ms (51), 80% -> 31.4 ms (9), 100% -> 26.7 ms (1). Slow settings match theory; fast settings floor at ~27 ms = the pitch detector's own latency (YIN 2048 frame + hop). So `tune` controls the snap for gentle-to-medium settings and the retune smoothing itself is 1-9 ms at high settings, but the *total* retune-to-new-note time cannot beat the ~27 ms detection floor. Documented as a known limit; reducing it is a detector change (out of scope).
- No regressions: shifter comparison tables, Character probe, and D4 probe (noise passthrough 0.986, vowel f0 -1.9 c) all unchanged from 0.3.10.
- Latency: PSOLA-mode lead drops from 23.7 ms (removed PSOLA lead) to the window shifter's pitch-synced ~13-20 ms; harmony and dry unchanged.

Scope note (flagged for follow-up): the current editor has **no on-screen Tune/Retune knob** — the `tune` parameter was orphaned (APVTS-only). This iteration makes it functional and renames it, but per the direction's "no GUI layout change" boundary (and because an unverifiable layout edit is risky without visual check) no knob was added. The default `tune = 0.80` already delivers a ~9 ms hard-tune snap, so the improvement lands at the default. Adding a Retune knob to the editor is the recommended next step.

Build status:

- Built by agent: harness Release (measurements above), plugin Debug + Release (VST3/Standalone). Only pre-existing harfbuzz C4819 codepage warnings; no errors.

User verification pending (listening):

1. PSOLA-mode Auto Tune should now feel as "direct" as the window engine (lead is window in both modes) — confirm the improved feel and lower monitoring latency.
2. Retune character: with `tune` at default (~9 ms smoothing, ~27 ms total) vs lowered values, confirm hard-tune vs gentle behaviour (control via host automation until a knob exists).
3. Lead Tune OFF, harmony voices, and Glide must be unchanged from before.
4. Consonants/breath under Lead Tune should no longer drop out oddly (lead no longer follows the harmony D4 split).

## 0.3.10 D4: voiced/unvoiced split on the PSOLA path (directions/0708_8)

Date: 2026-07-08

Scope: Branch `exp/d3-psola`. PLAN-0.3 phase D4. Unvoiced input (consonants, breath) is no longer pitch-shifted on the PSOLA path: the shifters crossfade to a latency-matched dry copy, so consonants stay natural inside the harmony voices. Classic engine path unchanged (deferred until the D6 default-engine decision). VERSION `0.3.9` -> `0.3.10`.

Changes:

- `PsolaShifter::setVoicedAmount()`: per-sample ~15 ms crossfade between the shifted output (with makeup gain) and `inputRing[writePos - latency]` (exact latency-matched dry). Loudness normalization freezes while unvoiced so consonant statistics don't steer the voiced gain.
- `SimpleChoirEngine`: voiced hysteresis from the detector (`pitchState.voiced && confidence > 0.75` to turn on, `!voiced || confidence < 0.55` to turn off) feeding all 5 shifters per block.
- Harness: new D4 probe — alternating 300 ms vowel (196 Hz) / 100 ms white noise through the engine in PSOLA mode (MIDI 62, ratio ~1.5), plus a standalone forced-unvoiced shifter sanity check.

Measured (Release harness):

- Noise segments: wet vs latency-matched dry correlation **0.986** (pass > 0.9, 6 segments); standalone forced-unvoiced shifter: 0.998.
- Vowel segments: f0 293.34 Hz (-1.9 c vs target, sd 0.9 c) — still correctly shifted; detection reported unvoiced on 0/53 noise blocks (mean confidence 0.24), hysteresis behaves.
- Steady-voiced regression: none (all existing sine/vowel tables identical; voicedAmount stays 1 for steady tones).
- Probe debugging notes: two measurement bugs were found and fixed in the harness itself (white noise decorrelates at +/-1 sample -> lag search around L; correlation windows must be aligned to the burst pattern period). The engine/DSP needed no fix.

Build status: harness Release + plugin Debug/Release built by agent, no new warnings.

User verification pending (listening):

1. Sung phrases with consonants/sibilants in PSOLA mode: consonants should now sound natural (not robotic/chopped) inside the harmony; check the voiced<->unvoiced transitions for smoothness.
2. Sustained notes must be unaffected; rapid speech-like input is the stress case (hysteresis chatter would be audible as flutter).
3. Breathy singing: breath noise now passes unshifted — confirm this reads as "natural choir" rather than "dry leakage".

## 0.3.9 PSOLA adaptive loudness + stronger Vowel character (directions/0708_7)

Date: 2026-07-08

Scope: Branch `exp/d3-psola`. User feedback on 0.3.8: high notes simply sound quieter (not interval-dependent); Vowel character could be stronger. Root cause of the former: the 0.3.7 static gain was calibrated on a bright synthetic vowel; real voices have steeply falling spectral envelopes, so upshifted combs sample much less energy — no static curve fits all material. VERSION `0.3.8` -> `0.3.9`.

Changes:

- `PsolaShifter`: built-in slow loudness normalization — block input/output energies smoothed with a ~250 ms time constant, makeup gain steered toward input-RMS parity, clamp 0.5..2.8, gain applied after measurement (open loop, cannot pump). Static per-grain compensation kept as the fast first-order term.
- Harness: new "dark 196" vowel signal (formants 500/1100/2300 Hz with steeply falling gains) reproducing the realistic-envelope case.
- Character Vowel: formant gain x1.6 -> x2.2, LFO sweep +/-12% -> +/-18%, Q 2.2 -> 2.5.

Measured (Release harness):

- PSOLA wet RMS vs unison: all vowel runs (bright, vibrato, dark) now within **-1.0..0.0 dB** (0.3.7: +/-1.8 dB bright-only; dark high targets would have drooped further). Dark vowel +19 semitones: -0.7..-1.0 dB.
- No regressions: f0, HNR, LF-noise, AM, latency all at 0.3.7 levels (dark-vowel runs are near-ideal: HNR ~40 dB, LF < -100 dB — the dark tone is close to a pure low-partial tone).
- Character probe: Vowel now HNR +3.2, RMS +2.9 dB vs baseline (was +2.5/+2.3); band signature unchanged in shape.
- Known/deferred (D6 calibration item): Classic wet sits ~7 dB below PSOLA wet at unison (Classic's fixed 0.45 voice gain vs PSOLA's input-parity AGC). Existed since 0.3.6; engine-to-engine level matching belongs to the D6 integration pass.

Build status: harness Release + plugin Debug/Release built by agent, no new warnings.

User verification pending: high-note loudness with real voice (should now track the input); Vowel character strength (constants easy to trim further).

## 0.3.8 D5-lite: Character mode distinctiveness (directions/0708_6)

Date: 2026-07-08

Scope: Branch `exp/d3-psola`. Per user request ("make the 4 Characters' identities stand out"), each mode was re-tuned around a clear signature within the existing framework (3 biquads + saturation + per-slot detune/gain/delay, blended by amount). Two cheap new primitives: per-slot formant-sweep LFOs (block-rate, Vowel) and a per-voice sample-hold decimator (Digital). Applies in both Classic and PSOLA modes (Character sits after the shifter stage). VERSION `0.3.7` -> `0.3.8`.

Mode changes (`SimpleChoirEngine`):

- **Warm** (dark/thick): HS 3.5 kHz -9 dB, peak 300 Hz +4.5 dB, saturation drive 1+0.6a / mix 0.5a, slot detune x0.4 (was 0).
- **Bright** (hard/airy): HS 6 kHz +7 dB, peak 3.2 kHz +4.5 dB Q1.4, peak 280 Hz -4 dB, no detune.
- **Vowel** (moving mouths): per-slot formant peaks, gain x1.6, Q 1.3 -> 2.2, centers swept +/-12% by free-running per-slot LFOs (0.08-0.22 Hz, advanced per block), slot detune x1.0 (was x0.75).
- **Digital** (robotic/lo-fi): sample-hold decimator (hold 1..10 samples by amount, ~4.4 kHz effective rate at full), saturation drive 1+1.2a, HS 4.5 kHz +6 dB, detune 1.6 -> **0** (hard-locked robot; contrast with Vowel).
- New state: `characterLfoPhases[8]` (engine), `decimatorHoldValue/Counter` (per voice, reset with the voice). `configureCharacterTone` gained an `lfoPhase` parameter. No latency impact (all per-sample filters/hold); amount 0 remains a full bypass.

Character probe (new harness section: vowel 146.83 Hz + chord MIDI 50/54/57/62, Classic engine, amount 1.0 vs amount-0 baseline; band deltas in dB lowmid 200-500 / formant 900-1800 / presence 2.5-5.5k / air 6.5-11k):

- Warm:    +4.5 / -1.4 / -0.1 / **-4.9**  (dark: low up, air down)
- Bright:  -2.7 / +0.1 / +4.4 / **+5.8**  (mirror of Warm)
- Vowel:   +1.2 / -3.2 / +6.2 / +1.9, HNR +2.5 (detune de-phasing + swept formants; the movement itself is time-domain, not captured by static bands)
- Digital: +4.9 / +1.2 / +12.6 / **+20.3**, HNR -1.1 (decimator aliasing = intended lo-fi)
- All four signatures differ in sign pattern; amount-0 output matches the baseline (bypass integrity).

Build status:

- Built by agent: harness Release (probe above; shifter comparison tables unchanged from 0.3.7), plugin Debug + Release (VST3/Standalone), no new warnings.

User verification pending (listening):

1. Whether each mode now has an obviously distinct musical identity at amount 50-100%, and whether the strengths feel right (all values are single constants in `configureCharacterTone`/`applyCharacterTone`, easy to trim).
2. Vowel mode: the slow formant movement should read as "choir mouths", not as wobble.
3. Digital mode: decimator harshness at amount 100% (intended lo-fi; reduce the 9-sample max hold if too much).
4. Character modes under PSOLA engine (decimator + PSOLA interaction).
5. No clicks when switching modes or sweeping amount.

## 0.3.7 D3: PSOLA field-feedback fixes: 90 Hz floor, LF noise, loudness (directions/0708_5)

Date: 2026-07-08

Scope: Branch `exp/d3-psola`. Addresses the three issues from the user's 0.3.6 listening test: (1) lower the PSOLA input floor to ~90 Hz accepting more latency, (2) low-frequency noise with real voice + high/polyphonic MIDI, (3) wet level dropping as the MIDI note moves away from the input pitch. Also records the user's direction: keep PSOLA as the "high quality" option alongside the windowed engine. VERSION `0.3.6` -> `0.3.7`.

Changes:

- `psolaMinF0Hz` 110 -> 90 (`SimpleChoirEngine.h`): PSOLA wet delay 19.7 -> **23.7 ms** @44.1 kHz (user-accepted trade).
- `PsolaShifter`: pitch-mark peak search now runs on a ~700 Hz one-pole lowpassed guide ring (formant ripple can no longer pull marks off the glottal cycle); period slew limit +/-8% per block (detector jumps can't modulate mark spacing); **fractional-sample grain placement** (window + linearly interpolated content evaluated at the fractional synthesis position; removes the integer-grid quantization); per-grain loudness compensation `duty^-0.6` (downshift gaps) / `ratio^0.35` (upshift comb thinning), cap +6.8 dB.
- `SimpleChoirEngine`: per-voice tracking highpass at `0.6 * target f0` (RBJ, Q 0.707, clamp 30..1000 Hz) on the PSOLA path only — everything below the output comb's fundamental is artifact energy (mark-reuse sidebands at incommensurate ratios), so it is removed without touching wanted harmonics. New `setHighPassFilter` helper.
- Harness: vibrato vowel signal (196 Hz, +/-30 cents @5 Hz, offsets +19/+16/+12/+7/0) reproducing the reported case; LF-noise metric (band < 0.6*min(f0_in, f0_target) vs broad band); RMS metric (vs dry); mirrored tracking highpass so measurements match the plugin path.
- Tried and **rejected**: OLA-correlation micro phase alignment of grains — it aligns to the *output* periodicity and cancels the pitch shift entirely (output locks to input pitch). Documented here so it is not retried.

Measured (Release harness, agent-run; baseline -> fixed):

- **LF noise (item 2)**: worst reproduced case (vowel 196 vib, +16 semitones, ratio 2.52): psola **-38 -> -59 dB** (engine -56 dB); vib +19: -54 -> -80 dB; all vowel rows now at or better than the engine (-50..-81 dB). Root cause split: mostly mark-reuse phase sidebands below the output fundamental (removed by the tracking highpass), plus robustness from the guide/slew changes.
- **Loudness (item 3)**: B50 wet RMS vs unison across all vowel runs: was 0..-6.0 dB, now within **+/-1.8 dB** (target +/-3 dB).
- No regressions: f0 accuracy/stability, HNR, formant peaks unchanged or better (vowel294 +12 HNR 30.3 -> 32.4 dB); unison reconstruction improved (fractional placement); sine-unison SNR up (psolaA 220 Hz: 38 -> 94 dB).
- Latency: configured values track the new floor; plugin provisioning at 90 Hz = 1044 samples = 23.7 ms; measured (sine unison, envelope xcorr) tracks configured within ~2 ms.
- Known residual: vibrato at ratio ~3 still shows ~11-14% AM (engine: 27%); inherent to grain reuse under pitch movement, acceptable relative to the engine.

Build status:

- Built by agent: harness Release (measurements above), plugin Debug + Release (VST3/Standalone/OfflineTest), all artifacts produced, no new warnings. Note: plugin binaries were rebuilt after the version bump to 0.3.7.

User verification pending:

1. Real-voice retest of the 0.3.6 complaints: high MIDI + polyphony low-frequency noise (should be gone or greatly reduced), and wet loudness consistency across intervals.
2. Low input notes down to ~G2/A2 (new 90 Hz floor) in PSOLA mode.
3. Whether ~23.7 ms wet delay still feels acceptable.

## 0.3.6 D3: PSOLA A/B wiring into the plugin (directions/0708_4)

Date: 2026-07-08

Scope: Branch `exp/d3-psola`. Wires the TD-PSOLA shifter into the plugin behind a new `psolaEnabled` parameter (GUI toggle "PSOLA") so the listening gate (PLAN-0.3 gate 2) can be run. Engine-internal integration: detection, MIDI voices, envelopes, Character tone/gain/pitch, Spread and Lead Tune are shared; only the shifter stage switches. PSOLA config = plan B50 @110 Hz floor (wet delay ~19.7 ms @44.1 kHz; constants in `SimpleChoirEngine.h`, B75 fallback = one constant). VERSION `0.3.5` -> `0.3.6`.

Implementation status:

- `VoxChordParameters`: new `psolaEnabled` AudioParameterBool "PSOLA Engine", default false (Classic). **No existing parameter IDs changed.**
- `PluginProcessor`: raw-pointer + `getPsolaEnabled()` (private) and `isPsolaEnabledForUi()` (public), passed as a new trailing `render()` argument.
- `SimpleChoirEngine`: PSOLA bank members (4 voice shifters + lead, `psolaScratch` buffer, per-slot target/current ratios), prepared/reset with the engine. The bank runs **every block in both modes** (~0.7% CPU) so A/B switching is instant and warm; per-sample source selection is the only mode-dependent code. PSOLA ratio uses a wider clamp (1/16..8, vs classic 0.25..8) computed alongside the classic clamp in the voice-setup loop, so the D1 low-MIDI failure is actually fixed in PSOLA mode (implementation addition vs the direction, which had reused the classic-clamped ratio). Ratio smoothing per block via `1-(1-alpha)^N` compounding. Character per-slot delay offsets are not applied on the PSOLA path (documented limitation).
- `PluginEditor`: "PSOLA" ToggleButton next to Mono Out (visible in Release too); Debug pitch line now prefixed `[PSOLA]`/`[WIN]`; Debug build id -> `d3-psola-ab-001`.
- CMake: `PsolaShifter.cpp/.h` added to the `VoxChord` and `VoxChordOfflineTest` targets (already in `VoxChordShifterCompare`).
- `render()` gained a defaulted `psolaEnabled = false` parameter, so the offline harness and self-tests compile unchanged.

Build status:

- **Not built by agent. User will build** (user took over builds for this iteration):
  - `cmake --build build --config Debug`
  - `cmake --build build --config Release`
  (configure only if the cache is broken; `build/` currently uses the VS 18 2026 generator)

User verification pending (listening gate 2 checklist, from directions/0708_4.md):

1. A/B toggle while playing: no glitch/dropout beyond a momentary waveform jump (no crossfade is applied by design).
2. Low MIDI notes (A2 and below): pitch stability in both modes — PSOLA should hold pitch where Classic jumped an octave.
3. PSOLA warble reduction / voice quality at large shifts.
4. Whether the PSOLA wet delay (~19.7 ms) feels acceptable live.
5. B50 roughness on breathy/clean voices — if rough, set `psolaRightHalfFactor` to `0.75f` and rebuild.
6. Character modes / Spread / Lead Tune sanity in PSOLA mode.
7. Panic, rapid note on/off, no stuck notes; Debug build shows `VoxChord_dbg`, subtitle `Build: d3-psola-ab-001`, and the `[PSOLA]`/`[WIN]` prefix follows the toggle.

## 0.3.5 D3: PSOLA asymmetric grains (plan B) + 4-way comparison (directions/0708_3)

Date: 2026-07-08

Scope: Branch `exp/d3-psola`. Adds `rightHalfWidthFactor` to `PsolaShifter::prepare()` (two-piece Hann, right/future half shortened; factor 1.0 = 0.3.4 behaviour, verified bit-identical metrics). Harness rows reorganized to engine / psolaA (factor 1.0) / psolaB75 / psolaB50 (psolaQ dropped — its ratio<=0.5 defect was established in 0.3.3). Latency `L = Hl + factor*Hl + Pmax/2 + 64`. Plugin DSP untouched. VERSION `0.3.4` -> `0.3.5`.

Results (Release, agent-built and run; 110 Hz-floor plugin provisioning by formula):

- Latency: A' 24.2 ms / B75 21.9 ms / **B50 19.7 ms** — B50 meets the ~20 ms target. Sine-unison measured latencies track configured within ~2 ms.
- Vowel quality vs A': B75 within 0.5 dB HNR / 0.2 pt AM everywhere (on the 294 Hz vowel B75/B50 actually score 1-2.6 dB *better* HNR than A'); B50 worst case -1.2 dB HNR (147 Hz unison 24.5 -> 23.3). Both far inside the <=2 dB / <=2 pt acceptance bar. f0 accuracy/stability unchanged (<=0.1 c).
- Pure-tone cost: sine unison SNR B75 ~= A' (28.6/37.6/45.9 vs 29.0/38.4/45.5 dB); **B50 drops 6-17 dB** (22.8/27.0/28.2). Inaudible in the vowel metrics but suggests mild added roughness on very clean/breathy material — flag for the listening test.
- Side effect: the asymmetric window breaks the exact anti-phase cancellation, so the sine ratio-2.0 pathological case now yields the correct f0 (residual still dominates; spectral-envelope preservation unchanged).
- CPU: unchanged (PSOLA 1.3-2.3 ms/s per voice).

Four-way verdict recorded for the D3 gate (vowel inputs, 110 Hz-floor true transient latency): window engine = lowest latency at high input pitch but 28-30% AM warble, HNR as low as 2-12 dB when shifting, formants scale with ratio, breaks below ratio 0.25; PSOLA A' = cleanest reference, 24.2 ms; B75 = same quality, 21.9 ms; **B50 = recommended live default, 19.7 ms, quality within bar** (fallback to B75 if the ear test hears roughness). Plan A (0.3.3) remains rejected (content-age flaw).

Build status:

- Release `VoxChordShifterCompare` built and run by agent; no new warnings.

User verification pending:

1. Approve B50 (or B75) as the PSOLA configuration to wire into the plugin for the A/B listening gate (PLAN-0.3 gate 2).
2. Listening checks remain impossible until plugin integration (next iteration).

## 0.3.4 D3: PSOLA placement bound P/2 fix (directions/0708_2, plan A')

Date: 2026-07-08

Scope: Branch `exp/d3-psola`. Implements directions/0708_2.md plan A': placement loop bound `newestPlaceable + P` -> `newestPlaceable + P/2` and `latencySamples = 2H + ceil(Pmax/2) + 64`, removing the ~1-period content age introduced in 0.3.3. Buffer latency rises (110 Hz floor: 19.6 -> 24.2 ms) but the *true transient* latency falls (~27 -> ~24 ms) and unison returns to (near-)exact reconstruction. Plugin DSP untouched. VERSION `0.3.3` -> `0.3.4`.

Changes: `Source/PsolaShifter.cpp` (2 spots) + header latency comment. Release harness rebuilt and rerun by agent.

Acceptance criteria vs directions/0708_2:

- sine440 unison SNR: 27.2 -> **29.0/29.1 dB**. Improved as predicted but ~1 dB short of the >=30 dB bar (0.3.2 was 30.7; sine110 similarly 45.5 vs 47.0). Residual gap attributed to integer-sample mark placement (sub-sample mark refinement would be a separate quality item, unrelated to latency). Partially met.
- Measured latency (sine unison, psolaLL): 8.7/16.0/30.5 ms vs configured 8.6/15.7/29.9 ms -> within +/-2 ms. Met. (Vowel-run latency readings remain +/-4 ms noise, as in 0.3.3.)
- Vowel quality at 0.3.2 levels: HNR within +/-0.3 dB, AM within +/-0.2 pt, f0/sd within +/-0.1 c across both vowels. Met.
- Deep downshift: 55 Hz run still -0.0 c / sd 0.2 c. Met.
- 110 Hz floor plugin provisioning: 2*401 + 201 + 64 = 1067 samples = **24.2 ms** configured = true transient latency (content age ~0). Confirmed by formula and by the harness's 88 Hz-headroom run (29.9 ms configured, 30.5 measured).
- CPU back at 0.3.2 levels (engine 58.4/66.5 ms/s incl. detection; PSOLA 1.2-2.0 ms/s per voice), confirming the 0.3.3 CPU spike was background load.

Where this leaves the D3 latency ledger (110 Hz floor, true transient latency): 0.3.2 = ~31 ms -> 0.3.4 = **~24 ms** with quality intact. Reaching ~20 ms still requires plan B (asymmetric grains, ~20 ms, quality re-check) or plan C (min-F0 floor 135 Hz). Engine's own measured latency is 9-23 ms depending on input pitch, so PSOLA at 24 ms costs roughly one 110 Hz period over the engine's low-pitch case.

Build status:

- Release `VoxChordShifterCompare` built and run by agent; no new warnings.

User verification pending:

1. Judge whether ~24 ms wet-path latency (110 Hz floor) is acceptable for live use, or whether plan B/C should be pursued before plugin integration. Recommendation: wire PsolaShifter into the plugin behind an A/B switch next (PLAN-0.3 gate 2 is a listening decision and cannot proceed offline).
2. No listening check possible yet (PsolaShifter still harness-only).

## 0.3.3 D3: PSOLA tight-pipeline scheduling (directions/0708_1, plan A)

Date: 2026-07-08

Scope: Branch `exp/d3-psola`. Implements directions/0708_1.md plan A: `PsolaShifter` scheduling reworked so the constant latency drops from `2H + search + P + 64` to `2H + 64` (at the 110 Hz floor: 31.0 ms -> 19.6 ms configured). Windows, grains and mark refinement are unchanged; plugin DSP untouched (PsolaShifter still harness-only). VERSION `0.3.2` -> `0.3.3`.

Implementation status (all in `Source/PsolaShifter.cpp/.h`):

1. Mark finalization now waits only for the peak-search context (`writePos > guess + searchHalf + 1`); grain extractability is checked at placement.
2. `placeReadyGrains()` uses "placeable" marks (`mark + halfWidth < writePos`); synthesis marks place while `nextSynthMark <= newestPlaceable + P`, choosing the nearest *placeable* mark.
3. `latencySamples = 2 * maxGrainHalfWidth + 64`; header latency comment updated.

Harness re-run vs the 0.3.2 acceptance criteria (Release, agent-built and run):

- Configured latency: all runs shortened as designed (sine110 38.5 -> 24.2 ms with the harness's 0.8*f0 headroom; vowel147 29.2 -> 18.5 ms). Plugin-provisioning formula at exactly the 110 Hz floor: 866 samples = **19.6 ms** (target <= ~20 ms met on paper — but see finding below).
- Measured latency (sine unison, envelope xcorr): 7.3 / 10.9 / 24.7 ms vs configured 7.2 / 12.8 / 24.2 ms -> within +/-2 ms. Vowel-run latency measurements proved unreliable (+/-4 ms; one reading below the configured buffer delay, which is physically impossible), so sine runs are the reference.
- Quality: vowel HNR within +/-1 dB of 0.3.2, AM within +/-1 pt, f0 error/sd within +/-0.3 c, deep-downshift runs (55 Hz) still exact. Only regression: sine 440 unison SNR 30.7 -> 27.2 dB (sine-only metric; vowel HNR stable).
- CPU numbers this run were ~4x higher across the board (engine 220 ms/s vs 56 in 0.3.2) — background machine load during the bench; relative ratios (PSOLA per voice ~1/30 of engine incl. detection) unchanged.

**Structural finding (confirmed by code trace, motivates plan A'):** the eager placement rule makes every synthesis mark use content from ~one input period behind its position at ratio 1.0 (the nearest *placeable* mark is always the previous one). Buffer latency dropped by ~1.25P, but ~1P of *content age* was introduced, so true transient latency (syllable attacks) is ~3P + 64 ~= 27 ms at the 110 Hz floor — the real gain over 0.3.2 is only ~0.25P. Steady tones are unaffected (periodic content is self-similar), which is why sine measurements match the configured value while quality metrics stayed flat. Deriving further: with pitch-synchronous marks the mark-phase quantization adds an unavoidable ~P/2 term, so the true floor of symmetric one-period-grain PSOLA is ~2.5P (~23 ms at 110 Hz), not 2P as 0708_1 assumed. Reaching a true <= 20 ms therefore needs asymmetric grains (plan B) or a higher min-F0 assumption (plan C) on top of a corrected scheduler (plan A': per-grain wait for the nearest extractable mark, L = 2H + P/2 + 64, zero mean content age).

Build status:

- Release `VoxChordShifterCompare` built and run by agent; no new warnings.

User verification pending:

1. Decide the follow-up: accept ~23 ms true latency with plan A' (quality intact), or add plan B (asymmetric grains, ~20 ms, quality to be re-measured), or plan C (raise the min-F0 floor). Draft direction to be proposed.
2. No listening check possible yet (PsolaShifter still not wired into the plugin).

## 0.3.2 D3 experiment: TD-PSOLA shifter + offline A/B comparison (branch exp/d3-psola)

Date: 2026-07-08

Scope: Experimental branch `exp/d3-psola` (off `dev-0.3`). Implements a standalone TD-PSOLA pitch shifter and an offline harness that A/B-compares it against the current windowed dual-tap shifter on quantitative metrics (pitch accuracy/stability, SNR, AM depth, HNR, formant preservation, latency, CPU). **Plugin DSP, parameters, and GUI are unchanged** — `SimpleChoirEngine` is untouched and `PsolaShifter` is not yet wired into the plugin. Project VERSION `0.3.1` -> `0.3.2`.

Implementation status:

- New `Source/PsolaShifter.h/.cpp`: streaming, mono, single-voice TD-PSOLA (analysis marks pitch-synchronous with positive-peak refinement; synthesis marks at outputPeriod spacing; Hann grains OLA'd with window-sum normalization). Allocation-free / lock-free after `prepare()` (realtime-rules compliant), but consumed only by the test harness for now. Constant latency fixed at prepare time: ~2*grainHalfWidth + period + search margin (= ~3.3 input periods in the canonical 1-period-grain mode).
- New CMake console-app target `VoxChordShifterCompare` (`tests/ShifterComparisonMain.cpp`). Drives `SimpleChoirEngine::render()` (1 voice, character off) and two `PsolaShifter` configs from the *same* input and the *same* detected pitch (`getLastDetectedInputFrequencyHz()` feeds PSOLA). Inputs: sines 440/220/110 Hz and a synthetic vowel (impulse train through 700/1200/2600 Hz resonators) at 146.83/293.66 Hz; targets at semitone offsets +12..-36. Latency is measured on dedicated 3 Hz-AM runs by cross-correlating RMS envelopes (waveform cross-correlation is period-ambiguous).
- Both Debug and Release build clean (agent-built); Debug plugin artifacts still produced and unchanged.

Key measured results (Release, 44.1 kHz, block 512; full log in the report):

- **Low-MIDI failure fixed shifter-side**: at ratio 0.125 (440 Hz -> MIDI 33) the engine lands +1197 c (~1 octave high, the D1 ratio clamp); PSOLA lands 55.00 Hz, -0.0 c, sd 0.2 c. PSOLA stays pitch-accurate down to ratio 1/8 with no clamp.
- **Vowel (voice-like) input, the relevant case**: PSOLA beats the engine on every quality metric at shifted notes — AM depth 0.6-2.7% vs 28-30% (engine grain warble), HNR 20-26 dB vs 2-12 dB (147 Hz vowel), pitch sd <=0.2 c vs up to 1.2 c. Dominant spectral peak stays near the input formant (preserved) while the engine scales it by the ratio (chipmunk/monster effect).
- **Sine input caveat**: PSOLA "fails" sines at ratio 2.0 (near-silence + AM) and scores poor sine-fit SNR on downshifts. This is expected spectral-envelope preservation (a sine has no energy at the shifted comb), not a bug; the engine wins all sine SNR comparisons. Sine metrics must not be read as voice quality.
- **Grain-width experiment**: widening grains beyond 1 input period (tried caps 16x, then 2x) re-introduces the source periodicity — at ratio ~0.5 the "quality" config outputs the *input* pitch (+1200 c). Canonical 1-period grains (`grainCap = 1`) are correct at all ratios; adopt that as the PSOLA default.
- **Latency (measured, envelope xcorr)**: engine 9-23 ms depending on input pitch (window-synced taps). PSOLA canonical mode: 10.9 ms @440 Hz input, 20.3 @220, 25.4 @147, 39.2 @110 (≈3.3 input periods; per-run adaptive provisioning). A live worst-case provisioning at the 80 Hz detection floor computes to ~42 ms — above the ~20 ms budget in PLAN-0.3; needs either adaptive latency, a tighter mark-finalization scheme, or a higher min-F0 assumption. This is the main open trade-off for the D3 gate.
- **CPU (Release, per second of audio)**: engine 55.9 ms (1 note) / 62.5 ms (4 notes) — YIN detection dominates; PSOLA 1.2-2.2 ms per voice, i.e. per-voice cost comparable to the windowed shifter's marginal cost (~2.2 ms) and negligible next to detection.

Build status:

- Debug and Release built by agent (VS 18 2026 generator, x64): `VoxChordShifterCompare` both configs, plugin VST3/Standalone Debug rebuilt to confirm no impact. Only pre-existing warnings.

User verification pending:

1. Decide the D3 gate direction from these numbers (PSOLA adoption path vs windowed-engine improvements), especially the latency trade-off above.
2. Optional: run `build/VoxChordShifterCompare_artefacts/Release/VoxChordShifterCompare.exe` to reproduce the tables.
3. No listening check possible yet (PSOLA is not in the plugin); wiring an A/B switch into the engine is the natural next iteration on this branch.

## 0.3.1 D1 diagnostics: offline harness + readable readout

Date: 2026-07-08

Scope: D1 follow-up. Adds an agent-runnable offline harness so the sine + low-MIDI experiment (exp #2) no longer needs a DAW, and relocates the on-screen D1 readout to a legible position. No DSP, parameter, or Release-audio behavior change. Build id `d1-lowpitch-diag-002`; project VERSION `0.3.0` -> `0.3.1`.

Implementation status:

- New CMake console-app target `VoxChordOfflineTest` (`tests/OfflineDiagnosticsMain.cpp` + `Source/SimpleChoirEngine.cpp` + `Source/MidiVoiceState.cpp`, links `juce::juce_audio_basics`). It drives `SimpleChoirEngine::render()` headlessly with a steady 440 Hz sine + one held MIDI note (A5/A4/A3/A2/A0 by MIDI 81/69/57/45/33) at 44.1 kHz / block 512, settles 2 s, then prints the D1 `PitchState` per note. Tooling only; does not touch the plugin build output or any DSP path.
- GUI (Debug only): moved `pitchDebugLabel` out of the cramped 16 px bottom slot (where it was illegible) into the empty band above the mini keyboard, font 12 -> 13.5 bold. Readout reformatted to front-load Ratio (raw->clamped) / Per/Win / Wet Hz (cents), then Note / Grain / Clamp#. Release layout unchanged (band stays empty).
- `SimpleChoirEngine` / detection / shifter / clamp logic unchanged; harness and readout are read-only observers.

Agent build + offline result (this iteration):

- Built Debug and Release (VS 18 2026 generator, x64) — both succeeded, all artifacts produced. Debug VST3 metadata still `VoxChord_dbg`.
- `VoxChordOfflineTest.exe` output (input sine 440 Hz fixed; target = held MIDI note):
  - MIDI 81 (880 Hz): ratio 2.000->2.000, Per/Win 0.08, clamp# 0, wetHz 879.5 (-1 c)
  - MIDI 69 (440 Hz): ratio 1.000->1.000, Per/Win 0.17, clamp# 0, wetHz 440.0 (0 c)
  - MIDI 57 (220 Hz): ratio 0.500->0.500, Per/Win 0.33, clamp# 0, wetHz 219.8 (-2 c)
  - MIDI 45 (110 Hz): ratio 0.250->0.250, Per/Win 0.67, clamp# 172, wetHz 109.8 (-4 c)
  - MIDI 33 (55 Hz): ratio **0.125->0.250 (clamped)**, Per/Win 1.33, clamp# 172, wetHz 109.8 (**+1196 c**, ~1 octave high)

Preliminary reading (sine case only):

- With a fixed 440 Hz input, detection is rock-steady (detIn 440.0) at every target, so for the *low-MIDI-note* scenario the failure is **shifter-side, specifically the ratio clamp**: below ratio 0.25 (targets >2 octaves under the input, e.g. 440->55) the output is clamped and lands ~an octave high. Downward shifts within 0.25..1.0 track to within a few cents. Per/Win climbs monotonically as the target drops (0.08 -> 1.33) and passes 0.67 exactly where the clamp starts biting.
- Caveat: this isolates the shifter (input pitch is trivial to detect). It does **not** exercise low-*input*-pitch detection instability — that is still experiment #3 (real low voice) and remains user-verified.

User verification pending:

1. Confirm the Debug D1 readout is now legible in the band above the mini keyboard (Ratio / Per/Win / Wet visible without clipping), and subtitle shows `Build: d1-lowpitch-diag-002`.
2. Confirm Release GUI is unchanged (no readout, no reserved space) and Release still identifies as `VoxChord`.
3. Real voice + low MIDI (exp #3): sing/hum into the Debug build with descending notes; report where instability begins and the readout values there — to confirm whether real-world low-pitch trouble is the same shifter clamp or adds a detection component.

## 0.3.0 D1 low-pitch instability diagnostics

Date: 2026-07-03

Implementation status:

- Started the `dev-0.3` branch iteration (see `directions/PLAN-0.3.md`); bumped project VERSION to `0.3.0`.
- Extended the Debug pitch-detector self-test (`SimpleChoirEngine::runPitchDetectorSelfTest`) with a second pass covering `50 / 60 / 65 / 70 / 80 / 90 / 100 / 110 / 130 Hz`, run with harmonic correction enabled so activation (`raw/2`, `raw/3`, `raw*2`, `raw*3`, or `none`) is visible per frequency. The original 12-frequency, correction-OFF pass is unchanged.
- Added shifter/window diagnostics to `PitchState`: `windowPitchHz`, the representative (lowest-target active) voice's MIDI note, its current grain window length in samples (`representativeGrainWindowSamples`), its pitch ratio before and after clamping (`representativePitchRatioRaw` / `representativePitchRatioClamped`), and `outputPeriodToWindowRatio = (sampleRate / targetHz) / windowLengthSamples`.
- Added a cumulative `ratioClampHitCount` (atomic on `SimpleChoirEngine`, counts how often a voice's unclamped input-to-target pitch ratio would fall outside `0.25-8.0`). Resets to 0 on Panic (`VoxChordAudioProcessor::processBlock`'s panic branch now calls `choirEngine.resetRatioClampHitCount()`) and on engine `reset()`.
- Added a lightweight positive-going zero-crossing frequency estimator (`updateWetZeroCrossing`) applied to the representative voice's post-Character wet sample each block, exposed as `wetZeroCrossingHz` plus `wetZeroCrossingCentsDeviation` against that voice's MIDI target frequency. Allocation-free; intended to be accurate for the sine-wave self test, not for real vocal input.
- All new diagnostic fields flow through the existing PitchState -> per-field atomic -> `getPitchState()` publishing pattern already used for the other pitch/character diagnostics (no new synchronization primitives beyond the one new atomic counter).
- GUI: split the existing 32px MIDI debug row into two 16px lines. Line 1 keeps the existing MIDI counters unchanged; line 2 is the previously-unused `pitchDebugLabel`, now shown only in Debug builds (`#if JUCE_DEBUG`) with the new diagnostics (`D1 | Note: ... | Win: ... | Grain: ...smp | Ratio: raw->clamped | Per/Win: ... | Clamp#: ... | WetHz: ... (Nc)`). Both labels' font reduced to 12pt to fit the tighter row height.
- Debug subtitle now appends `| Build: d1-lowpitch-diag-001` (Debug builds only); Release subtitle is unchanged (`VoxChord v<version>`).
- Explicitly did not change: pitch detection, harmonic correction logic, grain-window computation, pitch-ratio clamping behavior, or any other DSP path. All new computations are read-only observations recorded alongside the existing (unmodified) logic.

Known discrepancy found during this work (not fixed, flagging for D3):

- `SimplePitchDetector::minFrequencyHz` in code is `80.0f`, not the `70 Hz` nominal floor stated in SPEC.md / AGENTS.md. This means the low-frequency self-test's 70 Hz case (and anything below it) is already below the coded detection floor, which should be accounted for when reading the self-test results.

Build status:

- Not built by agent. User will build.

User verification pending:

1. Low-frequency self-test: enable `enableDebugStartupSelfTests` in `PluginProcessor.cpp` (or otherwise trigger `SimpleChoirEngine::runPitchDetectorSelfTest()`), run a Debug build, and report the `SelfTest(low) ... Hz -> Raw / Corrected / Stable / Confidence / HarmonicCorrection` lines for all 9 low frequencies from the debugger output window.
2. Sine wave + low MIDI: feed a 440 Hz (or similar) sine wave into a Debug build/Standalone, play MIDI notes A4, A3, A2, A1 in turn, and report the second debug line's `Ratio`, `Per/Win`, and `WetHz (Nc)` values plus perceived pitch stability at each note.
3. Real vocal + low MIDI: sing/hum into the same Debug build with descending MIDI notes and report where instability begins along with the same debug-line values at that point.
4. Confirm the Debug subtitle shows `VoxChord v0.3.0 | Build: d1-lowpitch-diag-001` and the new second debug line is visible without visually clipping or overlapping the MIDI counter line above it.
5. Confirm Release build subtitle is unchanged and the second debug line is not shown/does not take up space in Release.

## 0.2.0 Beta initial fixed version

Date: 2026-05-29

Implementation status:

- Fixed VoxChord project version at `0.2.0`.
- Marked this revision as the first beta baseline.
- Preserved the existing beta feature set and behavior; this update only changes version/release records.
- Debug VST3 naming behavior from `0.1.43` remains unchanged: Debug reports `VoxChord_dbg`, Release reports `VoxChord`.

Build status:

- Not rebuilt after the version-freeze metadata update.
- Previous Debug `ALL_BUILD` completed successfully before the version bump.

User verification pending:

- User rebuilds Debug/Release and confirms the GUI/version metadata shows `0.2.0`.

## 0.1.43 Debug plugin name split build fix

Date: 2026-05-29

Implementation status:

- Fixed the MSBuild path error caused by putting a generator expression into `JUCE_PRODUCT_NAME`.
- Debug shared-code builds now define `VoxChord_DEBUG_PLUGIN_NAME=1`.
- `VoxChordAudioProcessor::getName()` returns `VoxChord_dbg` when that Debug define is present.
- Debug VST3 wrapper and VST3 manifest-helper builds force-include `Source/VoxChordDebugPluginName.h`.
- `VoxChordDebugPluginName.h` undefines JUCE's default `JucePlugin_Name` and `JucePlugin_Desc` macros and redefines them as `VoxChord_dbg` for Debug VST3 wrapper/manifest compilation.
- Non-Debug builds continue to use `VoxChord`.
- VST3 artifact/output path remains `VoxChord.vst3`.
- Plugin code/manufacturer code, MIDI capability declarations, DSP behavior, and APVTS parameter IDs were not changed.

Build status:

- Debug build completed successfully after the fix.
- Verified `build/VoxChord_artefacts/Debug/VST3/VoxChord.vst3/Contents/Resources/moduleinfo.json` reports `Name: VoxChord_dbg`.
- Remaining warnings are existing JUCE/Harfbuzz codepage warnings and one existing unused local warning; no build errors.

User verification pending:

- Debug VST3 appears as `VoxChord_dbg` in the host after rebuild/rescan.
- Release VST3 still appears as `VoxChord`.
- Existing MIDI debug counters remain visible in the Debug build.

## 0.1.41 MIDI debug display focus

Date: 2026-05-29

Implementation status:

- Removed the `Build:` build identifier from the Debug subtitle.
- Debug subtitle now shows only `VoxChord v<version>`.
- Temporarily hid Pitch, Last MIDI event, voice-slot, Character, and pitch-test debug text from the debug row.
- Expanded the MIDI debug counter display to use the debug row width.
- Kept the processBlock-level MIDI counter text visible as `MIDI In: blocks <n> | last <n> | total <n> | nonempty <n>`.
- No MIDI processing, DSP behavior, APVTS parameter IDs, or plugin capability declarations were changed.

Build status:

- Not built by agent. User will build Debug/Release.

User verification pending:

- Debug subtitle no longer begins with `Build:`.
- MIDI debug row clearly shows only the MIDI input counters.
- Pitch/Character/self-test debug text no longer obscures the MIDI counters.

## 0.1.40 VST3 MIDI input diagnostics

Date: 2026-05-29

Implementation status:

- Confirmed `CMakeLists.txt` has `NEEDS_MIDI_INPUT TRUE`.
- Confirmed `VoxChordAudioProcessor::acceptsMidi()` returns `true`.
- Existing generated Visual Studio project artifacts also show `JucePlugin_WantsMidiInput=1` and `JucePlugin_VSTNumMidiInputs=16`; those artifacts are from an older configured version and will refresh on the user's next CMake/build step.
- Confirmed MIDI handling runs before audio input copy, pitch detection, voiced/unvoiced checks, and choir rendering.
- Confirmed Standalone/VST3 branching is not used for MIDI handling; wrapper branching is only used for audio input source selection.
- Added processBlock-level MIDI counters independent of Note On/Off interpretation.
- Added GUI MIDI debug text: `MIDI In: blocks <n> | last <n> | total <n> | nonempty <n>`.
- Updated Debug build string to `Build: midi-input-debug-001`.
- No DSP pitch-shift behavior, Character behavior, APVTS parameter IDs, or MIDI voice allocation behavior was changed.

Build status:

- Not built by agent. User will build Debug/Release.

User verification pending:

- In VST3, `blocks` increments while the plugin is being processed.
- In VST3, `last` becomes greater than `0` on blocks where Waveform sends MIDI events.
- In VST3, `total` and `nonempty` increment when MIDI is delivered to `processBlock()`.
- If `blocks` increments but `total` remains `0`, the plugin is processing audio but MIDI is not reaching VoxChord from the host route.
- If `total` increments but note indicators do not change, the issue is inside VoxChord MIDI interpretation/GUI reflection.

## 0.1.39 GUI balance micro-adjustment

Date: 2026-05-28

Implementation status:

- Centered the `Character` title within the Character subcard.
- Centered the Level meter group horizontally inside the Level panel.
- Kept `OutL` and `OutR` directly adjacent as a connected stereo pair.
- Updated Debug build string to `Build: gui-balance-layout-001`.
- No DSP behavior or APVTS parameter behavior was changed.

Build status:

- Not built by agent. User will build Debug/Release.

User verification pending:

- Character title appears at the top center of the Character area.
- Output L/R meters sit slightly farther right than before.
- Level area feels closer to left/right symmetric.

## 0.1.38 GUI entry and live layout refinement

Date: 2026-05-28

Implementation status:

- Applied GUI-only layout and direct-entry refinements.
- Kept DSP behavior, APVTS parameter IDs, Character DSP, pitch shifter, Mono Out DSP, and mini keyboard behavior unchanged.
- Changed header utility button labels from `Lead` / `Mono` to `Auto Tune` / `Mono Out`.
- Shortened the right-side `Input Source` dropdown and `PANIC` button and aligned their right edges.
- Added editable value labels for `Voices`, `Glide`, `Spread`, and `Dry/Wet`.
- Kept editable value labels for `Input Gain`, `Output`, and `Amount`.
- `Voices` direct entry is parsed as an integer; `Glide`, `Spread`, `Dry/Wet`, and `Amount` are parsed as percentages.
- Reduced the Character `Type` dropdown height and increased the available `Amount` rotary area.
- Hid the compact MIDI `Notes` count from the top MIDI status row.
- Increased MIDI and Level panel height by reducing the Harmony panel height slightly.
- Brought MIDI and Level panels closer horizontally so MIDI extends farther right and Level starts farther left.
- Made Level meters narrower/taller and placed `OutL` / `OutR` directly adjacent.
- Updated Debug build string to `Build: gui-entry-layout-001`.

Build status:

- Not built by agent. User will build Debug/Release.

User verification pending:

- Header shows `Auto Tune`, `Input Source`, `Mono Out`, and `PANIC` without overlap.
- `Input Source` and `PANIC` are shorter and their right edges align.
- `Voices`, `Glide`, `Spread`, and `Dry/Wet` accept direct numeric entry.
- Character Amount knob is visibly larger than before.
- MIDI top status row no longer shows the compact input-note count.
- MIDI and Level panels feel taller and less cramped.
- Level meters are narrower/taller, with `OutL` and `OutR` connected horizontally.
- Existing DSP behavior is unchanged.

## 0.1.37 GUI micro layout adjustments

Date: 2026-05-27

Implementation status:

- Applied GUI-only micro-adjustments from `directions/0527_1.md`.
- Kept DSP behavior, APVTS parameter IDs, Character DSP, pitch shifter, Mono Out DSP, and mini keyboard behavior unchanged.
- Rearranged the header utility controls so `Lead` and `Input Source` share the top row, while `Mono` and a wide `PANIC` button share the second row.
- Shortened visible button labels from `Lead Tune` / `Mono Out` to `Lead` / `Mono` for compact live-layout readability.
- Added a visible `Character` title inside the Character subcard.
- Renamed the Character mode label from `Char Type` to `Type`.
- Enlarged the Character subcard and Character Amount knob area.
- Moved level meter dB values below the meter fill area and made the text slightly more readable.
- Slightly increased non-logo GUI label and section title font sizes.
- Updated Debug build string to `Build: gui-micro-layout-001`.

Build status:

- Not built by agent. User will build Debug/Release.

User verification pending:

- Header upper-right shows `Lead` and `Input Source` naturally on the top row.
- Header upper-right shows `Mono` and wide `PANIC` naturally on the second row.
- `Input Source` dropdown remains visible and usable.
- Character subcard clearly reads as `Character`, with `Type` and `Amount` labels.
- Character Amount knob is visibly larger than before.
- Input/Output meter dB values no longer overlap the meter fill.
- Existing DSP behavior is unchanged.

## 0.1.36 final GUI controls and direct numeric entry

Date: 2026-05-25

Implementation status:

- Corrected GUI layout responsibilities without changing DSP behavior.
- Split the header into `LogoArea`, `GainArea`, and `UtilityArea`.
- Used the center header space as `GainArea` for larger horizontal `Input Gain` and `Output` sliders.
- Added editable value labels for `Input Gain` and `Output`.
- Placed `Input Source`, `Lead Tune`, `Mono Out`, and a large horizontal `PANIC` button in `UtilityArea`.
- Displayed Character as a Harmony subcard that groups Character Type and Character Amount.
- Enlarged the Character Amount control area and added an editable percent value label.
- Direct numeric entry targets `Input Gain`, `Output`, and `Character Amount`.
- dB input accepts numeric text with optional `dB`; percent input accepts numeric text with optional `%`.
- Invalid numeric input reverts to the current APVTS value without changing the parameter.
- Out-of-range numeric input is clamped through the existing APVTS parameter ranges.
- Numeric edits write to the existing APVTS parameters; no parameter IDs were changed.
- Updated Debug build string to `Build: gui-final-controls-001`.

Build status:

- Not built by agent. User will build Debug/Release.

User verification pending:

- Header is visibly split into LogoArea / GainArea / UtilityArea.
- VoxChord logo right-side center space is used by the larger Input Gain / Output controls.
- Input Gain / Output values are readable and editable directly.
- UtilityArea contains Input Source, Lead Tune, Mono Out, and a large PANIC button.
- Character appears as one subcard containing Type and Amount.
- Character Amount is easier to operate and editable directly.
- Numeric invalid input does not change parameter values.
- Numeric out-of-range input clamps safely.
- Enter / Esc / focus lost do not break editing behavior.
- Existing DSP behavior is unchanged.

## 0.1.35 GUI responsibility and sizing correction

Date: 2026-05-25

Implementation status:

- Corrected GUI layout responsibilities without changing DSP behavior.
- Removed Pitch, Last, and Active/Notes display from the header.
- Moved Pitch, Last, and Notes into the MIDI panel.
- Moved Input Gain and Output Gain from the Level panel into the header controls area.
- Kept Input Source, Lead Tune, Mono Out, and PANIC in the header controls area.
- Enlarged the PANIC button in the header.
- Made the Level panel meter-focused and enlarged the vertical input/output meters.
- Widened the Character sub-area and enlarged the Character Amount control.
- Changed MIDI note count label to `Notes`.
- Updated Debug build string to `Build: gui-responsibility-fix-001`.

Build status:

- Not built by agent. User will build Debug/Release.

User verification pending:

- Header no longer duplicates Pitch / Last / Notes information.
- MIDI panel clearly shows Pitch, Last, and Notes.
- Header contains Input Gain, Output, Input Source, Lead Tune, Mono Out, and a larger PANIC button.
- Level panel is meter-focused and meters are larger.
- Character Amount is easier to operate than before.
- Existing DSP behavior is unchanged.

## 0.1.34 GUI layout correction

Date: 2026-05-25

Implementation status:

- Corrected the GUI layout without changing DSP behavior.
- Split the header into LogoArea and HeaderControlsArea.
- Moved Input Source, Lead Tune, Mono Out, PANIC, Pitch, Last, and Active note count into the header controls area.
- Removed the previous right-column Input / Lead, Level / Output, and Status panel split from the main layout.
- Expanded Harmony to use the full main width.
- Split the bottom area into a wide MIDI panel and a right-side Level panel.
- Grouped Input Gain, Output Gain, and Input/Output meters in the Level panel.
- Fixed Mini Keyboard black-key drawing so only C#, D#, F#, G#, and A# are drawn, positioned relative to white keys.
- Updated Debug build string to `Build: gui-layout-fix-001`.

Build status:

- Not built by agent. User will build Debug/Release.

User verification pending:

- VoxChord logo right-side dead space is now used by controls/status.
- Header shows Lead Tune, Mono Out, PANIC, Pitch, Last, and Active note count.
- Harmony panel uses the horizontal space cleanly.
- Input/Output labels no longer overlap nearby controls.
- Input/Output meters are visible in the Level panel.
- Mini Keyboard no longer shows incorrectly spaced black-key patterns.
- Release UI remains readable.
- Existing DSP behavior is unchanged.

## 0.1.33 GUI layout, Mono Out, and mini MIDI keyboard

Date: 2026-05-25

Implementation status:

- Reorganized the GUI into Header, Harmony, Input / Lead, Level / Output, Status, and bottom MIDI/status areas.
- Grouped Character Type and Character Amount into one Character area.
- Added `monoOutputEnabled` APVTS bool parameter with GUI label `Mono Out`.
- Added final-stage Mono Out DSP: `0.5 * (left + right)` copied to L/R.
- Smoothed Mono Out switching over approximately `12 ms`.
- Expanded meter publishing to input peak plus output L/R peaks.
- Replaced horizontal meters with vertical meters.
- Added stereo output meter display for L/R; Mono Out displays matching L/R mono values after smoothing.
- Added display-only C2-C6 mini MIDI keyboard highlighting active MIDI notes.
- Kept Input Source on the main GUI.
- Did not implement physical output channel selection, output 3/4 routing, keyboard click input, variable keyboard range, or Options-page device settings.

Build status:

- Not built by agent. User will build Debug/Release.

User verification pending:

- Output meter is fully visible.
- Input and output meters are readable as vertical meters.
- Mono Out Off preserves stereo output.
- Mono Out On makes L/R the same mono signal without a large level jump.
- Mono Out switching does not create an obvious click.
- Mini keyboard highlights active MIDI notes from C2-C6.
- Existing Voice 8, Tuned Lead, Character, Input Gain, and input-synced pitch shifting still behave as before.

## 0.1.32 Character EQ redesign

Date: 2026-05-25

Implementation status:

- Reworked Character DSP from one-pole high/mid difference shaping into lightweight per-voice biquad EQ.
- Added reusable `CharacterBiquad` filter state to each harmony voice.
- Warm: high-shelf cut, low-mid boost, and very light soft saturation.
- Bright: high-shelf boost, presence boost, and low-mid cleanup.
- Vowel: slot-dependent formant-ish peaking EQ with mixed boost/cut centers.
- Digital: high/presence boost plus light soft saturation.
- Character Amount still acts as a clean-to-character blend and remains smoothed over `20 ms`.
- Character Type changes reset per-voice Character filter states.
- Added Debug Character relative delta display as `CharDelta rms/pk/rel`, with `rel` in dB.
- Did not change pitch ratio correction, pitch shifter design, input-synced window behavior, Voice 8, Tuned Lead, or the Character Type list.

Build status:

- Not built by agent. User will build Debug/Release.

User verification pending:

- Warm 100% sounds rounder/thicker and produces visible non-zero Character delta.
- Bright 100% sounds clearer/forward without excessive harshness.
- Vowel 100% gives a clear formant-ish voice-color change.
- Digital 100% sounds harder/artificial without excessive pitch instability.
- Amount 0% remains effectively Clean for all Character Types.
- Amount 50% is a usable intermediate color.
- Voice 8 + Character 100% does not break the output.
- Tuned Lead + Character 100% does not break the output.
- Character operation does not add obvious clicks.

## 0.1.31 Character Type internal mode mapping

Date: 2026-05-20

Implementation status:

- Fixed Character Type GUI-to-DSP internal mode mapping.
- Confirmed the root cause in JUCE: `ComboBoxParameterAttachment` maps by selected item index, not ComboBox item ID.
- The previous 4-item GUI attached to a 5-choice APVTS parameter produced APVTS values `0`, `1`, `3`, and `4` for Warm, Bright, Vowel, and Digital.
- Changed APVTS `characterMode` choices to the visible 4 choices only: Warm, Bright, Vowel, Digital.
- Added centralized conversion function `voxchord::characterModeGuiIndexToInternalMode()`.
- Internal DSP mapping is now `Warm -> 1`, `Bright -> 2`, `Vowel -> 3`, `Digital -> 4`.
- Updated `SimpleChoirEngine::sanitizeCharacterMode()` to clamp internal modes to `1-4`.
- Updated Debug build string to `Build: character-mode-map-001`.

Build status:

- Not built by agent. User will build Debug/Release.

User verification pending:

- Debug Character mode display shows `CharMode internal/safe` as Warm `1/1`, Bright `2/2`, Vowel `3/3`, Digital `4/4`.
- Warm at Amount `100%` produces non-zero `CharDelta` while MIDI harmony voices are sounding.
- Bright enters the intended Bright tone path, `applyCharacterTone()` `case 2`.

## 0.1.30 Compact Character debug display

Date: 2026-05-20

Implementation status:

- Hid Debug GUI pitch runtime detail fields by default.
- Added `showDebugPitchRuntimeDetails = false` in `PluginEditor.cpp` as the restore point.
- Hidden pitch fields: RMS, Raw, Corr, Disp, RatioIn, Conf, Voiced, Fix, RatioSmooth.
- Character diagnostics remain visible in the Debug subtitle.
- Self-test summary display remains hidden by default.

Build status:

- Not built by agent. User will build Debug/Release.

User verification pending:

- Debug subtitle is no longer filled by Raw/Corr and related pitch diagnostic fields.
- Character diagnostic fields are visible without being pushed out of view.
- `showDebugPitchRuntimeDetails = true` restores the pitch runtime details if needed later.

## 0.1.29 Debug self-test quiet mode

Date: 2026-05-20

Implementation status:

- Disabled Debug startup execution of `runPitchDetectorSelfTest()` and `runPitchShifterSelfTest()` by default.
- Left both self-test functions in the codebase for future restoration.
- Added `enableDebugStartupSelfTests = false` in `PluginProcessor.cpp` as the restore point.
- Disabled Debug GUI display of the Pitch Shifter SelfTest summary by default.
- Added `showDebugSelfTestSummary = false` in `PluginEditor.cpp` as the restore point.
- Runtime pitch and Character diagnostics remain visible in Debug builds.

Build status:

- Not built by agent. User will build Debug/Release.

User verification pending:

- Debug Standalone/VST3 startup no longer runs the PitchDetector/PitchShifter self-test DBG spam.
- Debug GUI subtitle no longer shows the Pitch Shifter SelfTest summary.
- Runtime pitch and Character diagnostics remain visible.

## 0.1.28 Character signal-path diagnostics

Date: 2026-05-20

Implementation status:

- Added Debug GUI diagnostics for Character signal-path verification.
- Debug subtitle now shows `CharMode raw/safe`, `CharAmt raw/sm`, and `CharDelta rms/pk`.
- `CharAmt raw` comes from APVTS parameter ID `character`.
- `CharAmt sm` is the processor-smoothed value passed to `SimpleChoirEngine`.
- `CharMode raw` comes from APVTS parameter ID `characterMode`.
- `CharMode safe` is the sanitized internal mode used by DSP.
- `CharDelta rms/pk` measures the difference between pre-character and post-character harmony voice samples inside the wet harmony render path.
- Character is currently specified to affect harmony voices only; it does not process Dry or Tuned Lead.

Build status:

- Not built by agent. User will build Debug/Release.

Debug test recommendation:

- Set `Dry/Wet = 100% Wet`.
- Set `Lead Tune = Off`.
- Set `Voice Count = 4` or `8`.
- Set `Character Type = Vowel`.
- Compare `Character Amount = 0%` and `100%`.
- At `0%`, `CharAmt raw/sm` should approach `0.00/0.00`, and `CharDelta rms/pk` should approach zero.
- At `100%`, `CharAmt raw/sm` should approach `1.00/1.00`, and `CharDelta rms/pk` should become non-zero while MIDI harmony voices are sounding.

## 0.1.27 Stronger Character coloration

Date: 2026-05-19

Implementation status:

- Strengthened Character Amount 100% tone coloration for Warm, Bright, Vowel, and Digital.
- Warm now applies stronger high attenuation and low warmth.
- Bright now applies stronger high emphasis.
- Vowel now uses per-slot mid coefficients across 8 voices for clearer variation.
- Digital now applies stronger high and mid emphasis.
- Character Amount tone blend uses `pow(amount, 1.2)` so low values stay controlled and 100% is clearer.
- Pitch detune, pitch ratio behavior, delay offset, and gain variation were not strengthened in this pass.
- Existing Voice 8, Tuned Lead, input-synced window, Glide, and Input Gain behavior are intended to remain unchanged.

Build status:

- Not built by agent. User will build Debug/Release.

User verification pending:

- Character Amount 0% remains effectively Clean for all Types.
- Character Amount 50% gives usable moderate coloration.
- Character Amount 100% makes Warm / Bright / Vowel / Digital clearly distinguishable.
- Warm 100% sounds rounder/warmer without becoming too muffled.
- Bright 100% sounds clearer/brighter without becoming painfully sharp.
- Vowel 100% makes vowel-like mid coloration easier to perceive.
- Digital 100% sounds more artificial/bright without making pitch feel unstable.
- Amount and Type changes do not add obvious clicks.
- Voice 8 + Character 100% and Tuned Lead + Character 100% do not break existing behavior.

## 0.1.26 Character Type plus Amount

Date: 2026-05-19

Implementation status:

- Split Character control into `Char Type` and `Amount`.
- `Char Type` uses existing `characterMode` with GUI choices: Warm, Bright, Vowel, Digital.
- `Formant-ish` display text was removed and renamed to `Vowel`.
- `Amount` reuses the existing `character` parameter ID; no new `characterAmount` ID was added.
- `Amount` range is `0.0-1.0`, displayed as 0-100%, default `0.0`.
- Amount scales Character pitch detune, per-slot gain variation, delay offset, and tone shaping.
- Amount `0%` should be effectively Clean regardless of selected Character Type.
- Amount is smoothed at the processor level over about `20 ms`.
- Existing input-synced window, Tuned Lead, Voice 8, and MIDI transition de-click behavior are intended to remain unchanged.

Build status:

- Not built by agent. User will build Debug/Release.

User verification pending:

- Character Type dropdown shows Warm / Bright / Vowel / Digital, with no visible Formant-ish entry.
- Character Amount knob is visible and changes from 0% to 100%.
- Character Amount 0% behaves like Clean.
- Character Amount 50% gives an intermediate coloration.
- Character Amount 100% gives the strongest selected Character Type effect.
- Warm, Bright, Vowel, and Digital all respond to Amount.
- Amount and Type changes do not create obvious clicks.
- Digital and Vowel do not make pitch accuracy feel unstable.
- Voice 8 and Tuned Lead still work together with Character enabled.

## 0.1.25 Priority B tuned lead / 8 voices / character modes

Date: 2026-05-19

Implementation status:

- Added `leadTuneEnabled` parameter and GUI toggle.
- Lead Tune uses nearest chromatic pitch correction from `correctionInputPitchHz`.
- Lead Tune replaces the dry side when enabled; original dry remains the dry side when disabled.
- Lead Tune falls back/crossfades to original dry when input pitch is unvoiced or invalid.
- Lead Tune dry-source switching is smoothed to reduce on/off clicks.
- Extended MIDI harmony voice capacity and `voiceCount` range from 4 to 8 voices.
- Added `characterMode` choice parameter and GUI dropdown: Clean, Warm, Bright, Formant-ish, Digital.
- Existing `character` parameter remains for compatibility but is not the visible primary control.
- Existing input-synced pitch shifter, window continuity, MIDI transition de-click, Input Gain, meters, and self-test summaries are intended to remain unchanged.

Build status:

- Not built by agent. User will build Debug/Release.

User verification pending:

- Standalone/VST3 open without parameter-state issues after adding `leadTuneEnabled` and `characterMode`.
- `Voice Count` can select up to 8 and displays/plays up to 8 MIDI harmony slots.
- `Lead Tune` off: Dry/Wet dry side remains original input.
- `Lead Tune` on and Dry/Wet 0%: dry side becomes chromatically tuned lead.
- `Lead Tune` on with invalid/unvoiced pitch: output safely falls back toward original dry without clicks.
- Character dropdown modes produce audible but lightweight differences without breaking pitch accuracy.
- Fast MIDI note changes remain de-clicked.

VoxChord の各バージョンで確認した動作を記録する。

## 0.1.0 scaffold

日付: 2026-05-17

対象ビルド:

- Debug Standalone: `build-vs18/VoxChord_artefacts/Debug/Standalone/VoxChord.exe`
- Debug VST3: `build-vs18/VoxChord_artefacts/Debug/VST3/VoxChord.vst3`

確認済み:

- Standalone 版で入力音声がスルーされることを確認した。
- Standalone 版で MIDI note indicator が正しく表示されることを確認した。
- Standalone 版で GUI から各パラメータを変更できることを確認した。
- VST3 版でも Standalone 版と同様に、入力音声スルー、MIDI note indicator、GUI パラメータ変更が動作することを確認した。
- 現時点で実際に出力音へ反映されるパラメータは `outputLevel` のみである。

想定通りの未実装:

- `tune` / `glide` / `character` / `spread` / `dryWet` は、まだ DSP に接続されていない。
- `dryWet` は wet engine 未実装のため、音声スルー状態では聴感上の変化を持たない。
- 高品質または簡易の pitch shifter は未実装。

未確認:

- Release Standalone の実行確認。
- Release VST3 の DAW 認識確認。
- Panic / All Notes Off の実機確認。
- MIDI stuck からの復帰確認。
- 長時間動作時の安定性。

## 0.1.1 phase 2 voice-state visibility

日付: 2026-05-17

対象ビルド:

- Debug Standalone: `build-vs18/VoxChord_artefacts/Debug/Standalone/VoxChord.exe`
- Debug VST3: `build-vs18/VoxChord_artefacts/Debug/VST3/VoxChord.vst3`
- Release Standalone: `build-vs18/VoxChord_artefacts/Release/Standalone/VoxChord.exe`
- Release VST3: `build-vs18/VoxChord_artefacts/Release/VST3/VoxChord.vst3`

実装済み:

- GUI に voice slot 表示を追加した。
- `voiceCount` によって無効化される slot を `off` として表示するようにした。
- 最後に受信した MIDI 関連イベントを `Last:` として表示するようにした。
- Panic button 押下時に `Last: Panic` が表示されるようにした。
- MIDI の Note On / Note Off / All Notes Off / All Sound Off / Reset Controllers を表示・処理対象にした。
- `outputLevel` に 20 ms の smoothing を追加し、音量変更時のクリックを減らす準備をした。

ビルド確認:

- Debug build 成功。
- Release build 成功。
- Debug VST3 生成成功。
- Debug Standalone 生成成功。
- Release VST3 生成成功。
- Release Standalone 生成成功。

ユーザー確認済み:

- Standalone / VST3 版で voice slot 表示が Note On / Note Off に追従することを確認した。
- Standalone / VST3 版で `voiceCount` を下げたとき、余剰 slot が `off` 表示になり、余剰 note が消えることを確認した。
- Standalone / VST3 版で Panic button 押下後に active note / slot が解除されることを確認した。

ユーザー確認待ち:

- Standalone 版で MIDI All Notes Off を受けたとき、`Last: All Notes Off` と表示され、active note / slot が解除されること。
- `outputLevel` 変更時に以前より音量変化が滑らかに感じられること。

想定通りの未実装:

- wet engine は未実装。
- `tune` / `glide` / `character` / `spread` / `dryWet` は、まだ出力音には反映されない。

## 0.1.2 phase 3A MIDI-gated wet bus

日付: 2026-05-17

対象ビルド:

- Debug Standalone: ユーザー確認予定。
- Debug VST3: ユーザー確認予定。
- Release Standalone: ユーザー確認予定。
- Release VST3: ユーザー確認予定。

実装済み:

- `SimpleChoirEngine` を追加した。
- wet bus 用の `dryBuffer` / `wetBuffer` を `prepareToPlay()` で事前確保するようにした。
- MIDI active slot があるときだけ wet voice を生成するようにした。
- 最初の wet voice はピッチシフトなしの入力音声コピーとして実装した。
- `dryWet` を実際の dry/wet mix に接続した。
- `spread` を wet voice の左右配置に接続した。
- `voiceCount` / Note Off / Panic による active slot 解除が wet voice 停止にも反映される構成にした。
- 追加 latency は 0 samples のまま。

ユーザー確認済み:

- MIDI note がない状態で `dryWet = 100%` にすると wet が無音になることを確認した。
- MIDI note がある状態で `dryWet = 100%` にすると入力音声コピーの wet が鳴ることを確認した。
- `dryWet = 0%` で従来通り dry のみになることを確認した。
- Note Off / Panic で wet voice が止まることを確認した。

ユーザー確認結果メモ:

- `dryWet` の中間値による混ざり方は、現状 dry と wet が同じ入力音声コピーのため聴感上は判別できなかった。
- `spread` による左右配置の変化は、現状 dry と wet が同じ入力音声コピーのため聴感上は判別できなかった。
- `voiceCount` による wet voice 数の変化は、現状 dry と wet が同じ入力音声コピーのため聴感上は判別できなかった。

想定通りの未実装:

- MIDI note の音程に合わせた pitch shift は未実装。
- `tune` / `glide` / `character` は、まだ出力音には反映されない。

## 0.1.3 phase 3B simple pitch shifter

日付: 2026-05-17

対象ビルド:

- Debug Standalone: ユーザー確認済み。
- Debug VST3: ユーザー確認済み。
- Release Standalone: ユーザー確認済み。
- Release VST3: ユーザー確認済み。

実装済み:

- `SimpleChoirEngine` に短い delay buffer と dual-window 読み出しによる簡易 pitch shift を追加した。
- 入力 pitch detection はまだ使わず、固定基準 pitch を C4 とした。
- MIDI note 周波数 / C4 を pitch ratio として wet voice の読み出しに反映した。
- pitch ratio は 0.25x - 4.0x に制限した。
- voice ごとに pitch ratio を 30 ms smoothing するようにした。
- `dryWet = 100%` で pitch shifted wet のみを確認できる構成にした。
- `spread` は引き続き wet voice の左右配置に反映される。
- Note Off / Panic による wet voice 停止は維持した。
- ホストへ報告する latency は 0 samples のまま。

ユーザー確認済み:

- MIDI note がない状態で `dryWet = 100%` にすると wet が無音になることを確認した。
- MIDI note がある状態で `dryWet = 100%` にすると pitch shifted wet が鳴ることを確認した。
- C4 付近の MIDI note で、wet が入力音に近い高さに聞こえることを確認した。
- C3 付近の MIDI note で、wet が低く聞こえることを確認した。
- C5 付近の MIDI note で、wet が高く聞こえることを確認した。
- 複数 MIDI note で複数 pitch の wet が重なることを確認した。
- `spread` を上げると複数 wet voice の左右配置が広がることを確認した。
- Note Off / Panic で wet voice が止まることを確認した。

想定される制限:

- まだ入力 pitch detection がないため、入力ボーカルの実際の音高には追従しない。
- 音質は確認用であり、クリック、金属感、グレイン感、濁りが出る可能性がある。
- pitch shift の内部 window により wet はわずかに遅れて聞こえる可能性があるが、ホスト報告 latency は 0 samples のまま。
- `tune` / `glide` / `character` は、まだ出力音には反映されない。

## 0.1.4 phase 3C pitch control parameters

日付: 2026-05-17

対象ビルド:

- Debug Standalone: ユーザー確認予定
- Debug VST3: ユーザー確認予定
- Release Standalone: ユーザー確認予定
- Release VST3: ユーザー確認予定

実装済み:

- `tune` を簡易 pitch shift の MIDI note 追従量に接続した。
- `glide` を voice ごとの pitch ratio smoothing に接続した。
- `character` を voice ごとの微小 detune / gain 差に接続した。
- 既存の `voiceCount` / `spread` / `dryWet` / `outputLevel` / Note Off / Panic の構造は維持した。
- 入力 pitch detection はまだ使わず、引き続き C4 固定基準の簡易 pitch shift とした。
- ホストへ報告する latency は 0 samples のままとした。

ユーザー確認待ち:

- `character = 0%` にしたうえで、`tune = 0%` では wet の pitch shift 量が小さくなること。
- `tune = 100%` で MIDI note に応じた pitch shift 量が強くなること。
- `voiceCount = 1` で前の note を押したまま次の note を押したとき、`glide` を上げるほど pitch 移動が遅くなること。
- `character = 0%` と `character = 100%` で、複数 voice の揺れ・厚み・音量差が変わること。
- 既存確認済みの dry/wet、spread、voiceCount、Note Off、Panic が破綻していないこと。

想定される制限:

- まだ入力 vocal の実 pitch には追従しない。
- pitch shifter は確認用の簡易方式であり、クリック、金属感、grain 感が残る可能性がある。
- glide は同一 voice slot の note 変更で最も分かりやすいため、確認時は `voiceCount = 1` の legato 操作を推奨する。

## 0.1.5 phase 3D stronger character spread

日付: 2026-05-17

対象ビルド:

- Debug Standalone: ユーザー確認予定
- Debug VST3: ユーザー確認予定
- Release Standalone: ユーザー確認予定
- Release VST3: ユーザー確認予定

実装済み:

- `character` の voice ごとの detune 幅を増やした。
- `character` に voice ごとの追加 delay offset を接続した。
- 追加 delay offset は既存の delay buffer 読み出し位置をずらす方式とし、新規バッファは追加していない。
- ホストへ報告する latency は 0 samples のままとした。
- `tune` / `glide` / `spread` / `dryWet` / `voiceCount` の既存接続は維持した。

ユーザー確認待ち:

- 複数 MIDI note / 複数 voice で `character = 0%` と `character = 100%` の差が以前より分かりやすいこと。
- `character = 100%` で wet に軽い厚み、ズレ、揺れ、濁りが加わること。
- `character` を上げても音量が極端に上がらないこと。
- Note Off / Panic で character 付き wet voice も停止すること。

想定される制限:

- `character` は自然な声質変換ではなく、現段階では detune / delay による個体差の付与である。
- `character` を上げると意図的に濁りや位相差が増えるため、設定によっては rough に聞こえる可能性がある。

ユーザー確認済み:

- `character` による voice ごとの delay offset / detune 増量が正しく動作することを確認した。
- `tune` / `glide` の正常な変化、およびその他パラメータへの追従が維持されていることを確認した。

追加観測:

- C5 より上の MIDI note が入力されたとき、note 表示は入力 MIDI 通りだが、実際の wet 出力音高が C5 相当で頭打ちになる現象を確認した。
- 現行実装では C4 固定基準に対して pitch ratio を 0.25x - 4.0x に制限しているため、少なくとも C5 より上も変化する想定であり、この頭打ちは仕様ではなく簡易 pitch shifter の制限または不具合として扱う。

次に確認 / 修正すべきこと:

- MIDI note から計算した pitch ratio と実際の聴感上の pitch が C5 以上で一致しているかを切り分ける。
- 現行 dual-window delay 読み出し方式が高い pitch ratio で破綻または折り返していないか確認する。
- 必要に応じて一時的に pitch ratio の安全上限を C5 相当に制限するか、高音域でも追従する方式へ差し替える。

調査結果:

- GUI の note 表示は `juce::MidiMessage::getMidiNoteName(note, true, true, 3)` を使っているため、MIDI note 60 が `C3` と表示される。
- DSP の neutral 基準 `referenceFrequencyHz = 261.625565f` は MIDI note 60 の周波数であり、GUI 表示上は `C3` に対応する。
- したがって「wet 音が入力と同じ音高になるのが C3」という観測は、DSP 基準と GUI 表示の対応として正しい。
- GUI 表示上の `C5` は MIDI note 84 で、MIDI note 60 の 4 倍周波数に相当する。
- 現行実装は `maxPitchRatio = 4.0f` で上限 clamp しているため、GUI 表示上の C5 より上が C5 相当に頭打ちになる。
- 原因は pitch shifter の偶発的な破綻ではなく、octave 表示規約と `maxPitchRatio` 上限の組み合わせである可能性が高い。

修正候補:

- GUI / ドキュメント上は「C3 表示 = MIDI note 60」を明記し、現行の有効上限を GUI 表示 C5 までとする。
- あるいは `maxPitchRatio` を 8.0f まで拡張し、GUI 表示 C6 まで追従できるか検証する。ただし高い pitch ratio ほど簡易 pitch shifter の音質劣化リスクが高い。
- より根本的には、固定基準ではなく入力 pitch detection を入れて、入力 vocal pitch から MIDI target への比率を計算する。

## 0.1.6 phase 3E pitch reference and detector entry

日付: 2026-05-18

対象ビルド:

- Debug Standalone: ユーザー確認予定
- Debug VST3: ユーザー確認予定
- Release Standalone: ユーザー確認予定
- Release VST3: ユーザー確認予定

実装済み:

- GUI subtitle に `C3 = MIDI 60` を表示し、現行の octave 表示基準を明示した。
- `maxPitchRatio` を `4.0x` から `8.0x` に拡張した。
- `SimpleChoirEngine` に簡易 pitch detector の入口を追加した。
- pitch detector は Schmitt trigger 風の zero-crossing 検出で、入力 mono signal の概算 frequency を block ごとに更新する。
- 入力 pitch を検出できた場合は `MIDI target frequency / detected input frequency` を pitch ratio として使う。
- 入力 pitch を検出できない場合は従来通り MIDI note 60 / 261.625565 Hz を fallback reference として使う。
- 追加のファイル I/O、processBlock 内の動的メモリ確保、ホスト報告 latency の変更は行っていない。

ユーザー確認待ち:

- GUI subtitle に `C3 = MIDI 60` が表示されること。
- C5 より上の MIDI note で wet pitch が以前より上へ追従すること。
- 入力 vocal の音高を変えたとき、同じ MIDI note でも wet の変換量が変わること。
- 無音時または pitch 検出不能時に、従来の C3 表示基準 fallback として動作すること。
- `tune` / `glide` / `character` / `spread` / `dryWet` / `voiceCount` / Panic が破綻していないこと。

想定される制限:

- pitch detector は現段階では確認用の簡易方式であり、息成分、倍音、ビブラート、子音、ノイズで誤検出する可能性がある。
- 高音域は `maxPitchRatio = 8.0x` まで許可したが、簡易 pitch shifter の音質劣化や grain 感が増える可能性がある。
- 検出 pitch は GUI にはまだ表示していない。

## 0.1.7 phase 3F pitch debug visibility

日付: 2026-05-18

対象ビルド:

- Debug Standalone: ユーザー確認予定
- Debug VST3: ユーザー確認予定
- Release Standalone: ユーザー確認予定
- Release VST3: ユーザー確認予定

ユーザー確認結果:

- C5 より上の MIDI note では、引き続き C5 相当の wet pitch が出力される。
- 入力 vocal pitch に対して wet pitch が追従している様子は確認できなかった。

再確認した実装状態:

- コード上の `maxPitchRatio` は `8.0f` に拡張済みであり、計算上は GUI 表示 C5 より上も許可している。
- pitch ratio 計算は、検出 pitch が 0 より大きい場合 `targetFrequency / detectedInputFrequency` を使う。
- 検出 pitch が 0 の場合は、従来通り MIDI note 60 / 261.625565 Hz を fallback reference として使う。
- したがって、実測どおり変化しない場合は、pitch detector が 0 のまま、または簡易 pitch shifter が高 ratio を聴感上正しく出せていない可能性が高い。

実装済み:

- GUI に `Pitch: --` / `Pitch: xxx.x Hz` の debug 表示を追加した。
- `SimpleChoirEngine` の検出 pitch を `PluginProcessor` 経由で atomic publish し、GUI timer から読み出すようにした。
- pitch detector の zero-crossing threshold を下げ、低めの入力レベルでも検出しやすくした。

ユーザー確認待ち:

- 入力音を入れたとき `Pitch: --` から Hz 表示へ変わるか。
- 声の音高を上下させたとき、表示 Hz が追従するか。
- Hz 表示が出ている状態で、同じ MIDI note に対する wet pitch の変換量が入力 pitch に応じて変わるか。
- C5 より上の MIDI note で、Hz 表示の有無によって頭打ち挙動が変わるか。

## 0.1.8 phase 3G visible pitch debug label

日付: 2026-05-18

対象ビルド:

- Debug Standalone: ユーザー確認予定
- Debug VST3: ユーザー確認予定
- Release Standalone: ユーザー確認予定
- Release VST3: ユーザー確認予定

ユーザー確認結果:

- 0.1.7 では GUI に `Pitch: --` / `Pitch: xxx.x Hz` が表示されないことを確認した。

再確認した実装状態:

- `pitchDebugLabel` 自体は作成され `addAndMakeVisible()` されていた。
- ただし top status row 内の右側メーター群に混ぜていたため、表示位置が分かりにくい、またはレイアウト上で見落とされる可能性があった。

実装済み:

- `Pitch` debug 表示を top row から event row の右側へ移動した。
- 既存の `MIDI:` 行にも `| Pitch: --` / `| Pitch: xxx.x Hz` を追加表示するようにした。
- subtitle にも `Pitch` debug 表示を重複表示し、少なくとも 1 箇所では確認できるようにした。
- バージョンを `0.1.8` に更新した。

ユーザー確認待ち:

- GUI 上で `Pitch: --` が見えること。
- 入力音を入れたとき、`Pitch: xxx.x Hz` に変わること。
- MIDI row / event row / subtitle のいずれかで Pitch 表示が確認できること。

## 0.1.9 phase 3H YIN pitch detector stabilization

日付: 2026-05-18

対象ビルド:

- Debug Standalone: ユーザー確認予定
- Debug VST3: ユーザー確認予定
- Release Standalone: ユーザー確認予定
- Release VST3: ユーザー確認予定

実装方針:

- `DIRECTION0518.md` に従い、zero crossing ベースの pitch detector を廃止した。
- 既存の 4 voice choir / MIDI 制御 / 基本 UI 構造は維持した。
- 音質改善や新機能追加ではなく、入力 pitch 検出パイプラインの安定化を目的とした。

採用したアルゴリズム:

- YIN / CMNDF
- `frameLength = 2048 samples`
- `hopSize = 512 samples`
- `pitchRange = 70-1000 Hz`
- `rmsGateDb = -45.0 dBFS`
- `confidence = 1.0 - cmndfValue`
- `confidenceThreshold = 0.75`
- `smoothingAlpha = 0.2`
- `holdTimeMs = 100 ms`

実装済み:

- 入力音声を detector 内部の ring buffer に蓄積し、512 samples ごとに 2048 samples frame を解析するようにした。
- frame RMS を計算し、RMS gate 未満では raw pitch / confidence / voiced を無効化するようにした。
- YIN の difference function と cumulative mean normalized difference function を実装した。
- confidence が低い raw pitch を `stablePitchHz` に即反映しないようにした。
- `rawPitchHz`, `stablePitchHz`, `confidence`, `voiced`, `inputRmsDb` を持つ `PitchState` を追加した。
- ハーモニー生成側は raw pitch ではなく `stablePitchHz` を参照するようにした。
- octave 誤検出対策として、前回 stable pitch に近い `raw`, `raw / 2`, `raw * 2` を候補にする補正を追加した。
- 100 ms の hold を追加し、一瞬の検出失敗で stable pitch が即 0 にならないようにした。
- GUI subtitle に `Build: pitch-yin-001`, RMS, Raw Pitch, Stable Pitch, Confidence, Voiced を表示するようにした。

ユーザー確認待ち:

- 無音時に Raw Pitch / Stable Pitch が暴れないこと。
- Input RMS dB が GUI で確認できること。
- 持続母音で Stable Pitch が大きく暴れないこと。
- 子音やノイズでランダムな pitch が出にくいこと。
- Raw Pitch が多少揺れても Stable Pitch がなめらかになること。
- Confidence が低いときに `stablePitchHz` へ反映されにくいこと。
- ハーモニー生成が `stablePitchHz` に基づいて動くこと。
- subtitle の `Build: pitch-yin-001` により新しいビルドであることを確認できること。

## 0.1.24 focused live GUI layout

Date: 2026-05-19

Target builds:

- Debug Standalone: user verification pending
- Debug VST3: user verification pending
- Release Standalone: user verification pending
- Release VST3: user verification pending

Implemented:

- Removed the unused Tune knob from the visible GUI.
- Kept the `tune` APVTS parameter for compatibility.
- Moved Input Source selection to the upper-right area.
- Moved Input Gain, Output Gain, and PANIC to the upper-right area.
- Changed Input Gain and Output Gain to smaller horizontal controls.
- Changed the main performance knob row to `Voices`, `Glide`, `Character`, `Spread`, and `Dry/Wet`.
- Replaced text-only input/output meter labels with horizontal bar-style meter components.
- Moved input/output bar meters to the lower-right area.
- GUI Debug build string updated to `Build: gui-layout-001`.
- CMake project version updated to `0.1.24`.

Not changed:

- No parameter ID was removed or renamed.
- No DSP, pitch detector, input-synced window, or MIDI de-click behavior was changed.

Verification pending:

- Confirm Tune knob is no longer visible.
- Confirm Input Source, Input Gain, Output Gain, and PANIC are grouped in the upper-right area.
- Confirm input/output levels are visually readable as bar meters in the lower-right area.
- Confirm existing parameter automation/state compatibility is preserved.

## 0.1.23 priority A live usability pass

Date: 2026-05-19

Target builds:

- Debug Standalone: user verification pending
- Debug VST3: user verification pending
- Release Standalone: user verification pending
- Release VST3: user verification pending

Implementation request:

- Follow `directions/0519_1.md` priority A items.
- Do not change PitchDetector, input-synced window pitch shifter, ASIO/Standalone/VST3 basics, or existing parameter IDs.

Implemented:

- Release build top debug subtitle now shows only `VoxChord v<version>`.
- Debug build keeps detailed self-test, pitch, RMS, confidence, voiced, harmonic correction, ratio smoothing, and build tag display.
- Added `inputGainDb` APVTS parameter: `-24.0 dB` to `+24.0 dB`, default `0.0 dB`, step `0.1 dB`.
- Added GUI Input Gain knob.
- Input Gain is applied after Input Source selection and before Pitch Detector / Harmony / dry path / Input Meter.
- Input Gain uses `20 ms` smoothing.
- Changed wet voice gain from active-count-normalized gain to constant voice-level style gain with `baseVoiceGain = 0.45`.
- Changed voice stealing assignment to prefer the active voice with the nearest MIDI note to the incoming note, using oldest age as a tie breaker.
- GUI Debug build string updated to `Build: priority-a-001`.
- CMake project version updated to `0.1.23`.

Not changed:

- No empirical ratio correction coefficient was added.
- PitchDetector behavior was not changed.
- Input-synced window and MIDI de-click behavior remain in place.
- No new pitch shifter, reverb, Tuned Lead, Voice 8, PSOLA, or formant shifter was added.

Verification pending:

- Release build top display shows version only.
- Debug build still shows detailed debug status.
- Input Gain `-12 / 0 / +12 dB` changes input level, input meter, pitch detector input, and harmony input consistently.
- Voice Count `1 / 2 / 4` keeps per-voice loudness more consistent, while total wet level may rise naturally.
- Fast chord movement such as `C-E-G -> D-F-A` maps closer notes together more naturally.

## 0.1.22 phase 3T MIDI note transition de-click

Date: 2026-05-19

Target builds:

- Debug Standalone: user verification pending
- Debug VST3: user verification pending
- Release Standalone: user verification pending
- Release VST3: user verification pending

User finding:

- Input-synced window made harmony muddiness almost negligible.
- Click/pop artifacts from window changes were greatly reduced.
- Occasional clicks remain when MIDI input changes quickly.

Implemented:

- Kept the input-synced window pitch shifter path.
- Added per-voice attack/release envelope for MIDI note transitions.
- New voices fade in over approximately `8 ms`.
- Released voices fade out over approximately `12 ms` and continue rendering during release.
- MIDI target ratio changes now use short log-domain smoothing when Glide is off.
- Existing slot note changes preserve `phaseA`, `phaseB`, and active `windowSamplesA/B`.
- Active grains still do not change window length mid-grain.
- GUI debug build string updated to `Build: midi-declick-001`.
- CMake project version updated to `0.1.22`.

Not changed:

- No empirical ratio correction coefficient was added.
- PitchDetector behavior was not changed.
- The input-synced window continuity behavior remains in place.

Verification pending:

- User build of Debug/Release targets.
- Confirm existing pitch shifter self test PASS behavior is unchanged.
- Confirm fast MIDI note changes produce fewer clicks.
- Confirm Note Off release does not feel too sluggish for live playing.

## 0.1.21 phase 3S input-synced window continuity

Date: 2026-05-19

Target builds:

- Debug Standalone: user verification pending
- Debug VST3: user verification pending
- Release Standalone: user verification pending
- Release VST3: user verification pending

User finding:

- Input-synced window significantly improved pitch accuracy.
- In real use, short click/pop artifacts can occur.
- The likely cause is not CPU dropout, but delay/readPosition discontinuity from dynamic `pitchWindowSamples` changes.

Implemented:

- Separated pitch-ratio input and window-length input.
- `correctionInputPitchHz` remains the source for MIDI pitch ratio.
- Added slower-smoothed `windowPitchHz` for input-synced window calculation.
- Added per-voice `windowSamplesA` and `windowSamplesB`.
- `delayA` and `delayB` now use each read window's own window length.
- Target window length is no longer applied immediately to currently playing grains.
- Each A/B window adopts a new window length only at its own phase wrap.
- Per-wrap window length changes are limited by `maxWindowChangeRatioPerGrain` and `maxWindowChangeSamplesPerGrain`.
- GUI debug build string updated to `Build: input-synced-window-continuity-001`.
- CMake project version updated to `0.1.21`.

Not changed:

- No empirical ratio correction coefficient was added.
- PitchDetector behavior was not changed.
- Input-synced window remains the render-path default candidate.

Verification pending:

- User build of Debug/Release targets.
- Confirm pitch accuracy remains close to the input-synced window build.
- Confirm click/pop artifacts during vocal pitch changes are reduced.
- Confirm abrupt pitch changes do not create stuck or unstable grains.

## 0.1.20 phase 3R separated self test summaries and expanded input-sync tests

Date: 2026-05-19

Target builds:

- Debug Standalone: user verification pending
- Debug VST3: user verification pending
- Release Standalone: user verification pending
- Release VST3: user verification pending

Context:

- User confirmed that the input-synced window direction is effective.
- Next diagnostic need: fixed-window and input-synced-window self test summaries must be separated so fixed failures do not obscure input-synced results.
- Low input frequencies can produce long windows, so window tuning values should be easy to adjust.

Implemented:

- Split `PitchShifterSelfTestSummary` into separate `fixedWindow` and `inputSyncedWindow` mode summaries.
- GUI debug subtitle now reports fixed and input-synced self test PASS/FAIL independently.
- Input-synced window is now the render-path default candidate via `useInputSyncedPitchWindowByDefault = true`.
- Window constants are grouped for tuning: `fixedPitchWindowSeconds`, `inputSyncedPitchWindowCycles`, `inputSyncedMinWindowSamples`, `inputSyncedMaxWindowSamples`.
- Input-synced self test coverage was expanded to inputs `100/150/220/330/440/660/880 Hz` and ratios `0.5/0.75/1.0/1.5/2.0`.
- Fixed-window representative cases remain for regression comparison.
- GUI debug build string updated to `Build: input-synced-window-002`.
- CMake project version updated to `0.1.20`.

Not changed:

- No empirical ratio correction coefficient was added.
- PitchDetector behavior was not changed.
- The delay-window pitch shifter structure was not replaced.

Verification pending:

- User build of Debug/Release targets.
- Confirm the GUI shows separate `Fixed` and `InputSync` self test summaries.
- Confirm DBG output includes the expanded input-synced test range.
- Check whether low-frequency cases such as `100 Hz * 0.5` remain acceptable with the current max window clamp.

## 0.1.19 phase 3Q input-synced pitch window prototype

Date: 2026-05-19

Target builds:

- Debug Standalone: user verification pending
- Debug VST3: user verification pending
- Release Standalone: user verification pending
- Release VST3: user verification pending

Context:

- Previous PitchShifterSelfTest results indicated that read speed, phaseDelta, delayStep, and phaseWrap behaved according to theory.
- Spectral diagnostics showed that the dominant output peak could still shift away from the expected frequency.
- Current hypothesis: the fixed-window delay shifter's non-input-synchronous crossfade/grain length may create sidebands or an effective spectral peak offset.

Implemented:

- Added an experimental input-F0-synced pitch window mode without empirical ratio correction.
- Active window formula: `periodSamples = sampleRate / correctionInputPitchHz`.
- Active window formula: `windowSamples = clamp(round(6.0 * periodSamples), 256, 4096)`.
- Invalid or missing `correctionInputPitchHz` falls back to the existing fixed `18 ms` window.
- `renderPitchShiftedSample()` now receives the active `windowSamples` explicitly and keeps the theoretical relation `phaseDelta = (1 - ratio) / windowSamples`.
- Delay buffer allocation now reserves for the maximum 4096-sample window.
- PitchShifterSelfTest now keeps fixed-window cases and adds input-synced comparison cases for `440/660/880 Hz * 0.5` and `440 Hz * 2.0`.
- SelfTest DBG output now includes `windowMode`, active `pitchWindowSamples`, `fixedPitchWindowSamples`, `inputPeriodSamples`, and `inputSyncedWindowCycles`.
- GUI debug build string updated to `Build: input-synced-window-001`.
- CMake project version updated to `0.1.19`.

Not changed:

- No empirical ratio correction coefficient was added.
- PitchDetector behavior was not changed.
- The pitch shifter algorithm was not replaced.

Verification pending:

- User build of Debug/Release targets.
- Compare DBG output for fixed vs input-synced window cases, especially ratio `0.5`.
- Check whether the top spectral peak moves closer to expected frequencies for `220/330/440 Hz` expected outputs.

## 0.1.18 phase 3P pitch shifter spectral diagnostics

日付: 2026-05-19

対象ビルド:

- Debug Standalone: ユーザー確認待ち
- Debug VST3: ユーザー確認待ち
- Release Standalone: ユーザー確認待ち
- Release VST3: ユーザー確認待ち

実装方針:

- pitch shifter の ratio 補正や音質変更は行わない。
- PitchShifterSelfTest の出力波形に対して、expected/measured 付近と sideband を確認するためのスペクトル診断ログを追加する。

実装済み:

- 指定4ケースに `PitchShifterSpectrum` DBG 行を追加した。
- 対象ケースは `440 Hz * 0.5 -> 220 Hz`, `660 Hz * 0.5 -> 330 Hz`, `880 Hz * 0.5 -> 440 Hz`, `440 Hz * 2.0 -> 880 Hz`。
- top 5 spectral peaks を frequency / magnitude で表示するようにした。
- expected frequency bin magnitude を表示するようにした。
- measured frequency bin magnitude を表示するようにした。
- measured result に使っている周波数源を `zeroCrossMeasured` と明記した。
- スペクトル解析は self test output の tail に対して Hann window + Goertzel scan で行う。
- GUI debug 表示を `Build: pitch-shifter-spectrum-001` に更新した。
- CMake project version を `0.1.18` に更新した。
- `SPEC.md` に spectral diagnostics 仕様を追記した。

未確認:

- ユーザー環境での Debug / Release ビルド。
- expected frequency 付近に強い peak があるか。
- zero-cross measured frequency 付近に別の強い peak があるか。
- window/crossfade 由来の sideband が top 5 に出るか。
- ratio `0.5`, expected `330 Hz` 周辺の peak 構造。

次に確認すべきこと:

- Debug Standalone を起動し、`PitchShifterSpectrum` 行を確認すること。
- top 5 peak と expectedBin / measuredBin の magnitude を比較すること。
- expectedBin が最大 peak なのか、zero-cross measured 付近や sideband が強いのかを確認すること。

## 0.1.17 phase 3O pitch shifter phase wrap diagnostics

日付: 2026-05-19

対象ビルド:

- Debug Standalone: ユーザー確認待ち
- Debug VST3: ユーザー確認待ち
- Release Standalone: ユーザー確認待ち
- Release VST3: ユーザー確認待ち

ユーザー確認済みの 0.1.16 self test 傾向:

- ratio `1.0` は完全一致。
- target ratio `2.0` -> actual ratio `2.01008`
- target ratio `1.5` -> actual ratio `1.50505`
- target ratio `0.5` -> actual ratio `0.49495`
- ratio が `1.0` から離れる方向に実効 ratio が約 `1.010` 倍過剰に動いている。

実装方針:

- 経験的な ratio 補正係数は追加しない。
- pitch shifter の音を変えず、phase / delay / read position / window wrap の実測診断ログだけを追加する。

実装済み:

- PitchShifterSelfTest の各ケースで phaseA / phaseB の wrap interval samples を計測するようにした。
- `measuredDelayStepA/B avg/min/max/count` を DBG に追加した。
- `measuredReadStepA/B avg/min/max/count` を DBG に追加した。
- `phaseWrapA/B count/avg/min/max` を DBG に追加した。
- read position step は delay buffer wrap の影響で巨大差分にならないよう circular delta として計測する。
- GUI debug 表示を `Build: pitch-shifter-wrap-diagnostics-001` に更新した。
- CMake project version を `0.1.17` に更新した。
- `SPEC.md` に phase wrap / delay step / read step 診断仕様を追記した。

コード再確認メモ:

- `renderPitchShiftedSample()` は delayA/delayB と gainA/gainB を現在の phaseA/phaseB から計算し、`readDelayLine()` 後に phaseA/phaseB を更新する。
- `readDelayLine()` は `readPosition = writeIndex - delaySamples` 相当の計算を行い、circular buffer 内に wrap した後、`index0=floor(readPosition)`, `index1=index0+1`, `fraction=readPosition-index0` で線形補間する。
- ratio `2.0` では理論上 delay step は `-1.0 sample/output sample`、read step は `2.0 sample/output sample`。
- ratio `0.5` では理論上 delay step は `+0.5 sample/output sample`、read step は `0.5 sample/output sample`。
- `pitchWindowSamples=864` で ratio `2.0` の場合、phase wrap interval は理論上 `864 samples`。

未確認:

- ユーザー環境での Debug / Release ビルド。
- phaseWrapA/B の actual wrap interval が `pitchWindowSamples` と一致するか。
- measuredDelayStepA/B が理論 delay step と一致するか。
- measuredReadStepA/B が theoreticalReadSpeed と一致するか。
- これらが一致する場合、次の疑いを window crossfade / effective window length / 測定窓へ進めること。

次に確認すべきこと:

- Debug Standalone を起動し、ratio `2.0`, `1.5`, `0.5` の `measuredDelayStep`, `measuredReadStep`, `phaseWrap` を確認すること。
- phase wrap interval が約 `855 samples` ではなく `864 samples` か確認すること。
- delay/read step が理論値と一致するか確認すること。

## 0.1.16 phase 3N pitch shifter ratio diagnostics

日付: 2026-05-19

対象ビルド:

- Debug Standalone: ユーザー確認待ち
- Debug VST3: ユーザー確認待ち
- Release Standalone: ユーザー確認待ち
- Release VST3: ユーザー確認待ち

ユーザー確認済みの 0.1.15 self test 傾向:

- ratio `2.0` は約 `+8.70 cents`
- ratio `1.5` は約 `+5.82 cents`
- ratio `0.5` は約 `-17.56` から `-17.59 cents`
- 実効 ratio は target ratio からランダムではなく系統的にずれている可能性がある。

実装方針:

- 経験的な ratio 補正係数は追加しない。
- pitch detector 改善、choir 音質改善、新 pitch shifter 追加は行わない。
- まず ratio 1.0 と phase/delay/read speed の診断ログを追加し、原因が phaseDelta / delay / readPosition / interpolation / measurement のどこにあるか切り分ける。

実装済み:

- PitchShifterSelfTest に ratio `1.0` ケースを追加した。
- 追加ケースは `440 Hz * 1.0 -> 440 Hz`, `660 Hz * 1.0 -> 660 Hz`, `880 Hz * 1.0 -> 880 Hz`。
- 各ケースの debug output に `actual ratio`, `actual/target`, `phaseDelta`, `delayStep`, `theoreticalReadSpeed` を追加した。
- GUI summary に worst case の actual ratio を追加した。
- GUI debug 表示を `Build: pitch-shifter-diagnostics-001` に更新した。
- CMake project version を `0.1.16` に更新した。
- `SPEC.md` に ratio 1.0 追加ケースと診断ログ仕様を追記した。

コード再確認メモ:

- 現在の phase 更新順序は、delay/read 計算後に `phaseA` / `phaseB` を更新する形。
- `readDelayLine()` は `readPosition = writeIndex - delaySamples` を circular buffer 内へ wrap し、`index0=floor(readPosition)`, `index1=index0+1`, `fraction=readPosition-index0` で線形補間している。
- 式だけを見ると `delay = baseDelay + phase * pitchWindowSamples`, `phaseDelta = (1 - ratio) / pitchWindowSamples` なので、`delayStep = 1 - ratio`, `readPositionStep = 1 - delayStep = ratio` となる。
- したがって、現段階では数学式上の read speed は target ratio と一致する想定であり、残る切り分け対象は window wrap/crossfade, window 有効長, interpolation, self test measurement である。

未確認:

- ユーザー環境での Debug / Release ビルド。
- ratio `1.0` の measured frequency と error cents。
- ratio `1.0` の誤差が十分小さいか。
- actual/target が ratio 依存で同じ傾向を示すか。
- GUI summary と DBG 詳細ログが一致するか。

次に確認すべきこと:

- Debug Standalone を起動し、ratio `1.0` 3ケースの `PitchShifterSelfTest` 詳細ログを確認すること。
- 各ケースの `actual ratio`, `actual/target`, `phaseDelta`, `delayStep`, `theoreticalReadSpeed` を確認すること。
- ratio `1.0` でも誤差が出る場合は measurement / readDelayLine / delay tap 側を優先して疑うこと。
- ratio `1.0` が正確で ratio != 1 だけずれる場合は window wrap/crossfade または phase window 有効長を優先して疑うこと。

## 0.1.15 phase 3M pitch shifter self test GUI summary

日付: 2026-05-19

対象ビルド:

- Debug Standalone: ユーザー確認待ち
- Debug VST3: ユーザー確認待ち
- Release Standalone: ユーザー確認待ち
- Release VST3: ユーザー確認待ち

実装方針:

- PitchShifterSelfTest の詳細ログは `DBG()` のまま残し、GUI debug 欄にも成否要約を表示する。
- pitch shifter 本体、pitch detector、MIDI 処理、choir 音質は変更しない。

実装済み:

- `PitchShifterSelfTestSummary` を追加した。
- `SimpleChoirEngine::runPitchShifterSelfTest()` が max error cents と worst case を保存するようにした。
- `SimpleChoirEngine::getPitchShifterSelfTestSummary()` と `VoxChordAudioProcessor::getPitchShifterSelfTestSummary()` を追加した。
- GUI debug subtitle に `Pitch Shifter SelfTest: PASS / FAIL`, max error cents, worst case input Hz, worst case ratio, worst case measured Hz を表示するようにした。
- self test 未実行時は `Pitch Shifter SelfTest: NOT RUN` と表示するようにした。
- GUI debug 表示を `Build: pitch-shifter-selftest-gui-001` に更新した。
- CMake project version を `0.1.15` に更新した。
- `SPEC.md` に GUI summary 仕様を追記した。

未確認:

- ユーザー環境での Debug / Release ビルド。
- Debug Standalone 起動時に GUI debug 欄で PASS/FAIL が見えること。
- GUI の max error cents / worst input / worst ratio / worst measured が `DBG()` 詳細ログと一致すること。
- Release など self test 未実行時に `NOT RUN` と表示されること。

次に確認すべきこと:

- Debug Standalone を起動し、GUI 上で `Pitch Shifter SelfTest: PASS` または `FAIL` が表示されること。
- FAIL の場合は GUI の worst case と `DBG()` の該当行を照合すること。

## 0.1.14 phase 3L pitch shifter self test

日付: 2026-05-19

対象ビルド:

- Debug Standalone: ユーザー確認待ち
- Debug VST3: ユーザー確認待ち
- Release Standalone: ユーザー確認待ち
- Release VST3: ユーザー確認待ち

実装方針:

- ユーザー指示に従い、高域 ratio 補正などの経験的ハックは行わず、既存 delay-window pitch shifter 単体の固定 ratio 精度を検証する debug self test のみ追加した。
- PitchDetector 改善、choir 音質改善、新 pitch shifter 追加、MIDI voice 仕様変更は行っていない。
- 通常の `processBlock()` では self test を実行しない。

実装済み:

- `SimpleChoirEngine::runPitchShifterSelfTest()` を追加した。
- Debug ビルド時に processor 生成時だけ、PitchDetector self test とは別に pitch shifter self test を実行するようにした。
- self test は内部サイン波を delay buffer に書き込み、固定 ratio の `renderPitchShiftedSample()` に直接通す。
- Character=0 相当、delay offset `0`、Voice Count=1 相当、Glide 無効として実行する。
- ratio smoothing/glide は、`currentPitchRatio` と `targetPitchRatio` を固定 ratio に揃え、`renderPitchShiftedSample()` に `glideCoefficient=1.0f` を渡すことで完全に無効化した。
- 出力信号の周波数は、初期 transient を skip した後の正方向ゼロクロスから測定する。
- Debug output に expected frequency, measured frequency, error cents, +/-10 cents 判定, `pitchWindowSamples`, `minimumDelaySamples`, ratio smoothing/glide disabled を出力する。
- GUI debug 表示を `Build: pitch-shifter-selftest-001` に更新した。
- CMake project version を `0.1.14` に更新した。
- `SPEC.md` に pitch shifter self test 仕様を追記した。

Self test cases:

- input `220 Hz`, ratio `2.0`, expected `440 Hz`
- input `440 Hz`, ratio `2.0`, expected `880 Hz`
- input `440 Hz`, ratio `1.5`, expected `660 Hz`
- input `440 Hz`, ratio `0.5`, expected `220 Hz`
- input `660 Hz`, ratio `0.5`, expected `330 Hz`
- input `880 Hz`, ratio `0.5`, expected `440 Hz`

未確認:

- ユーザー環境での Debug / Release ビルド。
- Debug output に PitchShifterSelfTest の各ケース結果が表示されること。
- 各ケースが pitch shifter 単体で +/-10 cents 以内に収まること。
- `pitchWindowSamples` / `minimumDelaySamples` が debug output に表示されること。

次に確認すべきこと:

- Debug Standalone または Debug VST3 を起動し、Output window の `PitchShifterSelfTest` 行を確認すること。
- measured frequency と error cents を共有して、問題が pitch shifter 本体か、それ以前の pitch tracking / ratio generation かを切り分けること。

## 0.1.13 phase 5A standalone input source

日付: 2026-05-19

対象ビルド:

- Debug Standalone: ユーザー確認待ち
- Debug VST3: ユーザー確認待ち
- Release Standalone: ユーザー確認待ち
- Release VST3: ユーザー確認待ち

実装方針:

- `directions/0518_5.md` に従い、音質系 DSP / pitch detector / MIDI voice 仕様は変更せず、入力チャンネル選択と mono 化処理だけを対象にした。
- Standalone では物理入力の `Input 1`, `Input 2`, `Mix 1+2`, `Auto` を選べるようにした。
- VST3 では DAW ルーティングを尊重し、常に ch0/L を VoxChord の vocal input として扱う。

実装済み:

- APVTS に `inputSource` choice parameter を追加した。選択肢は `Auto`, `Input 1`, `Input 2`, `Mix 1+2`、default は `Auto`。
- GUI event row に最小限の Input Source ComboBox を追加した。
- `copyInputToDryBuffer()` で、Standalone のみ `inputSource` に従って mono input を選択し、dry buffer の L/R 両方へ同じ信号を書き込むようにした。
- `Input 2` は ch1 が存在する場合 ch1、存在しない場合 ch0 fallback とした。
- `Mix 1+2` は ch1 が存在する場合 `0.5 * (ch0 + ch1)`、存在しない場合 ch0 とした。
- `Auto` は block ごとの ch0/ch1 peak を比較し、大きい方を選択する。ch1 が存在しない場合は ch0。
- VST3 では `inputSource` に関係なく ch0/L 固定で処理するようにした。
- input meter は選択後 mono input の level を表示するようにした。
- GUI debug 表示を `Build: input-source-standalone-001` に更新した。
- CMake project version を `0.1.13` に更新した。
- `SPEC.md` に input source 仕様、Standalone/VST3 の分岐、既知制限を追記した。

未確認:

- ユーザー環境での Debug / Release ビルド。
- ASIO Standalone / UR22C / Input 1 / Dry/Wet=0 で音が出ること。
- ASIO Standalone / UR22C / Input 2 / Dry/Wet=0 で音が出ること。
- ASIO Standalone / UR22C / Mix 1+2 / Dry/Wet=0 で音が出ること。
- ASIO Standalone / UR22C / Auto で Input 1 または Input 2 を選べること。
- VST3 で stereo input 時に L/ch0 のみが vocal input として使われること。

次に確認すべきこと:

- Standalone で Input Source ComboBox が表示され、選択できること。
- Dry/Wet=0 でも選択した input source が dry through されること。
- PitchDetector と wet choir も選択した input source に追従すること。
- VST3 では inputSource による物理入力選択を期待せず、DAW 側の routing で入力を選ぶこと。

## 0.1.12 phase 3K hard tune tracking

日付: 2026-05-18

対象ビルド:

- Debug Standalone: ユーザー確認待ち
- Debug VST3: ユーザー確認待ち
- Release Standalone: ユーザー確認待ち
- Release VST3: ユーザー確認待ち

実装方針:

- `directions/0518_4.md` に従い、新しい 2 段 pitch shift は追加せず、既存の 1 段 pitch shift の pitch ratio 計算に使う入力ピッチ追従を改善した。
- GUI 表示用の安定ピッチと、実際の `targetMidiHz / inputPitchHz` に使う補正用ピッチを分離した。
- UI デザイン、MIDI 制御、4 Voice Choir の基本構造は変更していない。

実装済み:

- `PitchState` に `displayStablePitchHz`, `correctionInputPitchHz`, `ratioSmoothingCoefficient` を追加した。
- `stablePitchHz` は後方互換のため残し、実体としては GUI 表示用の `displayStablePitchHz` と同義にした。
- `harmonyPitchHz` は後方互換のため残し、実体としては pitch ratio 計算用の `correctionInputPitchHz` と同義にした。
- pitch ratio 計算を `targetMidiHz / correctionInputPitchHz` に変更した。
- `correctionInputPitchHz` は confidence が高く voiced な `correctedPitchHz` に log-domain で `0.7` の fast attack 追従を行うようにした。
- 無音または低 confidence が `100 ms` を超えた場合、`correctionInputPitchHz` を無効化して、ランダムな pitch ratio が出ないようにした。
- GUI debug 表示を `Build: hard-tune-tracking-001` に更新し、`Disp` と `RatioIn` と `RatioSmooth` を表示するようにした。
- ratio smoothing 係数を `0.10` から `0.35` に変更し、入力ピッチ変化への追従遅れを軽減した。
- `Tune` ノブは UI 上は残したまま、内部 DSP では hard tune 固定として未使用にした。
- CMake project version を `0.1.12` に更新した。

未確認:

- ユーザー環境での Debug / Release ビルド。
- GUI に `Build: hard-tune-tracking-001` が表示されること。
- A4 付近で入力が `435-445 Hz` 程度に揺れても、出力ハーモニーの音程が MIDI 目標に安定すること。
- `Disp` は見た目用に滑らかで、`RatioIn` は入力 pitch へより速く追従すること。
- `Tune` ノブが現段階で音に影響しないこと。

次に確認すべきこと:

- `Raw`, `Corr`, `Disp`, `RatioIn` のうち、実音声でどこが不安定になるか確認すること。
- 無音時または子音時に `RatioIn` が短時間 hold 後に無効化され、wet pitch が暴れないこと。
- MIDI note 変更時の Glide と、入力 pitch tracking が混ざって不自然に遅れないこと。

## 0.1.11 phase 3J pitch range 900 self test

日付: 2026-05-18

対象ビルド:

- Debug Standalone: ユーザー確認待ち
- Debug VST3: ユーザー確認待ち
- Release Standalone: ユーザー確認待ち
- Release VST3: ユーザー確認待ち

実装方針:

- `directions/0518_3.md` に従い、PitchDetector 本体の検出範囲と 600 Hz 超えの octave-down 誤検出切り分けを優先した。
- UI デザイン、MIDI 制御、Voice Choir の基本構造、新規エフェクト追加は行っていない。
- 通常の audio thread 内では self test を実行せず、Debug ビルドの processor 生成時に一度だけ実行する形にした。

実装済み:

- pitch range を `80-900 Hz` に変更した。
- lag 範囲計算を `floor(sampleRate / maxFrequencyHz)` / `ceil(sampleRate / minFrequencyHz)` に変更した。
- YIN の lag 選択を、threshold 到達後の局所最小を優先し、fallback 時は近いスコアなら短い lag 側を優先する形に調整した。
- harmonic correction を内部フラグ `harmonicCorrectionEnabled` で OFF にできるようにした。
- harmonic correction は raw/2, raw/3, raw*2, raw*3 候補を持つが、前回 stable への過剰ロックを避けるため、高 confidence の raw が継続している場合は raw 側へ戻れるようにした。
- correction 候補は 1 frame だけでは採用せず、同じ補正候補が連続した場合のみ採用するようにした。
- Debug self test として、内部サイン波 `100 / 150 / 220 / 261.63 / 329.63 / 440 / 523.25 / 600 / 659.25 / 700 / 800 / 880 Hz` を PitchDetector に直接入力する処理を追加した。
- GUI debug 表示を `Build: pitch-range-900-selftest-001` に更新した。
- CMake project version を `0.1.11` に更新した。

未確認:

- ユーザー環境での Debug / Release ビルド。
- Debug 出力に self test 結果が表示されること。
- harmonic correction OFF の self test で 700 Hz が 350 Hz、800 Hz が 400 Hz、880 Hz が 440 Hz に落ちないこと。
- 実音声で 600 Hz 超えの Raw Pitch が半分に落ちにくくなること。
- harmonic correction ON の通常動作で、高音 Raw Pitch が raw/2 へ過剰補正されないこと。

次に確認すべきこと:

- GUI の `Build: pitch-range-900-selftest-001` が表示されること。
- GUI の Raw / Corrected / Stable / Harmony のどの段階で誤差が出るかを確認すること。
- 連続母音で 700-880 Hz 付近を入力し、Raw Pitch が追従するか確認すること。

## 0.1.10 phase 3I pitch stability correction

日付: 2026-05-18

対象ビルド:

- Debug Standalone: ユーザー確認予定
- Debug VST3: ユーザー確認予定
- Release Standalone: ユーザー確認予定
- Release VST3: ユーザー確認予定

実装方針:

- `directions/0518_2.md` に従い、既存 YIN detector の後段安定化を行った。
- UI デザイン、4 Voice Choir、MIDI 制御、基本パラメータ構造は変更していない。
- raw pitch をハーモニー生成へ直接使わない方針を徹底した。

実装済み:

- `PitchState` に `correctedPitchHz`, `harmonyPitchHz`, `harmonicCorrectionMode` を追加した。
- `rawPitchHz`, `rawPitchHz / 2`, `rawPitchHz / 3`, `rawPitchHz * 2` の候補から、前回 stable pitch に cent 差で最も近い値を選ぶ harmonic correction を追加した。
- pitch range を `70-600 Hz` に変更した。
- `correctedPitchHz` から `stablePitchHz` を作る前に、log2 周波数上の median filter を追加した。
- `maxJumpCents = 350.0f` による外れ値判定を追加した。
- confidence が非常に高く、かつ大ジャンプが連続した場合のみ新しい pitch へ移れるようにした。
- `stablePitchHz` の smoothing を log 周波数上で行うようにした。
- `harmonyPitchHz` を追加し、`stablePitchHz` からさらに `harmonySmoothingAlpha = 0.1f` で平滑化するようにした。
- ハーモニー生成側の pitch ratio は `targetFrequencyHz / harmonyPitchHz` を使うようにした。
- `harmonyPitchHz <= 0.0f` の場合は固定 C3 fallback ではなく、pitch ratio を 1.0 にして無効 pitch から比率計算しないようにした。
- voice ごとの pitch ratio 変化に最低限 `ratioSmoothingAlpha = 0.1f` を適用するようにした。
- GUI debug 表示を `Build: pitch-stability-002` に更新し、Raw / Corrected / Stable / Harmony / Confidence / Voiced / Harmonic Correction を表示するようにした。

ユーザー確認待ち:

- 持続母音で `rawPitchHz` が一瞬 3 倍に飛んでも `correctedPitchHz` または `stablePitchHz` が追従しにくいこと。
- `stablePitchHz` が大きく octave / 3 倍ジャンプしないこと。
- `harmonyPitchHz` が `stablePitchHz` よりさらに滑らかに変化すること。
- pitch ratio が瞬間的に大きく変わりにくいこと。
- 子音、息、無音でハーモニーが暴れにくいこと。
- GUI で Raw / Corrected / Stable / Harmony Pitch の違いを確認できること。
- `Build: pitch-stability-002` が表示され、新しいビルドであることを確認できること。
