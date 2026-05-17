# VoxChord Test Log

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
