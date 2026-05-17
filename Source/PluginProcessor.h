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
    int getCurrentVoiceLimit() const noexcept;
    const voxchord::LevelMeterState& getLevelMeterState() const noexcept { return meters; }

private:
    static float calculatePeak (const juce::AudioBuffer<float>& buffer, int channels, int samples) noexcept;

    int getVoiceLimit() const noexcept;
    float getDryWet() const noexcept;
    float getOutputGain() const noexcept;
    float getSpread() const noexcept;
    float getTune() const noexcept;
    float getGlide() const noexcept;
    float getCharacter() const noexcept;

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
    juce::SmoothedValue<float> dryWetSmoothed { 0.0f };
    juce::SmoothedValue<float> outputGainSmoothed { 1.0f };

    std::array<std::atomic<int>, voxchord::MidiVoiceState::maxVoices> activeMidiNotes;
    std::atomic<int> lastMidiActivity { static_cast<int> (MidiActivity::none) };
    std::atomic<uint32_t> midiActivityCounter { 0 };
    std::atomic<bool> panicRequested { false };

    std::atomic<float>* dryWetParameter = nullptr;
    std::atomic<float>* voiceCountParameter = nullptr;
    std::atomic<float>* tuneParameter = nullptr;
    std::atomic<float>* glideParameter = nullptr;
    std::atomic<float>* characterParameter = nullptr;
    std::atomic<float>* spreadParameter = nullptr;
    std::atomic<float>* outputLevelParameter = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxChordAudioProcessor)
};
