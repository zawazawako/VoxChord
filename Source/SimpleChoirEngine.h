#pragma once

#include <array>

#include <juce_audio_basics/juce_audio_basics.h>

#include "MidiVoiceState.h"

namespace voxchord
{
class SimpleChoirEngine final
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    void render (const juce::AudioBuffer<float>& dryInput,
                 juce::AudioBuffer<float>& wetOutput,
                 const MidiVoiceState::NoteSnapshot& activeNotes,
                 int voiceLimit,
                 float spread) noexcept;

private:
    struct VoicePitchState
    {
        int lastMidiNote = -1;
        bool wasActive = false;
        float phaseA = 0.0f;
        float phaseB = 0.5f;
        juce::SmoothedValue<float> pitchRatio { 1.0f };
    };

    static int countActiveVoices (const MidiVoiceState::NoteSnapshot& activeNotes, int voiceLimit) noexcept;
    static float getPanForVoice (int activeIndex, int activeCount, float spread) noexcept;
    static float getPitchRatioForNote (int midiNote) noexcept;
    static float readMonoInput (const juce::AudioBuffer<float>& input, int sample) noexcept;
    static float wrapPhase (float phase) noexcept;
    static float windowGain (float phase) noexcept;

    void resetVoice (VoicePitchState& voice) noexcept;
    float readDelayLine (float delaySamples) const noexcept;
    float renderPitchShiftedSample (VoicePitchState& voice) noexcept;

    static constexpr float referenceFrequencyHz = 261.625565f;
    static constexpr float minPitchRatio = 0.25f;
    static constexpr float maxPitchRatio = 4.0f;

    std::array<VoicePitchState, MidiVoiceState::maxVoices> voiceStates {};
    juce::AudioBuffer<float> delayBuffer;

    int delayBufferSize = 0;
    int writeIndex = 0;
    int pitchWindowSamples = 1024;
    int minimumDelaySamples = 128;
    double currentSampleRate = 44100.0;
};

} // namespace voxchord
