# VoxChord Implementation Plan

## 1. 作業目的

この設計書は、AGENTS.md の仕様を実装作業に落とし込むための実装用 PLAN である。

今回の主目的は以下に該当する。

- VST3 対応
- Standalone 対応
- MIDI 処理
- パラメータ追加
- DSP 実装
- GUI 実装
- リファクタ

最初の到達点は、ピッチシフト品質ではなく、以下の骨格を確実にビルドして動作確認できる状態にすること。

- プロジェクト名を VoxChord にする
- VST3 と Standalone を同時にビルドする
- MIDI 入力を受け取る
- APVTS で MVP パラメータを保持する
- Panic / All Notes Off の処理入口を用意する
- 入力音声をそのままステレオ出力へ通す

## 2. 現状把握

現在のリポジトリは JUCE の最小プラグインテンプレートに近い状態である。

- `CMakeLists.txt` は `MyFirstPlugin` のまま
- `FORMATS` は `VST3` のみ
- `NEEDS_MIDI_INPUT` が未指定
- `PRODUCT_NAME` は `MyFirstPlugin`
- `PluginProcessor.*` は MIDI を無視している
- APVTS は未導入
- state save / restore は未実装
- `PluginEditor.*` は Hello World 表示のみ
- 音声処理は実質スルーだが、mono 入力を stereo 出力へ複製する VoxChord 仕様にはまだなっていない

補足: 端末上では `AGENTS.md` が文字化けして表示されるため、ファイルは UTF-8 として保存し直すことを推奨する。ただし、本 PLAN はユーザー提示の AGENTS.md 内容を正として作成する。

## 3. 実装方針

VoxChord はライブ用の低遅延ボーカル楽器である。したがって、初期段階では音質よりも、ビルド可能性、起動安定性、MIDI 応答、音声スルー、パラメータ保存を優先する。

守るべき制約は以下。

- `processBlock()` 内でメモリ確保しない
- `processBlock()` 内でロックしない
- `processBlock()` 内でファイル I/O しない
- GUI スレッドから audio thread の状態を直接壊さない
- audio thread から GUI を直接更新しない
- 初期 MVP では lookahead を導入しない
- 初期 MVP の報告 latency は 0 samples とする
- ピッチシフターは Phase 3 以降に段階導入する

## 4. 目標アーキテクチャ

### 4.1 ファイル構成

短期的には既存ファイルを活かし、必要な小クラスを追加する。

```text
CMakeLists.txt
PluginProcessor.h
PluginProcessor.cpp
PluginEditor.h
PluginEditor.cpp
PLAN.md
Source/
  MidiVoiceState.h          # Phase 2 で追加
  MidiVoiceState.cpp        # Phase 2 で追加
  LevelMeterState.h         # Phase 2 or 4 で追加
  SimplePitchVoice.h        # Phase 3 で追加
  SimplePitchVoice.cpp      # Phase 3 で追加
```

改善案: 最初から大きく `Source/` 移動すると CMake と include の変更量が増えるため、Phase 1 では既存ファイル名のまま進める。Phase 2 以降で小クラスだけ `Source/` に追加する。

### 4.2 クラス名

最終的には以下へ rename する。

- `AudioPluginAudioProcessor` -> `VoxChordAudioProcessor`
- `AudioPluginAudioProcessorEditor` -> `VoxChordAudioProcessorEditor`

ただし、最初のビルド通過を優先する場合は、CMake rename と APVTS 導入を先に行い、クラス rename は同じ PR/作業単位内で小さく済ませる。

### 4.3 Processor の責務

`VoxChordAudioProcessor` は以下を担当する。

- JUCE AudioProcessor の lifecycle 管理
- bus layout 判定
- APVTS の所有
- MIDI note on/off/panic の受信
- voice state の所有
- audio pass-through / dry-wet / output gain
- meter 用 atomic 値の更新
- state save / restore

Processor に GUI 部品や重い描画状態を持たせない。

### 4.4 Editor の責務

`VoxChordAudioProcessorEditor` は以下を担当する。

- APVTS attachment による knob / slider 表示
- Panic button の発火
- MIDI note indicator の表示
- input/output meter の表示
- clip indicator の表示

Editor は DSP ロジックを持たない。

## 5. CMake 設計

### 5.1 project

```cmake
project(VoxChord VERSION 0.1.0)
```

### 5.2 juce_add_plugin

短期 MVP の設定案。

```cmake
juce_add_plugin(VoxChord
    COMPANY_NAME "VoxChord"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Vxcd
    PLUGIN_CODE VxC1
    FORMATS VST3 Standalone
    PRODUCT_NAME "VoxChord")
```

注意: JUCE の plugin/manufacturer code は 4 文字制約がある。ホスト互換性のため、英数字のみで固定し、後から変更しない。

### 5.3 linked modules

Phase 1 では以下で十分。

```cmake
target_link_libraries(VoxChord
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
```

Phase 3 で `juce_dsp` を使う場合のみ追加する。

```cmake
juce::juce_dsp
```

## 6. APVTS パラメータ設計

パラメータ ID は最初に固定し、後から変更しない。

| ID | 表示名 | 型 | 範囲 | 初期値 | 用途 |
| --- | --- | --- | --- | --- | --- |
| `voiceCount` | Voice Count | int choice or int range | 1-4 | 4 | 同時発音する wet voice 上限 |
| `tune` | Tune | float | 0.0-1.0 | 0.8 | 目標 pitch への追従度 |
| `glide` | Glide | float | 0.0-1.0 | 0.15 | MIDI note 変更時の pitch smoothing |
| `character` | Character | float | 0.0-1.0 | 0.35 | 声ごとの質感差 |
| `spread` | Spread | float | 0.0-1.0 | 0.55 | stereo pan 幅 |
| `dryWet` | Dry/Wet | float | 0.0-1.0 | 0.5 | dry と wet のミックス |
| `outputLevel` | Output | float | -24.0 to +6.0 dB | -3.0 dB | 最終出力ゲイン |

改善案: MVP では `voiceCount` を `AudioParameterInt` にする。GUI 側で 1, 2, 3, 4 の表示にすれば十分で、choice 変更による将来の互換性問題を避けやすい。

### 6.1 Panic の扱い

`panic` は APVTS の連続パラメータにしない。理由は以下。

- DAW automation される通常パラメータではない
- 一瞬だけ発火する command である
- state save に保存されるべき値ではない

実装案。

```cpp
void panic();
void allNotesOff();
```

Editor の button click から `processor.panic()` を呼ぶ。audio thread との競合を避けるため、内部は lock-free に近い単純操作にする。Phase 1 では Processor 側に `std::atomic<bool> panicRequested` を置き、`processBlock()` 先頭で consume して voice をクリアする。

## 7. MIDI ボイス設計

### 7.1 VoiceState

Phase 2 で追加する最小構造。

```cpp
struct MidiVoice
{
    bool active = false;
    int midiNote = -1;
    float targetFrequency = 0.0f;
    float currentFrequency = 0.0f;
    float pitchRatio = 1.0f;
    float gain = 0.0f;
    float pan = 0.0f;
    float detuneCents = 0.0f;
    float character = 0.0f;
    int age = 0;
};
```

### 7.2 Voice allocation

最大 voice 数は固定で 4。動的配列は使わず、`std::array<MidiVoice, 4>` を使う。

Note On 処理。

1. velocity 0 の Note On は Note Off として扱う
2. 同じ note が既に active なら、その voice を更新する
3. 空き voice があればそこへ割り当てる
4. 空きがなければ最も古い voice を steal する
5. `targetFrequency = juce::MidiMessage::getMidiNoteInHertz(note)` を設定する

Note Off 処理。

1. 同じ note の active voice を探す
2. 見つかった voice を inactive にする
3. Phase 2 では release envelope は持たず即時停止
4. Phase 4 で click 低減用の短い release を追加する

### 7.3 MIDI stuck 対策

以下を必ず処理する。

- `noteOff`
- `allNotesOff`
- `allSoundOff`
- `resetAllControllers`
- Panic button

改善案: Sustain pedal は MVP では未対応でよいが、CC64 を受けたときに無視するのではなく、コメントで「Phase 5 以降」と明示する。

## 8. Audio Processing Phase 1

### 8.1 入出力仕様

MVP の processBlock は以下を行う。

- MIDI buffer を走査して voice state を更新
- panic request があれば全 voice を停止
- mono 入力なら L/R に複製して stereo 出力
- stereo 入力ならそのまま stereo 出力
- 余剰 output channel は clear
- `outputLevel` を適用
- `dryWet` は Phase 1 では dry 100% と同等でもよいが、パラメータ値は読める状態にする

改善案: Phase 1 では wet 音をまだ作らないため、`dryWet` を 100% wet にしても無音にならないよう、wet 未実装時は dry fallback とする。GUI には "Wet engine pending" ではなく、内部実装コメントに留める。

### 8.2 Bus layout

対応案。

- Input: mono or stereo
- Output: stereo 優先、mono も許可
- mono input + stereo output を許可
- stereo input + stereo output を許可
- output が mono の場合は mono 出力

既存テンプレートは input と output layout が一致しないと false にするため、VoxChord 用に変更する。

### 8.3 Level meter

Phase 1 では Processor に以下の atomic を持たせるだけでよい。

```cpp
std::atomic<float> inputPeak { 0.0f };
std::atomic<float> outputPeak { 0.0f };
std::atomic<bool> inputClipped { false };
std::atomic<bool> outputClipped { false };
```

`processBlock()` で block peak を計算して atomic に store する。GUI は Timer で 30 Hz 程度に読み取る。

## 9. State Save / Restore

APVTS の state を XML として保存する。

実装案。

```cpp
void getStateInformation(juce::MemoryBlock& destData) override
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void setStateInformation(const void* data, int sizeInBytes) override
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}
```

Panic や meter state は保存しない。

## 10. GUI MVP

### 10.1 Layout

1 画面で完結させる。

```text
Header:
  VoxChord
  subtitle: MIDI-controlled digital choir

Main controls:
  Voice Count
  Tune
  Glide
  Character
  Spread
  Dry/Wet
  Output

Status:
  MIDI notes
  Input meter
  Output meter
  Clip indicators
  Panic button
```

### 10.2 Attachment

Editor は以下の attachment を持つ。

```cpp
using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

std::unique_ptr<SliderAttachment> voiceCountAttachment;
std::unique_ptr<SliderAttachment> tuneAttachment;
std::unique_ptr<SliderAttachment> glideAttachment;
std::unique_ptr<SliderAttachment> characterAttachment;
std::unique_ptr<SliderAttachment> spreadAttachment;
std::unique_ptr<SliderAttachment> dryWetAttachment;
std::unique_ptr<SliderAttachment> outputAttachment;
```

改善案: `voiceCount` は slider でも動くが、ライブ操作では段階値のほうが誤操作しにくい。MVP では rotary slider を整数 step にし、Phase 4 以降で segmented control へ変更してもよい。

### 10.3 Panic button

Panic button は大きめに配置する。

```cpp
panicButton.onClick = [this]
{
    processorRef.panic();
};
```

GUI から voice 配列を直接触らない。

### 10.4 MIDI note indicator

Phase 2 では文字列表示で十分。

```text
MIDI: C3 E3 G3
```

Processor 側は active note bitset または 4 voice の snapshot を atomic-friendly に公開する。初期実装では Timer から `getActiveMidiNotes()` を呼ぶ方式でもよいが、ロックを使わない。

## 11. DSP Roadmap

### 11.1 Phase 1: Audio pass-through

目的: VST3 / Standalone / MIDI / APVTS / state / audio I/O の確認。

音声はピッチ変更しない。出力ゲインのみ適用する。

### 11.2 Phase 2: Voice state only

目的: MIDI note と最大 4 voices の割り当て確認。

まだ wet 音は作らず、GUI に active notes を表示する。Panic が GUI と MIDI CC の両方から効くことを確認する。

### 11.3 Phase 3: Temporary wet engine

目的: MIDI に反応して音が変わる最小 DSP を入れる。

候補。

- 短い delay line + modulated read position による簡易 pitch shift
- grain 2 本のクロスフェード方式
- latency を増やさない範囲で軽量にする

この段階では音質よりも、クリックしにくい、CPU が軽い、破綻時に Panic で戻れることを優先する。

### 11.4 Phase 4: Pitch detection

目的: 入力 monophonic vocal の基本周波数を推定する。

候補。

- autocorrelation
- YIN-lite
- zero-crossing は軽いが声には不安定なので補助に留める

改善案: Phase 3 と Phase 4 を分ける。ピッチシフトと pitch detection を同時に入れると、バグ切り分けが難しくなる。

### 11.5 Phase 5: Character / Spread

目的: digital choir らしい質感を軽く足す。

候補。

- voice ごとの pan
- 数 cents の detune
- 数 ms の delay offset
- formant 風の軽い tilt EQ
- saturation は入れるならごく軽く、出力保護を入れる

内蔵 reverb / delay は MVP では入れない。

## 12. Standalone 対応

JUCE の Standalone target を有効にし、同じ `VoxChordAudioProcessor` を使う。

Phase 1 の確認項目。

- Standalone exe が生成される
- audio input device を選べる
- audio output device を選べる
- MIDI input device を選べる
- input signal が output に出る
- Panic button が押せる

改善案: Standalone の詳細設定保存は初期実装では JUCE Standalone wrapper に任せる。独自設定保存は Phase 5 で行う。

## 13. 実装順序

### Step 1: CMake rename

変更対象。

- `CMakeLists.txt`

内容。

- `project(VoxChord VERSION 0.1.0)`
- target name を `VoxChord` に変更
- `FORMATS VST3 Standalone`
- `NEEDS_MIDI_INPUT TRUE`
- `PRODUCT_NAME "VoxChord"`
- target sources / definitions / libraries の target 名を更新

完了条件。

- CMake configure が通る
- target 一覧に VST3 と Standalone が出る

### Step 2: Processor rename and APVTS

変更対象。

- `PluginProcessor.h`
- `PluginProcessor.cpp`
- `PluginEditor.h`
- `PluginEditor.cpp`

内容。

- class 名を `VoxChordAudioProcessor` に変更
- APVTS member を追加
- `createParameterLayout()` を追加
- state save / restore を APVTS に変更
- `acceptsMidi()` が true になることを確認

完了条件。

- Debug build が通る
- plugin scan で VoxChord と表示される

### Step 3: Audio pass-through

変更対象。

- `PluginProcessor.cpp`

内容。

- mono input -> stereo output 複製
- stereo input -> stereo output スルー
- outputLevel 適用
- meter peak 計算
- no allocation / no lock を維持

完了条件。

- Standalone で入力音が出る
- outputLevel 変更で音量が変わる

### Step 4: MIDI voice state

変更対象。

- `PluginProcessor.h`
- `PluginProcessor.cpp`
- 必要なら `Source/MidiVoiceState.*`

内容。

- `std::array<MidiVoice, 4>` 追加
- note on/off/all notes off 処理
- panic 処理
- active note snapshot 取得関数

完了条件。

- MIDI note on/off が voice state に反映される
- Panic で全 voice が inactive になる
- stuck note が復帰できる

### Step 5: GUI MVP

変更対象。

- `PluginEditor.h`
- `PluginEditor.cpp`

内容。

- 7 knobs
- Panic button
- MIDI note indicator
- input/output meter
- dark stage-friendly layout

完了条件。

- 全パラメータが GUI から動く
- Panic button が効く
- meter が概ね反応する

### Step 6: Build verification

確認対象。

- Debug build
- Release build
- VST3 artifact
- Standalone exe artifact

Windows / Visual Studio 2022 x64 想定。

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
cmake --build build --config Release
```

## 14. 受け入れ条件

Phase 1 完了条件。

- `VoxChord` として CMake configure できる
- VST3 が生成される
- Standalone exe が生成される
- MIDI input が有効
- APVTS パラメータが作成される
- state save / restore がクラッシュしない
- mono/stereo input が stereo output に安全に出る
- outputLevel が効く
- Panic の public method が存在する

Phase 2 完了条件。

- Note On で最大 4 voices に割り当てられる
- Note Off で該当 voice が inactive になる
- All Notes Off / All Sound Off / Panic が効く
- GUI に active MIDI notes が表示される
- processBlock 内の allocation / lock がない

## 15. テスト観点

最低限の確認。

- Debug build が通る
- Release build が通る
- VST3 が生成される
- Standalone exe が生成される
- DAW が plugin を認識する
- Standalone が起動する
- 音声入力が通る
- MIDI 入力が反応する
- Panic が効く
- パラメータ変更でクラッシュしない
- 音量が極端に大きくならない

ライブ観点の確認。

- buffer size 64 samples で破綻しない
- buffer size 128 samples で安定する
- GUI 操作中に音が途切れない
- note on/off 連打で stuck しない
- Panic 後に復帰できる
- CPU 使用率が急増しない

## 16. 既知リスクと対策

### 16.1 ピッチシフト品質

リスク: 最小 pitch shifter は choir として粗く聞こえる可能性がある。

対策: Phase 1/2 では pitch shift を入れず、MIDI/voice/state の確認を独立させる。Phase 3 で temporary wet engine と明示する。

### 16.2 レイテンシ増加

リスク: 高品質 pitch shift / pitch detection で latency が増える。

対策: lookahead 前提の方式を避け、latency を増やす変更は PLAN と README に明記する。初期は 0 samples を維持する。

### 16.3 MIDI stuck

リスク: DAW/Standalone で note off を取り逃がすと音が残る。

対策: All Notes Off / All Sound Off / Panic を必ず実装し、GUI 上の Panic を目立たせる。

### 16.4 AGENTS.md encoding

リスク: 端末やエディタで仕様が文字化けし、チーム内で誤読される。

対策: `AGENTS.md` を UTF-8 without BOM または UTF-8 with BOM のどちらかに統一し、VS Code の status bar で確認する。Windows 環境では UTF-8 with BOM のほうが古いツールで安全な場合がある。

## 17. 改善案・具体化案

- `voiceCount` は将来の互換性を考えて `AudioParameterInt` にする
- Panic は APVTS parameter ではなく command として扱う
- Phase 1 では wet 未実装でも dry fallback を維持し、Dry/Wet 操作で無音事故を起こさない
- Pitch shifter と pitch detector は別 phase で入れ、問題切り分けを容易にする
- MIDI voice allocation は固定長 `std::array` にし、processBlock 内 allocation を避ける
- meter は atomic peak 値だけ Processor から公開し、GUI は Timer で読む
- Standalone の詳細設定保存は初期は JUCE wrapper に任せ、独自保存は後回しにする
- GUI は最初から 7 knob + Panic + meter に絞り、プリセットや複雑な routing は入れない
- `AGENTS.md` の文字化け対策を早めに行う

## 18. 次に実装する具体タスク

最初に実装すべき順番。

1. `CMakeLists.txt` を VoxChord / VST3 + Standalone / MIDI input に変更する
2. Processor / Editor の class 名を VoxChord に変更する
3. APVTS と MVP パラメータを追加する
4. state save / restore を APVTS 対応にする
5. mono/stereo input の安全な pass-through を実装する
6. outputLevel を適用する
7. Panic method と `allNotesOff()` を空実装ではなく voice clear 可能な形で用意する
8. Debug build と Release build を確認する

