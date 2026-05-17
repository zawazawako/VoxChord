#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "MidiVoiceState.h"

namespace voxchord
{
class SimpleChoirEngine final
{
public:
    void prepare (int maxBlockSize);
    void reset() noexcept;

    void render (const juce::AudioBuffer<float>& dryInput,
                 juce::AudioBuffer<float>& wetOutput,
                 const MidiVoiceState::NoteSnapshot& activeNotes,
                 int voiceLimit,
                 float spread) noexcept;

private:
    static int countActiveVoices (const MidiVoiceState::NoteSnapshot& activeNotes, int voiceLimit) noexcept;
    static float getPanForVoice (int activeIndex, int activeCount, float spread) noexcept;
    static float readMonoInput (const juce::AudioBuffer<float>& input, int sample) noexcept;

    int preparedBlockSize = 0;
};

} // namespace voxchord

