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
                 float spread,
                 float tune,
                 float glide,
                 float character) noexcept;
    float getLastDetectedInputFrequencyHz() const noexcept { return lastDetectedInputFrequencyHz; }

private:
    struct VoicePitchState
    {
        int lastMidiNote = -1;
        bool wasActive = false;
        float phaseA = 0.0f;
        float phaseB = 0.5f;
        float currentPitchRatio = 1.0f;
        float targetPitchRatio = 1.0f;
    };

    class SimplePitchDetector final
    {
    public:
        void prepare (double sampleRate) noexcept;
        void reset() noexcept;
        float processBlock (const juce::AudioBuffer<float>& input) noexcept;

    private:
        double sampleRateHz = 44100.0;
        int samplesSinceCrossing = 0;
        float smoothedFrequencyHz = 0.0f;
        float previousSample = 0.0f;
        bool wasBelowLowThreshold = false;
    };

    static int countActiveVoices (const MidiVoiceState::NoteSnapshot& activeNotes, int voiceLimit) noexcept;
    static float getPanForVoice (int activeIndex, int activeCount, float spread) noexcept;
    static float getPitchRatioForNote (int midiNote, float inputFrequencyHz) noexcept;
    static float applyTuneToRatio (float pitchRatio, float tune) noexcept;
    static float getGlideCoefficient (float glide, double sampleRate) noexcept;
    static float getCharacterPitchRatio (int slot, float character) noexcept;
    static float getCharacterGain (int slot, float character) noexcept;
    static float getCharacterDelayOffsetSamples (int slot, float character, double sampleRate) noexcept;
    static float readMonoInput (const juce::AudioBuffer<float>& input, int sample) noexcept;
    static float wrapPhase (float phase) noexcept;
    static float windowGain (float phase) noexcept;

    void resetVoice (VoicePitchState& voice) noexcept;
    float readDelayLine (float delaySamples) const noexcept;
    float renderPitchShiftedSample (VoicePitchState& voice,
                                    float glideCoefficient,
                                    float delayOffsetSamples) noexcept;

    static constexpr float fallbackReferenceFrequencyHz = 261.625565f;
    static constexpr float minPitchRatio = 0.25f;
    static constexpr float maxPitchRatio = 8.0f;

    std::array<VoicePitchState, MidiVoiceState::maxVoices> voiceStates {};
    SimplePitchDetector pitchDetector;
    juce::AudioBuffer<float> delayBuffer;
    float lastDetectedInputFrequencyHz = 0.0f;

    int delayBufferSize = 0;
    int writeIndex = 0;
    int pitchWindowSamples = 1024;
    int minimumDelaySamples = 128;
    double currentSampleRate = 44100.0;
};

} // namespace voxchord
