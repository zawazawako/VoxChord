#pragma once

#include <array>

#include <juce_audio_basics/juce_audio_basics.h>

#include "MidiVoiceState.h"

namespace voxchord
{
struct PitchState
{
    float inputRmsDb = -100.0f;
    float rawPitchHz = 0.0f;
    float correctedPitchHz = 0.0f;
    float displayStablePitchHz = 0.0f;
    float correctionInputPitchHz = 0.0f;
    float stablePitchHz = 0.0f;
    float harmonyPitchHz = 0.0f;
    float ratioSmoothingCoefficient = 0.0f;
    float confidence = 0.0f;
    bool voiced = false;
    int harmonicCorrectionMode = 0;
};

struct PitchShifterSelfTestModeSummary
{
    bool hasRun = false;
    bool passed = false;
    float maxErrorCents = 0.0f;
    float worstInputHz = 0.0f;
    float worstRatio = 0.0f;
    float worstActualRatio = 0.0f;
    float worstMeasuredHz = 0.0f;
};

struct PitchShifterSelfTestSummary
{
    PitchShifterSelfTestModeSummary fixedWindow;
    PitchShifterSelfTestModeSummary inputSyncedWindow;
};

class SimpleChoirEngine final
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;
    static void runPitchDetectorSelfTest();
    static void runPitchShifterSelfTest();
    static PitchShifterSelfTestSummary getPitchShifterSelfTestSummary() noexcept;

    void render (const juce::AudioBuffer<float>& dryInput,
                 juce::AudioBuffer<float>& wetOutput,
                 const MidiVoiceState::NoteSnapshot& activeNotes,
                 int voiceLimit,
                 float spread,
                 float tune,
                 float glide,
                 float character) noexcept;
    float getLastDetectedInputFrequencyHz() const noexcept { return lastDetectedInputFrequencyHz; }
    PitchState getPitchState() const noexcept { return pitchState; }

private:
    struct VoicePitchState
    {
        int lastMidiNote = -1;
        bool wasActive = false;
        float phaseA = 0.0f;
        float phaseB = 0.5f;
        float currentPitchRatio = 1.0f;
        float targetPitchRatio = 1.0f;
        int windowSamplesA = 1024;
        int windowSamplesB = 1024;
    };

    class SimplePitchDetector final
    {
    public:
        void prepare (double sampleRate) noexcept;
        void reset() noexcept;
        void setHarmonicCorrectionEnabled (bool shouldBeEnabled) noexcept;
        PitchState processBlock (const juce::AudioBuffer<float>& input) noexcept;

    private:
        void analyseFrame() noexcept;
        float detectPitchYin() noexcept;
        float correctHarmonicPitch (float rawPitchHz) noexcept;
        float applyMedianFilter (float correctedPitchHz) noexcept;
        bool shouldAcceptCandidate (float candidatePitchHz) noexcept;
        void updateStablePitch (float candidatePitchHz) noexcept;
        void updateCorrectionInputPitch (float targetPitchHz) noexcept;
        static float computeRmsDb (const std::array<float, 2048>& frame) noexcept;
        static float getParabolicOffset (float previous, float current, float next) noexcept;
        static float centsBetween (float a, float b) noexcept;
        static float smoothFrequencyLog (float previous, float target, float alpha) noexcept;

        static constexpr int frameLength = 2048;
        static constexpr int hopSize = 512;
        static constexpr float minFrequencyHz = 80.0f;
        static constexpr float maxFrequencyHz = 900.0f;
        static constexpr float rmsGateDb = -45.0f;
        static constexpr float confidenceThreshold = 0.75f;
        static constexpr float veryHighConfidenceThreshold = 0.9f;
        static constexpr float smoothingAlpha = 0.2f;
        static constexpr float correctionFastAttackAlpha = 0.7f;
        static constexpr float holdTimeMs = 100.0f;
        static constexpr float yinThreshold = 1.0f - confidenceThreshold;
        static constexpr float maxJumpCents = 350.0f;
        static constexpr float fallbackMinimumTolerance = 0.02f;
        static constexpr float correctionNearCents = 120.0f;
        static constexpr float repeatedRawCents = 80.0f;
        static constexpr int correctionConfirmationFrames = 2;
        static constexpr int highConfidenceRawFramesForUnlock = 3;
        static constexpr int medianWindowSize = 5;

        double sampleRateHz = 44100.0;
        int writeIndex = 0;
        int samplesUntilAnalysis = hopSize;
        int samplesSinceAccepted = 0;
        int consecutiveJumpFrames = 0;
        int correctionCandidateMode = 0;
        int correctionCandidateFrames = 0;
        int highConfidenceRawFrames = 0;
        int holdSamples = 4410;
        bool harmonicCorrectionEnabled = true;
        float previousRawPitchHz = 0.0f;
        PitchState state;
        std::array<float, frameLength> ringBuffer {};
        std::array<float, frameLength> analysisFrame {};
        std::array<float, frameLength> difference {};
        std::array<float, frameLength> cmndf {};
        std::array<float, medianWindowSize> medianLogBuffer {};
        int medianWriteIndex = 0;
        int medianCount = 0;
    };

    static int countActiveVoices (const MidiVoiceState::NoteSnapshot& activeNotes, int voiceLimit) noexcept;
    static float getPanForVoice (int activeIndex, int activeCount, float spread) noexcept;
    static float getPitchRatioForNote (int midiNote, float inputFrequencyHz) noexcept;
    static float applyTuneToRatio (float pitchRatio, float tune) noexcept;
    static float getGlideCoefficient (float glide, double sampleRate) noexcept;
    static float getCharacterPitchRatio (int slot, float character) noexcept;
    static float getCharacterGain (int slot, float character) noexcept;
    static float getCharacterDelayOffsetSamples (int slot, float character, double sampleRate) noexcept;
    static int getFixedPitchWindowSamples (double sampleRate) noexcept;
    static int getInputSyncedPitchWindowSamples (float inputFrequencyHz, double sampleRate) noexcept;
    static int getLimitedPitchWindowSamples (int currentWindowSamples, int targetWindowSamples) noexcept;
    static float getWindowPitchSmoothingCoefficient (int samples, double sampleRate) noexcept;
    static float smoothFrequencyLog (float previous, float target, float alpha) noexcept;
    static float readMonoInput (const juce::AudioBuffer<float>& input, int sample) noexcept;
    static float wrapPhase (float phase) noexcept;
    static float windowGain (float phase) noexcept;

    float updateWindowPitchHz (float correctionInputPitchHz, int samples) noexcept;
    void resetVoice (VoicePitchState& voice) noexcept;
    float readDelayLine (float delaySamples) const noexcept;
    float renderPitchShiftedSample (VoicePitchState& voice,
                                    float glideCoefficient,
                                    float delayOffsetSamples,
                                    int targetWindowSamples) noexcept;

    static constexpr float minPitchRatio = 0.25f;
    static constexpr float maxPitchRatio = 8.0f;
    static constexpr float ratioSmoothingAlpha = 0.35f;
    static constexpr bool useInputSyncedPitchWindowByDefault = true;
    static constexpr float fixedPitchWindowSeconds = 0.018f;
    static constexpr float inputSyncedPitchWindowCycles = 6.0f;
    static constexpr int inputSyncedMinWindowSamples = 256;
    static constexpr int inputSyncedMaxWindowSamples = 4096;
    static constexpr float windowPitchSmoothingSeconds = 0.15f;
    static constexpr float maxWindowChangeRatioPerGrain = 1.25f;
    static constexpr int maxWindowChangeSamplesPerGrain = 512;

    std::array<VoicePitchState, MidiVoiceState::maxVoices> voiceStates {};
    SimplePitchDetector pitchDetector;
    juce::AudioBuffer<float> delayBuffer;
    float lastDetectedInputFrequencyHz = 0.0f;
    float windowPitchHz = 0.0f;
    PitchState pitchState;

    int delayBufferSize = 0;
    int writeIndex = 0;
    int pitchWindowSamples = 1024;
    int minimumDelaySamples = 128;
    double currentSampleRate = 44100.0;
};

} // namespace voxchord
