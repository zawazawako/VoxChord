#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include <juce_audio_basics/juce_audio_basics.h>

#include "MidiVoiceState.h"
#include "PsolaShifter.h"

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
    // Lead Tune (Retune) path: harmonic-corrected pitch with no median/smoothing,
    // so the tuner snaps directly (0 when unvoiced). See directions/0708_9.md.
    float tunePitchHz = 0.0f;
    float ratioSmoothingCoefficient = 0.0f;
    float characterAmountRaw = 0.0f;
    float characterAmountSmoothed = 0.0f;
    float characterDeltaRms = 0.0f;
    float characterDeltaPeak = 0.0f;
    float characterDeltaRatioDb = -100.0f;
    float confidence = 0.0f;
    bool voiced = false;
    int harmonicCorrectionMode = 0;
    int characterModeRaw = 0;
    int characterModeSanitized = 0;

    // D1 low-pitch diagnostics (observation only, see directions/0703_1.md)
    float windowPitchHz = 0.0f;
    int representativeVoiceMidiNote = -1;
    int representativeGrainWindowSamples = 0;
    float representativePitchRatioRaw = 0.0f;
    float representativePitchRatioClamped = 0.0f;
    float outputPeriodToWindowRatio = 0.0f;
    uint32_t ratioClampHitCount = 0;
    float wetZeroCrossingHz = 0.0f;
    float wetZeroCrossingCentsDeviation = 0.0f;
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
                 juce::AudioBuffer<float>& tunedLeadOutput,
                 const MidiVoiceState::NoteSnapshot& activeNotes,
                 int voiceLimit,
                 float spread,
                 float tune,
                 float glide,
                 int characterMode,
                 float characterAmountRaw,
                 float characterAmountSmoothed,
                 bool leadTuneEnabled,
                 bool psolaEnabled = false) noexcept;
    float getLastDetectedInputFrequencyHz() const noexcept { return lastDetectedInputFrequencyHz; }
    PitchState getPitchState() const noexcept { return pitchState; }
    void resetRatioClampHitCount() noexcept { ratioClampHitCounter.store (0, std::memory_order_relaxed); }

private:
    struct VoicePitchState
    {
        struct CharacterBiquad
        {
            void reset() noexcept
            {
                z1 = 0.0f;
                z2 = 0.0f;
            }

            void setIdentity() noexcept
            {
                b0 = 1.0f;
                b1 = 0.0f;
                b2 = 0.0f;
                a1 = 0.0f;
                a2 = 0.0f;
            }

            float process (float input) noexcept
            {
                const auto output = b0 * input + z1;
                z1 = b1 * input - a1 * output + z2;
                z2 = b2 * input - a2 * output;
                return output;
            }

            float b0 = 1.0f;
            float b1 = 0.0f;
            float b2 = 0.0f;
            float a1 = 0.0f;
            float a2 = 0.0f;
            float z1 = 0.0f;
            float z2 = 0.0f;
        };

        int lastMidiNote = -1;
        bool wasActive = false;
        int lastCharacterMode = -1;
        float decimatorHoldValue = 0.0f;
        int decimatorHoldCounter = 0;
        float phaseA = 0.0f;
        float phaseB = 0.5f;
        float currentPitchRatio = 1.0f;
        float targetPitchRatio = 1.0f;
        float envelopeGain = 0.0f;
        float targetEnvelopeGain = 0.0f;
        float leftGain = 0.0f;
        float rightGain = 0.0f;
        float monoGain = 0.0f;
        float delayOffsetSamples = 0.0f;
        CharacterBiquad characterFilter1;
        CharacterBiquad characterFilter2;
        CharacterBiquad characterFilter3;
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
    static float getLeadRetuneCoefficient (float tune, double sampleRate) noexcept;
    static float getNoteTransitionRatioCoefficient (double sampleRate) noexcept;
    static float getEnvelopeCoefficient (float timeSeconds, double sampleRate) noexcept;
    static int sanitizeCharacterMode (int characterMode) noexcept;
    static float getChromaticLeadPitchRatio (float inputFrequencyHz) noexcept;
    static float getCharacterPitchRatio (int slot, int characterMode, float characterAmount) noexcept;
    static float getCharacterGain (int slot, int characterMode, float characterAmount) noexcept;
    static float getCharacterDelayOffsetSamples (int slot, int characterMode, float characterAmount, double sampleRate) noexcept;
    static void configureCharacterTone (VoicePitchState& voice, int slot, int characterMode, float characterAmount, double sampleRate, float lfoPhase) noexcept;
    static float applyCharacterTone (VoicePitchState& voice, float sample, int slot, int characterMode, float characterAmount) noexcept;
    static void setPeakingFilter (VoicePitchState::CharacterBiquad& filter, float frequencyHz, float gainDb, float q, double sampleRate) noexcept;
    static void setHighShelfFilter (VoicePitchState::CharacterBiquad& filter, float frequencyHz, float gainDb, float q, double sampleRate) noexcept;
    static void setHighPassFilter (VoicePitchState::CharacterBiquad& filter, float frequencyHz, float q, double sampleRate) noexcept;
    static float applySoftSaturation (float sample, float drive, float amount) noexcept;
    static int getFixedPitchWindowSamples (double sampleRate) noexcept;
    static int getInputSyncedPitchWindowSamples (float inputFrequencyHz, double sampleRate) noexcept;
    static int getLimitedPitchWindowSamples (int currentWindowSamples, int targetWindowSamples) noexcept;
    static float getWindowPitchSmoothingCoefficient (int samples, double sampleRate) noexcept;
    static float smoothFrequencyLog (float previous, float target, float alpha) noexcept;
    static float readMonoInput (const juce::AudioBuffer<float>& input, int sample) noexcept;
    static float wrapPhase (float phase) noexcept;
    static float windowGain (float phase) noexcept;

    float updateWindowPitchHz (float correctionInputPitchHz, int samples) noexcept;
    void updateWetZeroCrossing (float sampleValue) noexcept;
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
    static constexpr float noteTransitionRatioSmoothingSeconds = 0.008f;
    static constexpr float voiceAttackSeconds = 0.008f;
    static constexpr float voiceReleaseSeconds = 0.012f;
    static constexpr float leadAttackSeconds = 0.006f;
    static constexpr float leadReleaseSeconds = 0.010f;
    static constexpr float voiceEnvelopeSilenceThreshold = 0.0001f;
    static constexpr float baseVoiceGain = 0.45f;

    // D3 A/B experiment (branch exp/d3-psola, directions/0708_4.md):
    // per-voice TD-PSOLA bank, run every block so switching is instant.
    // Config = plan B50 at a 90 Hz provisioning floor (wet delay ~23.7 ms
    // @44.1 kHz; floor lowered from 110 Hz per user request, 0708_5). If
    // breathy/clean voices sound rough in the listening test, fall back to
    // plan B75 by setting psolaRightHalfFactor to 0.75f.
    static constexpr float psolaMinF0Hz = 90.0f;
    static constexpr float psolaRightHalfFactor = 0.5f;
    static constexpr float psolaGrainCapPeriods = 1.0f;
    static constexpr float psolaMinPitchRatio = 1.0f / 16.0f;
    static constexpr float psolaMaxPitchRatio = 8.0f;

    std::array<VoicePitchState, MidiVoiceState::maxVoices> voiceStates {};
    VoicePitchState leadVoiceState;
    // Character "Vowel" formant sweep LFOs, one slow free-running phase per
    // slot (block-rate; directions/0708_6.md).
    std::array<float, MidiVoiceState::maxVoices> characterLfoPhases {};
    SimplePitchDetector pitchDetector;
    juce::AudioBuffer<float> delayBuffer;
    // PSOLA bank covers the harmony voices only. The tuned lead always uses the
    // window shifter regardless of engine mode (directions/0708_9.md item 1):
    // it replaces the dry monitor signal, where the window shifter's lower,
    // pitch-synced latency and its snappier retune behaviour feel more natural.
    std::array<PsolaShifter, MidiVoiceState::maxVoices> psolaVoiceShifters;
    std::array<float, MidiVoiceState::maxVoices> psolaTargetRatios {};
    std::array<float, MidiVoiceState::maxVoices> psolaCurrentRatios {};
    // Tracking highpass at 0.6 * target f0 per PSOLA voice: everything below
    // the output comb's fundamental is artifact energy (mark-reuse sidebands
    // at incommensurate ratios), so it can be removed without touching the
    // wanted harmonics (directions/0708_5.md item 2).
    std::array<VoicePitchState::CharacterBiquad, MidiVoiceState::maxVoices> psolaHighpassFilters {};
    // D4 voiced/unvoiced hysteresis feeding PsolaShifter::setVoicedAmount
    // (on: voiced && confidence > 0.75, off: !voiced || confidence < 0.55).
    bool psolaVoicedState = false;
    juce::AudioBuffer<float> psolaScratch; // ch0 = mono in, ch1..maxVoices = harmony voices
    float lastDetectedInputFrequencyHz = 0.0f;
    float windowPitchHz = 0.0f;
    PitchState pitchState;

    // D1 low-pitch diagnostics state (observation only, see directions/0703_1.md)
    std::atomic<uint32_t> ratioClampHitCounter { 0 };
    int wetZeroCrossingTrackedSlot = -1;
    double wetZeroCrossingLocalSampleIndex = 0.0;
    double wetZeroCrossingLastCrossingSampleIndex = -1.0;
    float wetZeroCrossingPreviousSample = 0.0f;
    float wetZeroCrossingEstimatedHz = 0.0f;

    int delayBufferSize = 0;
    int writeIndex = 0;
    int pitchWindowSamples = 1024;
    int minimumDelaySamples = 128;
    double currentSampleRate = 44100.0;
};

} // namespace voxchord
