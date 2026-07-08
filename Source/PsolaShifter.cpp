#include "PsolaShifter.h"

#include <algorithm>
#include <cmath>

namespace voxchord
{
namespace
{
    constexpr double kPi = 3.14159265358979323846;

    inline std::int64_t absDistance (std::int64_t a, std::int64_t b) noexcept
    {
        return a > b ? a - b : b - a;
    }
} // namespace

void PsolaShifter::prepare (double sampleRate,
                            float minInputF0Hz,
                            float minPitchRatio,
                            float grainCapInputPeriods,
                            float rightHalfWidthFactor)
{
    sampleRateHz = std::max (1.0, sampleRate);

    const auto safeMinF0 = std::max (20.0f, minInputF0Hz);
    const auto safeMinRatio = std::clamp (minPitchRatio, absoluteMinRatio, absoluteMaxRatio);
    grainCapPeriods = std::clamp (grainCapInputPeriods, 1.0f, 16.0f);
    rightFactor = std::clamp (rightHalfWidthFactor, 0.25f, 1.0f);

    maxPeriodSamples = sampleRateHz / static_cast<double> (safeMinF0);
    minPeriodSamples = std::max (8.0, sampleRateHz / 1000.0);

    const auto widestFromRatio = maxPeriodSamples / static_cast<double> (safeMinRatio);
    const auto widestFromCap = maxPeriodSamples * static_cast<double> (grainCapPeriods);
    auto widest = std::max (maxPeriodSamples, std::min (widestFromRatio, widestFromCap));

    // The ring must hold the constant latency plus one full grain behind the
    // emit point; clamp the grain width if an extreme configuration would
    // exceed it (keeps indexing valid at the cost of gaps in that case).
    const auto ringBudget = static_cast<double> (ringSize) * 0.45;
    widest = std::min (widest, ringBudget / 4.0);

    maxGrainHalfWidth = static_cast<int> (std::ceil (widest));
    searchHalfMax = static_cast<int> (std::ceil (maxPeriodSamples * 0.25));
    latencySamples = maxGrainHalfWidth
                   + static_cast<int> (std::ceil (rightFactor * static_cast<float> (maxGrainHalfWidth)))
                   + static_cast<int> (std::ceil (maxPeriodSamples * 0.5)) + 64;

    inputRing.assign (ringSize, 0.0f);
    guideRing.assign (ringSize, 0.0f);
    olaRing.assign (ringSize, 0.0f);
    windowSumRing.assign (ringSize, 0.0f);

    // Peak search runs on a ~700 Hz one-pole lowpass so formant ripple on real
    // voices cannot pull marks off the glottal cycle (directions/0708_5.md).
    guideCoefficient = 1.0f - std::exp (static_cast<float> (-2.0 * kPi * 700.0 / sampleRateHz));

    reset();
}

void PsolaShifter::reset() noexcept
{
    std::fill (inputRing.begin(), inputRing.end(), 0.0f);
    std::fill (guideRing.begin(), guideRing.end(), 0.0f);
    std::fill (olaRing.begin(), olaRing.end(), 0.0f);
    std::fill (windowSumRing.begin(), windowSumRing.end(), 0.0f);

    guideState = 0.0f;
    storedMarkCount = 0;
    inputPeriodSamples = 0.0;
    writePos = 0;
    nextMarkGuess = -1.0;
    nextSynthMark = -1.0;
}

void PsolaShifter::setTargetPitchRatio (float ratio) noexcept
{
    currentRatio = std::clamp (ratio, absoluteMinRatio, absoluteMaxRatio);
}

void PsolaShifter::setInputPitchHz (float f0Hz) noexcept
{
    if (f0Hz <= 0.0f)
        return; // unvoiced / not yet detected: hold the previous period

    auto period = std::clamp (sampleRateHz / static_cast<double> (f0Hz),
                              minPeriodSamples,
                              maxPeriodSamples);

    // Slew-limit the period (+/-8% per block) so momentary detector jumps
    // cannot modulate the mark spacing into low-frequency artifacts
    // (directions/0708_5.md item 2).
    if (inputPeriodSamples > 0.0)
        period = std::clamp (period, inputPeriodSamples * 0.92, inputPeriodSamples * 1.08);

    inputPeriodSamples = period;
}

int PsolaShifter::currentGrainHalfWidth() const noexcept
{
    const auto periodOut = inputPeriodSamples / static_cast<double> (currentRatio);
    const auto capped = std::min (periodOut, inputPeriodSamples * static_cast<double> (grainCapPeriods));
    const auto halfWidth = std::max (inputPeriodSamples, capped);

    return std::clamp (static_cast<int> (std::lround (halfWidth)), 8, maxGrainHalfWidth);
}

int PsolaShifter::rightHalfWidthFor (int leftHalfWidth) const noexcept
{
    return std::max (8, static_cast<int> (std::lround (rightFactor * static_cast<float> (leftHalfWidth))));
}

void PsolaShifter::finalizeAnalysisMarksUpTo() noexcept
{
    const auto searchHalf = std::clamp (static_cast<int> (inputPeriodSamples * 0.25), 2, searchHalfMax);

    // A finalized mark only guarantees the peak-search context; whether the
    // grain around it is fully written is checked at placement time instead
    // (directions/0708_1.md item 1: folds the search lookahead into the
    // grain's own extraction wait).
    while (writePos > static_cast<std::int64_t> (std::llround (nextMarkGuess)) + searchHalf + 1)
    {
        const auto guess = static_cast<std::int64_t> (std::llround (nextMarkGuess));
        auto best = guess;
        auto bestValue = -1.0e30f;

        // Refine to the strongest positive peak of the lowpassed guide signal
        // so successive grains are extracted phase-coherently; the guide keeps
        // formant ripple from splitting the per-period peak into competitors.
        for (auto p = guess - searchHalf; p <= guess + searchHalf; ++p)
        {
            if (p < 0)
                continue;

            const auto value = guideRing[static_cast<size_t> (p & ringMask)];

            if (value > bestValue)
            {
                bestValue = value;
                best = p;
            }
        }

        if (storedMarkCount < maxStoredMarks)
        {
            storedMarks[storedMarkCount++] = best;
        }
        else
        {
            for (auto i = 1; i < maxStoredMarks; ++i)
                storedMarks[i - 1] = storedMarks[i];

            storedMarks[maxStoredMarks - 1] = best;
        }

        nextMarkGuess = static_cast<double> (best) + inputPeriodSamples;
    }
}

// Loudness compensation: downshift gaps (duty < 1) and upshift comb thinning
// both lower the wet RMS relative to unison; compensate per grain from the
// duty cycle / ratio (empirically calibrated, directions/0708_5.md item 3).
float PsolaShifter::grainGainFor (int leftHalfWidth, int rightHalfWidth, double periodOut) const noexcept
{
    if (periodOut <= 1.0)
        return 1.0f;

    const auto duty = static_cast<float> ((leftHalfWidth + rightHalfWidth) / periodOut);
    auto gain = 1.0f;

    if (duty < 1.0f)
        gain = std::pow (std::max (duty, 0.05f), -0.6f);
    else if (currentRatio > 1.0f)
        gain = std::pow (currentRatio, 0.35f);

    return std::min (gain, 2.2f); // cap ~ +6.8 dB
}

void PsolaShifter::placeGrain (std::int64_t analysisMark,
                               double synthesisMark,
                               int leftHalfWidth,
                               int rightHalfWidth,
                               float gain) noexcept
{
    // Two-piece Hann: continuous at k = 0 (peak 1); the shorter right half
    // trades spectral sharpness for lookahead (directions/0708_3.md item 1).
    //
    // The grain is deposited at the *fractional* synthesis position: rounding
    // the synthesis grid to integers modulates the grain spacing with a
    // sawtooth at the grid's beat rate against the sample clock, which shows
    // up as low-frequency sidebands at incommensurate ratios
    // (directions/0708_5.md item 2). Window and content are both evaluated at
    // the fractional offset (linear interpolation on the content).
    const auto invLeft = 1.0f / static_cast<float> (leftHalfWidth);
    const auto invRight = 1.0f / static_cast<float> (rightHalfWidth);
    const auto baseIndex = static_cast<std::int64_t> (std::floor (synthesisMark));
    const auto fraction = static_cast<float> (synthesisMark - static_cast<double> (baseIndex));

    for (auto j = -leftHalfWidth; j <= rightHalfWidth + 1; ++j)
    {
        const auto k = static_cast<float> (j) - fraction; // offset from the fractional mark

        if (k < static_cast<float> (-leftHalfWidth) || k > static_cast<float> (rightHalfWidth))
            continue;

        const auto destinationIndex = baseIndex + j;
        const auto sourceFloor = analysisMark + j - 1; // analysisMark + k lies in [sourceFloor, sourceFloor + 1]

        if (destinationIndex < 0 || sourceFloor < 0)
            continue;

        const auto invHalfWidth = k <= 0.0f ? invLeft : invRight;
        const auto window = 0.5f + 0.5f * std::cos (static_cast<float> (kPi) * k * invHalfWidth);

        const auto sourceFraction = 1.0f - fraction; // (analysisMark + k) - sourceFloor
        const auto s0 = inputRing[static_cast<size_t> (sourceFloor & ringMask)];
        const auto s1 = inputRing[static_cast<size_t> ((sourceFloor + 1) & ringMask)];
        const auto sample = s0 + (s1 - s0) * sourceFraction;
        const auto cell = static_cast<size_t> (destinationIndex & ringMask);

        olaRing[cell] += window * sample * gain;
        windowSumRing[cell] += window;
    }
}

void PsolaShifter::placeReadyGrains() noexcept
{
    if (storedMarkCount == 0)
        return;

    const auto leftHalfWidth = currentGrainHalfWidth();
    const auto rightHalfWidth = rightHalfWidthFor (leftHalfWidth);

    // A mark is placeable once its whole grain is inside the written input;
    // only the right (future) half needs waiting for (mark + Hr < writePos).
    // Synthesis marks do not wait for the nearest mark to become placeable;
    // they use the nearest *placeable* one, and the P/2 loop bound below
    // guarantees that mark is within half a period of the synthesis position
    // (directions/0708_1.md item 2, 0708_2.md item 1, 0708_3.md item 1).
    auto placeableCount = storedMarkCount;

    while (placeableCount > 0 && storedMarks[placeableCount - 1] + rightHalfWidth >= writePos)
        --placeableCount;

    if (placeableCount == 0)
        return;

    const auto newestPlaceable = storedMarks[placeableCount - 1];

    if (nextSynthMark < 0.0)
        nextSynthMark = static_cast<double> (newestPlaceable);

    const auto periodOut = std::max (2.0, inputPeriodSamples / static_cast<double> (currentRatio));

    // Bound P/2 (not P): guarantees a placeable mark within half a period of
    // every synthesis position, so grain content age averages zero and unison
    // reduces to exact reconstruction (directions/0708_2.md item 1).
    while (nextSynthMark <= static_cast<double> (newestPlaceable) + inputPeriodSamples * 0.5)
    {
        const auto synthesisMark = static_cast<std::int64_t> (std::llround (nextSynthMark));

        auto nearest = storedMarks[0];
        auto nearestDistance = absDistance (nearest, synthesisMark);

        for (auto i = 1; i < placeableCount; ++i)
        {
            const auto distance = absDistance (storedMarks[i], synthesisMark);

            if (distance < nearestDistance)
            {
                nearestDistance = distance;
                nearest = storedMarks[i];
            }
        }

        const auto gain = grainGainFor (leftHalfWidth, rightHalfWidth, periodOut);
        placeGrain (nearest, nextSynthMark, leftHalfWidth, rightHalfWidth, gain);
        nextSynthMark += periodOut;
    }
}

void PsolaShifter::onSampleWritten() noexcept
{
    if (inputPeriodSamples <= 0.0)
        return;

    if (nextMarkGuess < 0.0)
        nextMarkGuess = static_cast<double> (maxGrainHalfWidth + searchHalfMax + 2);

    finalizeAnalysisMarksUpTo();
    placeReadyGrains();
}

void PsolaShifter::processBlock (const float* input, float* output, int numSamples) noexcept
{
    for (auto i = 0; i < numSamples; ++i)
    {
        inputRing[static_cast<size_t> (writePos & ringMask)] = input[i];
        guideState += guideCoefficient * (input[i] - guideState);
        guideRing[static_cast<size_t> (writePos & ringMask)] = guideState;
        ++writePos;

        onSampleWritten();

        const auto emitPosition = writePos - static_cast<std::int64_t> (latencySamples);

        if (emitPosition < 0)
        {
            output[i] = 0.0f;
            continue;
        }

        const auto cell = static_cast<size_t> (emitPosition & ringMask);
        const auto windowSum = windowSumRing[cell];

        output[i] = olaRing[cell] / std::max (windowSum, 0.5f);

        olaRing[cell] = 0.0f;
        windowSumRing[cell] = 0.0f;
    }
}

} // namespace voxchord
