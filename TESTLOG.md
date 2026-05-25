# VoxChord Test Log

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
