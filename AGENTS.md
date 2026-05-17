# AGENTS.md

## プロジェクト概要

本プロジェクトは、JUCE フレームワークと C++ を用いて、ライブ演奏で使えるデジタルクワイア・プラグイン **VoxChord** を開発するプロジェクトである。

VoxChord は、入力された単声ボーカルを MIDI ノートでリアルタイムに和声化し、複数のデジタルクワイア声として出力することを目的とする。目標は、作曲支援用の多機能ボーカルエフェクトではなく、ライブで演奏できる軽量な「声の楽器」である。

完成形の方向性としては、Bon Iver / Justin Vernon 周辺で知られる The Messina 的なデジタルクワイア質感を目指す。ただし、特定アーティストや既存機材の完全コピーではなく、低遅延・軽量・無料公開可能な Windows 向け VST3 / Standalone プラグインとして独自実装する。

---

## 最重要方針

このプロジェクトの最優先事項は **ライブ演奏で使えること** である。

したがって、以下を常に優先する。

1. 低遅延
2. 安定動作
3. 軽量な DSP
4. 操作の単純さ
5. MIDI キーボードによる即時演奏性
6. VST3 と Standalone の両対応
7. 無料公開しやすいシンプルな構成

音質向上や機能追加は重要だが、低遅延性・安定性・実装の見通しを犠牲にしてはならない。

---

## 開発対象

### プラグイン名

**VoxChord**

### 対象環境

- OS: Windows 11
- 開発言語: C++
- フレームワーク: JUCE
- ビルドシステム: CMake
- IDE / エディタ:
  - Visual Studio
  - VS Code
- 出力形式:
  - VST3
  - Standalone

### 想定用途

- ライブ演奏
- ボーカル入力を MIDI キーボードで和声化
- 低遅延でのリアルタイム処理
- DAW 上での利用
- DAW を使わない Standalone アプリとしての利用

---

## MVP の仕様

まずは、以下の最小仕様を目標とする。

### 入出力

- 入力: 単声ボーカル
- 出力: ステレオ
- MIDI 入力: 必須
- オーディオ入力チャンネル:
  - まずは mono 入力を基本とする
  - stereo 入力の場合は、必要に応じて L/R を mono にまとめる

### ハーモニー生成

- MIDI ノートで出力音高を制御する
- 押されている MIDI ノートに対応する複数声を生成する
- 最大ボイス数はまず 4 voices
- 入力ボーカルは monophonic vocal を前提とする
- ポリフォニック音声入力には対応しない

### 基本パラメータ

MVP では、以下の程度に絞る。

- Voice Count
- Tune / Tightness
- Glide
- Character
- Spread
- Dry/Wet
- Output Level
- Panic / All Notes Off

可能なら 1 画面・8 ノブ程度で完結させる。

---

## 入れない機能

以下の機能は、少なくとも初期段階では実装しない。

- コード自動判定
- キー / スケール自動ハーモニー
- ピアノロール編集
- 大量のプリセット
- ピッチ補正エディタ
- AI 声質変換
- ポリフォニック音声入力対応
- 歌詞解析
- 母音解析
- 高度な声道モデル
- 重いニューラルネットワーク処理
- 内蔵リバーブ
- 内蔵ディレイ
- 高品質 offline render 専用モード
- マスタリング系処理
- 複雑なモジュレーションマトリクス

これらは、ライブ演奏に必要な最小機能が安定してから検討する。

---

## DSP 設計方針

### 基本思想

DSP は、リアルタイム動作を最優先する。音質のために重い処理を導入する場合でも、まずレイテンシと CPU 使用率への影響を明示する。

### 避けるべきこと

- `processBlock()` 内での不要なメモリ確保
- `processBlock()` 内でのファイル I/O
- `processBlock()` 内でのロック取得
- `processBlock()` 内での重い探索処理
- サンプル単位での不要なオブジェクト生成
- 例外に依存したリアルタイム処理
- GUI スレッドと audio thread の危険な共有
- audio thread から直接 GUI を更新すること

### 推奨

- 事前確保
- lock-free または atomic なパラメータ参照
- smoothing の利用
- サンプルレート変更時の明確な再初期化
- prepare / reset / releaseResources の責務分離
- デバッグしやすい小さな DSP クラス分割

---

## ピッチ処理の方針

VoxChord の中核は、入力ボーカルを MIDI ノートに合わせてリアルタイムに変換する処理である。

初期段階では、以下のような構成を想定する。

1. 入力ボーカルの基本周波数を推定する
2. 押されている MIDI ノートの周波数を取得する
3. 入力ピッチと目標ピッチの比率を計算する
4. 各ボイスを目標ピッチに向けてピッチシフトする
5. 必要に応じて Glide / Tune の挙動を加える
6. 各ボイスに軽い Character / Spread を付与する
7. Dry と Wet をミックスする

ただし、ピッチ検出やピッチシフトの方式は、まず動く最小実装を優先する。高品質化は段階的に行う。

---

## ハーモニーボイス設計

各ボイスは以下の情報を持つ。

- active 状態
- MIDI note number
- target frequency
- current frequency
- pitch ratio
- gain
- pan
- delay offset
- detune amount
- character amount

最初は最大 4 voices とし、MIDI ノートオンの順に割り当てる。ノートオフ時には該当ボイスを停止する。

### MIDI 処理

- Note On で voice を生成または更新する
- Note Off で該当 voice を解放する
- All Notes Off / Panic を必ず用意する
- Sustain pedal 対応は初期段階では必須ではない
- MIDI stuck 対策を重視する

---

## GUI 設計方針

ライブ用なので、GUI は複雑にしない。

### 方針

- 1 画面で完結
- 大きめのノブ / スライダー
- 暗い環境でも視認しやすい
- 現在の MIDI 入力状態が分かる
- 入力レベルと出力レベルが分かる
- クリップが分かる
- Panic ボタンを押しやすい位置に置く

### 初期 GUI 案

- 上部: VoxChord ロゴ / plugin name
- 中央: 主要ノブ
  - Voice Count
  - Tune
  - Glide
  - Character
  - Spread
  - Dry/Wet
  - Output
- 下部:
  - MIDI note indicator
  - Input meter
  - Output meter
  - Panic button

---

## Standalone 対応方針

VoxChord は VST3 と同時に Standalone 版も開発する。

### 最小対応

まずは JUCE の Standalone ビルドを有効にし、同一の `AudioProcessor` を VST3 と Standalone で共有する。

CMake では、出力形式に `VST3` と `Standalone` を含める。

### Standalone で必要な機能

ライブ用途では、Standalone 版に以下が必要になる。

- オーディオデバイス選択
- ASIO / WASAPI の利用
- バッファサイズ設定
- サンプルレート表示
- MIDI デバイス選択
- 入力メーター
- 出力メーター
- クリップ表示
- Panic ボタン
- 前回設定の保存と復元

ただし、最初から全て作り込まず、まずは起動・入出力・MIDI 制御の確認を優先する。

---

## CMake / ビルド方針

### 基本

- CMake を用いる
- Visual Studio 2022 x64 でビルドする
- VST3 と Standalone の両方をビルド対象にする
- `juce_dsp` が必要な場合は明示的にリンクする

### パスに関する注意

Windows では、開発作業ディレクトリは OneDrive 外、かつ ASCII パスに置く。

推奨例:

```text
C:\dev\VoxChord
```

避ける例:

```text
C:\Users\<User>\OneDrive\ドキュメント\...
C:\Users\<User>\デスクトップ\...
```

CMake キャッシュが壊れた場合や別パス由来の設定が残った場合は、`build` ディレクトリを削除して configure からやり直す。

---

## コーディング方針

### 基本方針

- まずビルドが通ること
- 次に DAW / Standalone で起動すること
- 次に音が出ること
- 次に MIDI で制御できること
- 最後に音質を上げること

### 変更単位

一度に大規模変更しない。  
機能追加・リファクタ・バグ修正はなるべく分ける。

### エージェントへの要求

コーディングエージェントは、作業後に必ず以下を報告すること。

- 変更したファイル
- 追加したクラス
- 変更の目的
- ビルド結果
- 未確認事項
- 既知の問題
- 次に確認すべきこと

### 勝手にやってはいけないこと

- プロジェクトの目的を変える
- 多機能作曲支援プラグインに寄せる
- 重い AI / ML 処理を導入する
- 不要なエフェクトを追加する
- UI を複雑にする
- 既存の動作確認済み部分を大きく壊す
- ビルド未確認の状態で「完了」と主張する
- DSP の根幹を変えたのに説明しない
- レイテンシが増える変更を無断で入れる

---

## パラメータ管理

パラメータ管理には JUCE の `AudioProcessorValueTreeState` を用いる。

### 方針

- DAW でオートメーション可能にする
- プリセット / 状態保存に対応する
- VST3 と Standalone で同じ状態管理を使う
- パラメータ ID は後から変えない

### パラメータ ID の例

```text
voiceCount
tune
glide
character
spread
dryWet
outputLevel
```

`Panic` は通常の連続パラメータではなく、ボタン操作または一時的な command として扱うことを検討する。

---

## レイテンシ方針

VoxChord はライブ用なので、レイテンシを最重要指標の一つとする。

### 方針

- 可能な限り低レイテンシで動作させる
- 長い lookahead を前提にしない
- 高品質化のために大きなバッファを要求しない
- レイテンシが増える処理を入れる場合は必ず明示する
- DAW に報告する latency samples が必要な場合は正しく設定する

### 許容感覚

初期目標として、バッファサイズ 64〜128 samples 程度で実用的に動くことを目指す。

---

## 品質確認

### 最低限の確認

各変更後、可能な範囲で以下を確認する。

- Debug ビルドが通る
- Release ビルドが通る
- VST3 が生成される
- Standalone exe が生成される
- DAW 上でプラグインが認識される
- Standalone が起動する
- 音声入力が通る
- MIDI 入力が反応する
- Panic が効く
- ノートオン / ノートオフで音が破綻しない
- パラメータ変更でクラッシュしない
- バイパス時に異常がない
- 音量が極端に大きくならない

### ライブ観点での確認

- ノイズが出ない
- クリックが出ない
- ノート切り替え時に破綻しない
- CPU 使用率が極端に上がらない
- GUI 操作で音が途切れない
- MIDI stuck 時に復帰できる
- 入出力レベルが確認できる

---

## 開発ロードマップ

### Phase 0: 環境確認

完了済み。  
簡単な 3 バンド EQ を実装し、ビルドおよび DAW 上での動作確認が完了している。

### Phase 1: VoxChord の骨格作成

- プロジェクト名を VoxChord にする
- VST3 + Standalone ビルドを有効化する
- AudioProcessor / Editor の基本構造を整理する
- MIDI 入力を受け取れるようにする
- パラメータ管理を APVTS で用意する

### Phase 2: MIDI ボイス管理

- Note On / Note Off を処理する
- 最大 4 voices の voice allocation を実装する
- Panic / All Notes Off を実装する
- MIDI note indicator を表示する

### Phase 3: 最小 DSP

- 入力音声を通す
- 仮のピッチシフトまたは簡易な音高変換を実装する
- MIDI ノートに応じて出力音高を変える
- Dry/Wet を実装する
- Output Level を実装する

### Phase 4: ライブ用の質感調整

- Tune / Tightness
- Glide
- Spread
- Character
- Voice level / pan
- クリック低減
- パラメータ smoothing

### Phase 5: Standalone 実用化

- オーディオデバイス設定
- MIDI デバイス設定
- 設定保存
- メーター
- クリップ表示

### Phase 6: 配布準備

- Release ビルド
- VST3 配置確認
- Standalone exe 確認
- README
- ライセンス表記
- 既知の問題
- 簡単な使い方
- デモ動画または音声

---

## 目標とする音の方向性

VoxChord は、自然なバックコーラス生成よりも、デジタルで神聖なクワイア感を目指す。

### キーワード

- digital choir
- playable vocal harmony
- MIDI-controlled voice
- hard-tuned choir
- low-latency vocal instrument
- Messina-inspired texture
- Prismizer-like harmony

### ただし避けること

- 特定アーティスト名や既存機材名を製品名・UI・README で前面に出しすぎない
- 完全再現を主張しない
- 商標やブランドに依存した表現をしない

---

## ライセンスと公開方針

完成後は Windows 用 VST3 / Standalone として無料公開する予定である。

### 方針

- 依存ライブラリのライセンスに注意する
- 商用プラグイン SDK や再配布制限のあるコードを不用意に含めない
- コード公開の有無は後で判断する
- 少なくともバイナリの無料配布を想定する

---

## エージェント作業時の基本ルール

作業前に、まず現在の目的を確認する。

以下のどれに該当するかを明示してから作業すること。

```text
- ビルド修正
- VST3 対応
- Standalone 対応
- MIDI 処理
- DSP 実装
- GUI 実装
- パラメータ追加
- バグ修正
- リファクタ
- 配布準備
```

作業後は、必ず以下を報告する。

```text
- 実施内容
- 変更ファイル
- ビルド確認の有無
- 実行確認の有無
- 未確認事項
- 次にやるべきこと
```

ビルド確認をしていない場合は、必ず「未ビルド」と明記する。

---

## 現在の最優先タスク

次に着手するべきことは、以下である。

1. 新しい VoxChord プロジェクトの骨格を作る
2. VST3 と Standalone の両方をビルド対象にする
3. MIDI 入力を受け取れることを確認する
4. AudioProcessorValueTreeState で MVP パラメータを定義する
5. Panic / All Notes Off の設計を入れる
6. まずは入力音をそのまま通す状態でビルド確認する

いきなり高品質なピッチシフターを実装しない。  
まずは、**VST3 / Standalone / MIDI / パラメータ / 音声スルー** の骨格を確実に通す。

---

## 一文でのプロジェクト定義

**VoxChord は、入力ボーカルを MIDI キーボードでリアルタイムに和声化する、ライブ演奏特化の軽量デジタルクワイア・プラグインである。**
