#include "MidiVoiceState.h"

namespace voxchord
{
namespace
{
    int clampVoiceLimit (int voiceLimit) noexcept
    {
        return juce::jlimit (1, MidiVoiceState::maxVoices, voiceLimit);
    }

    float midiNoteToFrequency (int midiNote) noexcept
    {
        return static_cast<float> (juce::MidiMessage::getMidiNoteInHertz (midiNote));
    }
}

void MidiVoiceState::reset() noexcept
{
    for (auto& voice : voices)
        clearVoice (voice);

    ageCounter = 0;
}

void MidiVoiceState::enforceVoiceLimit (int voiceLimit) noexcept
{
    const auto limit = clampVoiceLimit (voiceLimit);

    for (auto index = limit; index < maxVoices; ++index)
        clearVoice (voices[static_cast<size_t> (index)]);
}

void MidiVoiceState::handleMidiMessage (const juce::MidiMessage& message, int voiceLimit) noexcept
{
    if (message.isNoteOn())
    {
        noteOn (message.getNoteNumber(), message.getFloatVelocity(), voiceLimit);
        return;
    }

    if (message.isNoteOff())
    {
        noteOff (message.getNoteNumber());
        return;
    }

    if (message.isAllNotesOff() || message.isAllSoundOff() || message.isResetAllControllers())
        reset();
}

MidiVoiceState::NoteSnapshot MidiVoiceState::getActiveNotes() const noexcept
{
    NoteSnapshot notes {};
    notes.fill (-1);

    for (auto index = 0; index < maxVoices; ++index)
    {
        const auto& voice = voices[static_cast<size_t> (index)];
        notes[static_cast<size_t> (index)] = voice.active ? voice.midiNote : -1;
    }

    return notes;
}

void MidiVoiceState::noteOn (int midiNote, float velocity, int voiceLimit) noexcept
{
    const auto limit = clampVoiceLimit (voiceLimit);

    for (auto index = 0; index < limit; ++index)
    {
        auto& voice = voices[static_cast<size_t> (index)];

        if (voice.active && voice.midiNote == midiNote)
        {
            voice.gain = velocity;
            voice.targetFrequency = midiNoteToFrequency (midiNote);
            voice.currentFrequency = voice.targetFrequency;
            voice.age = ++ageCounter;
            return;
        }
    }

    auto selectedIndex = 0;

    for (auto index = 0; index < limit; ++index)
    {
        if (! voices[static_cast<size_t> (index)].active)
        {
            selectedIndex = index;
            break;
        }

        if (voices[static_cast<size_t> (index)].age < voices[static_cast<size_t> (selectedIndex)].age)
            selectedIndex = index;
    }

    auto& voice = voices[static_cast<size_t> (selectedIndex)];
    voice.active = true;
    voice.midiNote = midiNote;
    voice.targetFrequency = midiNoteToFrequency (midiNote);
    voice.currentFrequency = voice.targetFrequency;
    voice.pitchRatio = 1.0f;
    voice.gain = velocity;
    voice.pan = 0.0f;
    voice.delayOffsetSamples = 0.0f;
    voice.detuneCents = 0.0f;
    voice.character = 0.0f;
    voice.age = ++ageCounter;
}

void MidiVoiceState::noteOff (int midiNote) noexcept
{
    for (auto& voice : voices)
    {
        if (voice.active && voice.midiNote == midiNote)
            clearVoice (voice);
    }
}

void MidiVoiceState::clearVoice (MidiVoice& voice) noexcept
{
    voice = {};
}

} // namespace voxchord

