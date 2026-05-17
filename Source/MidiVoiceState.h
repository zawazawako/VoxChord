#pragma once

#include <array>

#include <juce_audio_basics/juce_audio_basics.h>

namespace voxchord
{
struct MidiVoice
{
    bool active = false;
    int midiNote = -1;
    float targetFrequency = 0.0f;
    float currentFrequency = 0.0f;
    float pitchRatio = 1.0f;
    float gain = 0.0f;
    float pan = 0.0f;
    float delayOffsetSamples = 0.0f;
    float detuneCents = 0.0f;
    float character = 0.0f;
    int age = 0;
};

class MidiVoiceState final
{
public:
    static constexpr int maxVoices = 4;
    using NoteSnapshot = std::array<int, maxVoices>;

    void reset() noexcept;
    void enforceVoiceLimit (int voiceLimit) noexcept;
    void handleMidiMessage (const juce::MidiMessage& message, int voiceLimit) noexcept;

    NoteSnapshot getActiveNotes() const noexcept;

private:
    void noteOn (int midiNote, float velocity, int voiceLimit) noexcept;
    void noteOff (int midiNote) noexcept;
    void clearVoice (MidiVoice& voice) noexcept;

    std::array<MidiVoice, maxVoices> voices {};
    int ageCounter = 0;
};

} // namespace voxchord

