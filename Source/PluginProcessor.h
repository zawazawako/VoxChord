#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "LevelMeterState.h"
#include "MidiVoiceState.h"
#include "SimpleChoirEngine.h"
#include "VoxChordParameters.h"

class VoxChordAudioProcessor final : public juce::AudioProcessor
{
public:
    using APVTS = juce::AudioProcessorValueTreeState;

    enum class MidiActivity
    {
        none = 0,
        noteOn,
        noteOff,
        allNotesOff,
        panic
    };

    struct MidiActivitySnapshot
    {
        MidiActivity activity = MidiActivity::none;
        uint32_t counter = 0;
    };

    struct MidiInputDebugSnapshot
    {
        uint32_t processBlockCounter = 0;
        uint32_t nonEmptyBlockCounter = 0;
        uint32_t totalEventCounter = 0;
        int lastBlockEventCount = 0;
    };

    enum class InputSource
    {
        autoDetect = 0,
        input1,
        input2,
        mix12
    };

    VoxChordAudioProcessor();
    ~VoxChordAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    APVTS& getValueTreeState() noexcept { return apvts; }
    const APVTS& getValueTreeState() const noexcept { return apvts; }

    void panic() noexcept;
    void clearClipFlags() noexcept;

    voxchord::MidiVoiceState::NoteSnapshot getActiveMidiNotes() const noexcept;
    MidiActivitySnapshot getMidiActivitySnapshot() const noexcept;
    MidiInputDebugSnapshot getMidiInputDebugSnapshot() const noexcept;
    int getCurrentVoiceLimit() const noexcept;
    float getDetectedInputPitchHz() const noexcept;
    voxchord::PitchState getPitchState() const noexcept;
    voxchord::PitchShifterSelfTestSummary getPitchShifterSelfTestSummary() const noexcept;
    const voxchord::LevelMeterState& getLevelMeterState() const noexcept { return meters; }
    bool isMonoOutputEnabledForUi() const noexcept;
    bool isPsolaEnabledForUi() const noexcept;

private:
    static float calculatePeak (const juce::AudioBuffer<float>& buffer, int channels, int samples) noexcept;
    static float calculateChannelPeak (const juce::AudioBuffer<float>& buffer, int channel, int samples) noexcept;

    int getVoiceLimit() const noexcept;
    float getDryWet() const noexcept;
    float getInputGain() const noexcept;
    float getOutputGain() const noexcept;
    float getSpread() const noexcept;
    float getTune() const noexcept;
    float getGlide() const noexcept;
    float getCharacter() const noexcept;
    int getCharacterModeRaw() const noexcept;
    int getCharacterMode() const noexcept;
    bool getLeadTuneEnabled() const noexcept;
    bool getPsolaEnabled() const noexcept;
    bool getMonoOutputEnabled() const noexcept;
    InputSource getInputSource() const noexcept;

    void handleMidi (const juce::MidiBuffer& midiMessages) noexcept;
    void publishMidiActivity (MidiActivity activity) noexcept;
    void publishMidiSnapshot() noexcept;
    void copyInputToDryBuffer (const juce::AudioBuffer<float>& buffer) noexcept;
    void mixDryWetToOutput (juce::AudioBuffer<float>& buffer) noexcept;

    APVTS apvts;
    voxchord::MidiVoiceState midiVoices;
    voxchord::SimpleChoirEngine choirEngine;
    voxchord::LevelMeterState meters;
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> wetBuffer;
    juce::AudioBuffer<float> tunedLeadBuffer;
    juce::SmoothedValue<float> dryWetSmoothed { 0.0f };
    juce::SmoothedValue<float> leadTuneDryMixSmoothed { 0.0f };
    juce::SmoothedValue<float> characterAmountSmoothed { 0.0f };
    juce::SmoothedValue<float> inputGainSmoothed { 1.0f };
    juce::SmoothedValue<float> outputGainSmoothed { 1.0f };
    juce::SmoothedValue<float> monoOutputSmoothed { 0.0f };

    std::array<std::atomic<int>, voxchord::MidiVoiceState::maxVoices> activeMidiNotes;
    std::atomic<int> lastMidiActivity { static_cast<int> (MidiActivity::none) };
    std::atomic<uint32_t> midiActivityCounter { 0 };
    std::atomic<uint32_t> midiProcessBlockCounter { 0 };
    std::atomic<uint32_t> midiNonEmptyBlockCounter { 0 };
    std::atomic<uint32_t> midiTotalEventCounter { 0 };
    std::atomic<int> midiLastBlockEventCount { 0 };
    std::atomic<bool> panicRequested { false };
    std::atomic<float> detectedInputPitchHz { 0.0f };
    std::atomic<float> inputRmsDb { -100.0f };
    std::atomic<float> rawPitchHz { 0.0f };
    std::atomic<float> correctedPitchHz { 0.0f };
    std::atomic<float> displayStablePitchHz { 0.0f };
    std::atomic<float> correctionInputPitchHz { 0.0f };
    std::atomic<float> stablePitchHz { 0.0f };
    std::atomic<float> harmonyPitchHz { 0.0f };
    std::atomic<float> ratioSmoothingCoefficient { 0.0f };
    std::atomic<float> characterAmountRaw { 0.0f };
    std::atomic<float> characterAmountSmoothedValue { 0.0f };
    std::atomic<float> characterDeltaRms { 0.0f };
    std::atomic<float> characterDeltaPeak { 0.0f };
    std::atomic<float> characterDeltaRatioDb { -100.0f };
    std::atomic<float> pitchConfidence { 0.0f };
    std::atomic<bool> pitchVoiced { false };
    std::atomic<int> harmonicCorrectionMode { 0 };
    std::atomic<int> characterModeRaw { 0 };
    std::atomic<int> characterModeSanitized { 0 };

    // D1 low-pitch diagnostics (observation only, see directions/0703_1.md)
    std::atomic<float> windowPitchHz { 0.0f };
    std::atomic<int> representativeVoiceMidiNote { -1 };
    std::atomic<int> representativeGrainWindowSamples { 0 };
    std::atomic<float> representativePitchRatioRaw { 0.0f };
    std::atomic<float> representativePitchRatioClamped { 0.0f };
    std::atomic<float> outputPeriodToWindowRatio { 0.0f };
    std::atomic<uint32_t> ratioClampHitCount { 0 };
    std::atomic<float> wetZeroCrossingHz { 0.0f };
    std::atomic<float> wetZeroCrossingCentsDeviation { 0.0f };

    std::atomic<float>* dryWetParameter = nullptr;
    std::atomic<float>* voiceCountParameter = nullptr;
    std::atomic<float>* tuneParameter = nullptr;
    std::atomic<float>* glideParameter = nullptr;
    std::atomic<float>* characterParameter = nullptr;
    std::atomic<float>* characterModeParameter = nullptr;
    std::atomic<float>* spreadParameter = nullptr;
    std::atomic<float>* outputLevelParameter = nullptr;
    std::atomic<float>* inputGainParameter = nullptr;
    std::atomic<float>* inputSourceParameter = nullptr;
    std::atomic<float>* leadTuneEnabledParameter = nullptr;
    std::atomic<float>* monoOutputEnabledParameter = nullptr;
    std::atomic<float>* psolaEnabledParameter = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxChordAudioProcessor)
};
