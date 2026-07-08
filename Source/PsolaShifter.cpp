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
                            float grainCapInputPeriods)
{
    sampleRateHz = std::max (1.0, sampleRate);

    const auto safeMinF0 = std::max (20.0f, minInputF0Hz);
    const auto safeMinRatio = std::clamp (minPitchRatio, absoluteMinRatio, absoluteMaxRatio);
    grainCapPeriods = std::clamp (grainCapInputPeriods, 1.0f, 16.0f);

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
    latencySamples = 2 * maxGrainHalfWidth
                   + static_cast<int> (std::ceil (maxPeriodSamples * 0.5)) + 64;

    inputRing.assign (ringSize, 0.0f);
    olaRing.assign (ringSize, 0.0f);
    windowSumRing.assign (ringSize, 0.0f);

    reset();
}

void PsolaShifter::reset() noexcept
{
    std::fill (inputRing.begin(), inputRing.end(), 0.0f);
    std::fill (olaRing.begin(), olaRing.end(), 0.0f);
    std::fill (windowSumRing.begin(), windowSumRing.end(), 0.0f);

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

    inputPeriodSamples = std::clamp (sampleRateHz / static_cast<double> (f0Hz),
                                     minPeriodSamples,
                                     maxPeriodSamples);
}

int PsolaShifter::currentGrainHalfWidth() const noexcept
{
    const auto periodOut = inputPeriodSamples / static_cast<double> (currentRatio);
    const auto capped = std::min (periodOut, inputPeriodSamples * static_cast<double> (grainCapPeriods));
    const auto halfWidth = std::max (inputPeriodSamples, capped);

    return std::clamp (static_cast<int> (std::lround (halfWidth)), 8, maxGrainHalfWidth);
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

        // Refine to the strongest positive waveform peak so successive grains
        // are extracted phase-coherently (one unambiguous peak per period).
        for (auto p = guess - searchHalf; p <= guess + searchHalf; ++p)
        {
            if (p < 0)
                continue;

            const auto value = inputRing[static_cast<size_t> (p & ringMask)];

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

void PsolaShifter::placeGrain (std::int64_t analysisMark,
                               std::int64_t synthesisMark,
                               int halfWidth) noexcept
{
    const auto invHalfWidth = 1.0f / static_cast<float> (halfWidth);

    for (auto k = -halfWidth; k <= halfWidth; ++k)
    {
        const auto sourceIndex = analysisMark + k;
        const auto destinationIndex = synthesisMark + k;

        if (sourceIndex < 0 || destinationIndex < 0)
            continue;

        const auto window = 0.5f + 0.5f * std::cos (static_cast<float> (kPi)
                                                    * static_cast<float> (k) * invHalfWidth);
        const auto sample = inputRing[static_cast<size_t> (sourceIndex & ringMask)];
        const auto cell = static_cast<size_t> (destinationIndex & ringMask);

        olaRing[cell] += window * sample;
        windowSumRing[cell] += window;
    }
}

void PsolaShifter::placeReadyGrains() noexcept
{
    if (storedMarkCount == 0)
        return;

    const auto halfWidth = currentGrainHalfWidth();

    // A mark is placeable once its whole grain is inside the written input
    // (mark + halfWidth < writePos). Synthesis marks no longer wait for the
    // nearest mark to become placeable; they use the nearest *placeable* one,
    // which may be up to ~one period older than the synthesis position
    // (directions/0708_1.md item 2: removes the finalization-granularity wait).
    auto placeableCount = storedMarkCount;

    while (placeableCount > 0 && storedMarks[placeableCount - 1] + halfWidth >= writePos)
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

        placeGrain (nearest, synthesisMark, halfWidth);
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
