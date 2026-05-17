#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "LevelMeterState.h"
#include "MidiVoiceState.h"
#include "VoxChordParameters.h"

class VoxChordAudioProcessor final : public juce::AudioProcessor
{
public:
    using APVTS = juce::AudioProcessorValueTreeState;

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
    const voxchord::LevelMeterState& getLevelMeterState() const noexcept { return meters; }

private:
    static float calculatePeak (const juce::AudioBuffer<float>& buffer, int channels, int samples) noexcept;

    int getVoiceLimit() const noexcept;
    float getOutputGain() const noexcept;

    void handleMidi (const juce::MidiBuffer& midiMessages) noexcept;
    void publishMidiSnapshot() noexcept;
    void processAudioPassThrough (juce::AudioBuffer<float>& buffer) noexcept;

    APVTS apvts;
    voxchord::MidiVoiceState midiVoices;
    voxchord::LevelMeterState meters;

    std::array<std::atomic<int>, voxchord::MidiVoiceState::maxVoices> activeMidiNotes;
    std::atomic<bool> panicRequested { false };

    std::atomic<float>* voiceCountParameter = nullptr;
    std::atomic<float>* outputLevelParameter = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxChordAudioProcessor)
};

