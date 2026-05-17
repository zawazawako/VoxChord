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
