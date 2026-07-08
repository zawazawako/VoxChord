#pragma once

#include <cstdint>
#include <vector>

// Experimental TD-PSOLA pitch shifter (branch exp/d3-psola, phase D3 trial).
//
// Streaming, single-voice, mono. All memory is allocated in prepare(); the
// process path is allocation-free, lock-free and exception-free so the class
// could later be hosted on the audio thread (realtime-rules compliant).
//
// Algorithm outline (classic pitch-scale-only TD-PSOLA, time scale = 1):
//  - Analysis pitch marks are laid down one input period apart, refined to the
//    strongest positive waveform peak in a +/- period/4 search window.
//  - Synthesis marks are laid down one *output* period (inputPeriod / ratio)
//    apart on the same timeline.
//  - For every synthesis mark the nearest finalized analysis mark is windowed
//    (Hann, half-width H) from the input ring and overlap-added into an output
//    accumulation ring together with the window itself; emitted samples are
//    normalized by the accumulated window sum.
//  - Grain half-width H = max(inputPeriod, min(outputPeriod, cap * inputPeriod)).
//    cap = 1 gives the low-latency mode (gaps appear below ratio 0.5);
//    cap >= 1/minRatio gives the quality mode (no gaps, more latency + smear).
//
// Latency is constant per prepare() configuration:
//    latency = Hlmax + Hrmax + Pmax/2 + margin (margin = 64 samples)
// where Hr = rightHalfWidthFactor * Hl (two-piece Hann window; factor 1.0 is
// the symmetric case). Worst-case values must be chosen up front
// (minInputF0Hz, minPitchRatio). The peak search lookahead is folded into the
// grain's extraction wait, and the Pmax/2 term pays for mark-phase
// quantization so every synthesis mark can use a mark within half a period of
// its position (content age averages zero; unison is exact reconstruction).
// Shrinking the right (future) half trades spectral sharpness for latency.
// See directions/0708_1.md, 0708_2.md and 0708_3.md.

namespace voxchord
{
class PsolaShifter final
{
public:
    void prepare (double sampleRate,
                  float minInputF0Hz,
                  float minPitchRatio,
                  float grainCapInputPeriods,
                  float rightHalfWidthFactor = 1.0f);
    void reset() noexcept;

    // ratio > 1 shifts up. Clamped to [1/16, 8]. Takes effect per grain.
    void setTargetPitchRatio (float ratio) noexcept;

    // Detected input fundamental in Hz; <= 0 holds the previous value.
    void setInputPitchHz (float f0Hz) noexcept;

    void processBlock (const float* input, float* output, int numSamples) noexcept;

    int getLatencySamples() const noexcept { return latencySamples; }

private:
    void onSampleWritten() noexcept;
    void finalizeAnalysisMarksUpTo() noexcept;
    void placeReadyGrains() noexcept;
    void placeGrain (std::int64_t analysisMark, std::int64_t synthesisMark,
                     int leftHalfWidth, int rightHalfWidth) noexcept;
    int currentGrainHalfWidth() const noexcept;
    int rightHalfWidthFor (int leftHalfWidth) const noexcept;

    static constexpr int ringSizePow2 = 17; // 131072 samples (~3 s @ 44.1 kHz)
    static constexpr int ringSize = 1 << ringSizePow2;
    static constexpr int ringMask = ringSize - 1;
    static constexpr int maxStoredMarks = 16;
    static constexpr float absoluteMinRatio = 1.0f / 16.0f;
    static constexpr float absoluteMaxRatio = 8.0f;

    std::vector<float> inputRing;
    std::vector<float> olaRing;
    std::vector<float> windowSumRing;
    std::int64_t storedMarks[maxStoredMarks] = {};
    int storedMarkCount = 0;

    double sampleRateHz = 44100.0;
    double inputPeriodSamples = 0.0;   // 0 until the first voiced pitch arrives
    double minPeriodSamples = 32.0;
    double maxPeriodSamples = 512.0;
    float currentRatio = 1.0f;
    float grainCapPeriods = 8.0f;
    float rightFactor = 1.0f;
    int maxGrainHalfWidth = 0;
    int searchHalfMax = 0;
    int latencySamples = 0;

    std::int64_t writePos = 0;         // absolute count of samples written
    double nextMarkGuess = -1.0;       // absolute position of next analysis mark
    double nextSynthMark = -1.0;       // absolute position of next synthesis mark
};

} // namespace voxchord
