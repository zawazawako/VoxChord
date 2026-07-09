#include "SimpleChoirEngine.h"

#include <cmath>
#include <limits>
#include <vector>

namespace voxchord
{
namespace
{
    juce::String formatSelfTestPitch (float pitchHz)
    {
        if (pitchHz <= 0.0f)
            return "--";

        return juce::String (pitchHz, 1) + " Hz";
    }

    PitchShifterSelfTestSummary& pitchShifterSelfTestSummary() noexcept
    {
        static PitchShifterSelfTestSummary summary;
        return summary;
    }

    juce::String describeHarmonicCorrectionMode (int mode)
    {
        switch (mode)
        {
            case 2:  return "raw/2";
            case 3:  return "raw/3";
            case -2: return "raw*2";
            case -3: return "raw*3";
            default: break;
        }

        return "none";
    }

    float centsError (float measuredHz, float expectedHz)
    {
        if (measuredHz <= 0.0f || expectedHz <= 0.0f)
            return 0.0f;

        return 1200.0f * std::log2 (measuredHz / expectedHz);
    }

    float measureFrequencyFromPositiveZeroCrossings (const std::vector<float>& samples, double sampleRate)
    {
        if (samples.size() < 2 || sampleRate <= 0.0)
            return 0.0f;

        std::vector<double> crossings;
        crossings.reserve (samples.size() / 64);

        for (size_t index = 1; index < samples.size(); ++index)
        {
            const auto previous = samples[index - 1];
            const auto current = samples[index];

            if (previous < 0.0f && current >= 0.0f)
            {
                const auto denominator = static_cast<double> (current - previous);
                const auto fraction = std::abs (denominator) > 0.0000001
                                          ? static_cast<double> (-previous) / denominator
                                          : 0.0;

                crossings.push_back (static_cast<double> (index - 1) + fraction);
            }
        }

        if (crossings.size() < 2)
            return 0.0f;

        auto periodSum = 0.0;

        for (size_t index = 1; index < crossings.size(); ++index)
            periodSum += crossings[index] - crossings[index - 1];

        const auto averagePeriodSamples = periodSum / static_cast<double> (crossings.size() - 1);

        if (averagePeriodSamples <= 0.0)
            return 0.0f;

        return static_cast<float> (sampleRate / averagePeriodSamples);
    }

    struct RunningStats
    {
        void add (float value) noexcept
        {
            if (count == 0)
            {
                min = value;
                max = value;
            }
            else
            {
                min = juce::jmin (min, value);
                max = juce::jmax (max, value);
            }

            sum += static_cast<double> (value);
            ++count;
        }

        float average() const noexcept
        {
            return count > 0 ? static_cast<float> (sum / static_cast<double> (count)) : 0.0f;
        }

        double sum = 0.0;
        float min = 0.0f;
        float max = 0.0f;
        int count = 0;
    };

    struct SpectralPeak
    {
        float frequencyHz = 0.0f;
        float magnitude = 0.0f;
    };

    struct SpectralDiagnostics
    {
        std::array<SpectralPeak, 5> topPeaks {};
        SpectralPeak expectedBin;
        SpectralPeak measuredBin;
        float binResolutionHz = 0.0f;
    };

    bool didPhaseWrap (float previous, float current, float phaseDelta) noexcept
    {
        if (phaseDelta > 0.0f)
            return current < previous;

        if (phaseDelta < 0.0f)
            return current > previous;

        return false;
    }

    float circularDelta (float current, float previous, int bufferSize) noexcept
    {
        auto delta = current - previous;
        const auto size = static_cast<float> (bufferSize);
        const auto halfSize = size * 0.5f;

        if (delta > halfSize)
            delta -= size;
        else if (delta < -halfSize)
            delta += size;

        return delta;
    }

    float goertzelMagnitudeForBin (const std::vector<float>& samples, int startSample, int sampleCount, int bin)
    {
        if (sampleCount <= 1 || bin <= 0)
            return 0.0f;

        const auto omega = juce::MathConstants<double>::twoPi * static_cast<double> (bin) / static_cast<double> (sampleCount);
        const auto coefficient = 2.0 * std::cos (omega);
        auto q0 = 0.0;
        auto q1 = 0.0;
        auto q2 = 0.0;
        auto windowSum = 0.0;

        for (auto index = 0; index < sampleCount; ++index)
        {
            const auto window = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi
                                                       * static_cast<double> (index)
                                                       / static_cast<double> (sampleCount - 1));
            const auto value = static_cast<double> (samples[static_cast<size_t> (startSample + index)]) * window;
            q0 = value + coefficient * q1 - q2;
            q2 = q1;
            q1 = q0;
            windowSum += window;
        }

        const auto magnitudeSquared = q1 * q1 + q2 * q2 - coefficient * q1 * q2;
        const auto magnitude = std::sqrt (juce::jmax (0.0, magnitudeSquared));
        return static_cast<float> (magnitude / juce::jmax (1.0, windowSum));
    }

    void insertSpectralPeak (std::array<SpectralPeak, 5>& peaks, SpectralPeak candidate) noexcept
    {
        for (auto index = 0; index < static_cast<int> (peaks.size()); ++index)
        {
            if (candidate.magnitude <= peaks[static_cast<size_t> (index)].magnitude)
                continue;

            for (auto move = static_cast<int> (peaks.size()) - 1; move > index; --move)
                peaks[static_cast<size_t> (move)] = peaks[static_cast<size_t> (move - 1)];

            peaks[static_cast<size_t> (index)] = candidate;
            break;
        }
    }

    SpectralDiagnostics analyseSpectrum (const std::vector<float>& samples,
                                         double sampleRate,
                                         float expectedHz,
                                         float measuredHz)
    {
        SpectralDiagnostics diagnostics;

        if (samples.size() < 4096 || sampleRate <= 0.0)
            return diagnostics;

        const auto analysisSamples = juce::jmin (32768, static_cast<int> (samples.size()));
        const auto startSample = static_cast<int> (samples.size()) - analysisSamples;
        const auto binHz = static_cast<float> (sampleRate / static_cast<double> (analysisSamples));
        const auto minBin = juce::jmax (1, static_cast<int> (std::floor (20.0f / binHz)));
        const auto maxBin = juce::jmin (analysisSamples / 2 - 1, static_cast<int> (std::ceil (4000.0f / binHz)));
        diagnostics.binResolutionHz = binHz;

        std::vector<float> magnitudes (static_cast<size_t> (maxBin + 1), 0.0f);

        for (auto bin = minBin; bin <= maxBin; ++bin)
            magnitudes[static_cast<size_t> (bin)] = goertzelMagnitudeForBin (samples, startSample, analysisSamples, bin);

        for (auto bin = minBin + 1; bin < maxBin; ++bin)
        {
            const auto previous = magnitudes[static_cast<size_t> (bin - 1)];
            const auto current = magnitudes[static_cast<size_t> (bin)];
            const auto next = magnitudes[static_cast<size_t> (bin + 1)];

            if (current >= previous && current >= next)
                insertSpectralPeak (diagnostics.topPeaks, { static_cast<float> (bin) * binHz, current });
        }

        const auto expectedBin = juce::jlimit (minBin, maxBin, juce::roundToInt (expectedHz / binHz));
        const auto measuredBin = juce::jlimit (minBin, maxBin, juce::roundToInt (measuredHz / binHz));
        diagnostics.expectedBin = { static_cast<float> (expectedBin) * binHz, magnitudes[static_cast<size_t> (expectedBin)] };
        diagnostics.measuredBin = { static_cast<float> (measuredBin) * binHz, magnitudes[static_cast<size_t> (measuredBin)] };
        return diagnostics;
    }

    bool shouldReportSpectrum (float inputHz, float ratio) noexcept
    {
        return (std::abs (inputHz - 440.0f) < 0.01f && std::abs (ratio - 0.5f) < 0.001f)
            || (std::abs (inputHz - 660.0f) < 0.01f && std::abs (ratio - 0.5f) < 0.001f)
            || (std::abs (inputHz - 880.0f) < 0.01f && std::abs (ratio - 0.5f) < 0.001f)
            || (std::abs (inputHz - 440.0f) < 0.01f && std::abs (ratio - 2.0f) < 0.001f);
    }
}

void SimpleChoirEngine::prepare (double sampleRate, int maxBlockSize)
{
    currentSampleRate = juce::jmax (1.0, sampleRate);
    pitchWindowSamples = getFixedPitchWindowSamples (currentSampleRate);
    minimumDelaySamples = juce::jlimit (32, 1024, juce::roundToInt (currentSampleRate * 0.004));
    delayBufferSize = juce::jmax (minimumDelaySamples + inputSyncedMaxWindowSamples * 2 + maxBlockSize + 8,
                                  juce::roundToInt (currentSampleRate * 0.25));

    delayBuffer.setSize (1, delayBufferSize, false, false, true);
    pitchDetector.prepare (currentSampleRate);

    // D3 A/B experiment: PSOLA bank for the harmony voices (see
    // directions/0708_4.md); the lead uses the window shifter (0708_9.md).
    psolaScratch.setSize (MidiVoiceState::maxVoices + 1, juce::jmax (1, maxBlockSize), false, false, true);

    for (auto& shifter : psolaVoiceShifters)
        shifter.prepare (currentSampleRate, psolaMinF0Hz, psolaMinPitchRatio,
                         psolaGrainCapPeriods, psolaRightHalfFactor);

    reset();
}

void SimpleChoirEngine::reset() noexcept
{
    delayBuffer.clear();
    writeIndex = 0;
    pitchDetector.reset();
    lastDetectedInputFrequencyHz = 0.0f;
    windowPitchHz = 0.0f;
    pitchState = {};

    ratioClampHitCounter.store (0, std::memory_order_relaxed);
    wetZeroCrossingTrackedSlot = -1;
    wetZeroCrossingLocalSampleIndex = 0.0;
    wetZeroCrossingLastCrossingSampleIndex = -1.0;
    wetZeroCrossingPreviousSample = 0.0f;
    wetZeroCrossingEstimatedHz = 0.0f;

    for (auto& voice : voiceStates)
        resetVoice (voice);

    resetVoice (leadVoiceState);
    characterLfoPhases.fill (0.0f);

    psolaScratch.clear();

    for (auto& shifter : psolaVoiceShifters)
        shifter.reset();

    psolaTargetRatios.fill (1.0f);
    psolaCurrentRatios.fill (1.0f);

    for (auto& filter : psolaHighpassFilters)
    {
        filter.setIdentity();
        filter.reset();
    }

    psolaVoicedState = false;
}

void SimpleChoirEngine::runPitchDetectorSelfTest()
{
    SimplePitchDetector detector;
    detector.prepare (48000.0);
    detector.setHarmonicCorrectionEnabled (false);

    constexpr double testSampleRate = 48000.0;
    constexpr auto testSamples = 48000;
    constexpr auto blockSize = 512;
    constexpr std::array<float, 12> testFrequencies {{
        100.0f, 150.0f, 220.0f, 261.63f, 329.63f, 440.0f,
        523.25f, 600.0f, 659.25f, 700.0f, 800.0f, 880.0f
    }};

    juce::AudioBuffer<float> block (1, blockSize);

    DBG ("VoxChord PitchDetector SelfTest: range 80-900 Hz, harmonic correction OFF");

    for (const auto frequencyHz : testFrequencies)
    {
        detector.reset();
        detector.setHarmonicCorrectionEnabled (false);

        auto phase = 0.0;
        PitchState result;

        for (auto processed = 0; processed < testSamples; processed += blockSize)
        {
            const auto samplesThisBlock = juce::jmin (blockSize, testSamples - processed);
            block.clear();

            for (auto sample = 0; sample < samplesThisBlock; ++sample)
            {
                block.setSample (0,
                                 sample,
                                 0.35f * std::sin (static_cast<float> (phase)));

                phase += juce::MathConstants<double>::twoPi * static_cast<double> (frequencyHz) / testSampleRate;

                if (phase >= juce::MathConstants<double>::twoPi)
                    phase -= juce::MathConstants<double>::twoPi;
            }

            result = detector.processBlock (block);
        }

        DBG (juce::String ("SelfTest ") + juce::String (frequencyHz, 2) + " Hz -> Raw: "
             + formatSelfTestPitch (result.rawPitchHz)
             + ", Corrected: " + formatSelfTestPitch (result.correctedPitchHz)
             + ", Stable: " + formatSelfTestPitch (result.displayStablePitchHz)
             + ", Confidence: " + juce::String (result.confidence, 2));
    }

    // D1 low-pitch diagnostics: extend coverage below the nominal 70 Hz detection
    // floor and run with harmonic correction ON so its activation is observable
    // (see directions/0703_1.md, item 1).
    constexpr std::array<float, 9> lowFrequencyTestFrequencies {{
        50.0f, 60.0f, 65.0f, 70.0f, 80.0f, 90.0f, 100.0f, 110.0f, 130.0f
    }};

    DBG ("VoxChord PitchDetector SelfTest (low-freq extension): range 50-130 Hz, harmonic correction ON");

    for (const auto frequencyHz : lowFrequencyTestFrequencies)
    {
        detector.reset();
        detector.setHarmonicCorrectionEnabled (true);

        auto phase = 0.0;
        PitchState result;

        for (auto processed = 0; processed < testSamples; processed += blockSize)
        {
            const auto samplesThisBlock = juce::jmin (blockSize, testSamples - processed);
            block.clear();

            for (auto sample = 0; sample < samplesThisBlock; ++sample)
            {
                block.setSample (0,
                                 sample,
                                 0.35f * std::sin (static_cast<float> (phase)));

                phase += juce::MathConstants<double>::twoPi * static_cast<double> (frequencyHz) / testSampleRate;

                if (phase >= juce::MathConstants<double>::twoPi)
                    phase -= juce::MathConstants<double>::twoPi;
            }

            result = detector.processBlock (block);
        }

        DBG (juce::String ("SelfTest(low) ") + juce::String (frequencyHz, 2) + " Hz -> Raw: "
             + formatSelfTestPitch (result.rawPitchHz)
             + ", Corrected: " + formatSelfTestPitch (result.correctedPitchHz)
             + ", Stable: " + formatSelfTestPitch (result.displayStablePitchHz)
             + ", Confidence: " + juce::String (result.confidence, 2)
             + ", HarmonicCorrection: " + describeHarmonicCorrectionMode (result.harmonicCorrectionMode));
    }
}

void SimpleChoirEngine::runPitchShifterSelfTest()
{
    struct TestCase
    {
        float inputFrequencyHz = 0.0f;
        float ratio = 1.0f;
        bool useInputSyncedWindow = false;
    };

    constexpr double testSampleRate = 48000.0;
    constexpr auto maxBlockSize = 512;
    constexpr auto totalSamples = 57600;
    constexpr auto skipSamples = 16800;
    constexpr std::array<TestCase, 44> testCases {{
        { 440.0f, 1.0f },
        { 660.0f, 1.0f },
        { 880.0f, 1.0f },
        { 220.0f, 2.0f },
        { 440.0f, 2.0f },
        { 440.0f, 1.5f },
        { 440.0f, 0.5f },
        { 660.0f, 0.5f },
        { 880.0f, 0.5f },
        { 440.0f, 0.5f, true },
        { 660.0f, 0.5f, true },
        { 880.0f, 0.5f, true },
        { 440.0f, 2.0f, true },
        { 100.0f, 0.5f, true },
        { 100.0f, 0.75f, true },
        { 100.0f, 1.0f, true },
        { 100.0f, 1.5f, true },
        { 100.0f, 2.0f, true },
        { 150.0f, 0.5f, true },
        { 150.0f, 0.75f, true },
        { 150.0f, 1.0f, true },
        { 150.0f, 1.5f, true },
        { 150.0f, 2.0f, true },
        { 220.0f, 0.5f, true },
        { 220.0f, 0.75f, true },
        { 220.0f, 1.0f, true },
        { 220.0f, 1.5f, true },
        { 220.0f, 2.0f, true },
        { 330.0f, 0.5f, true },
        { 330.0f, 0.75f, true },
        { 330.0f, 1.0f, true },
        { 330.0f, 1.5f, true },
        { 330.0f, 2.0f, true },
        { 440.0f, 0.75f, true },
        { 440.0f, 1.0f, true },
        { 440.0f, 1.5f, true },
        { 660.0f, 0.75f, true },
        { 660.0f, 1.0f, true },
        { 660.0f, 1.5f, true },
        { 660.0f, 2.0f, true },
        { 880.0f, 0.75f, true },
        { 880.0f, 1.0f, true },
        { 880.0f, 1.5f, true },
        { 880.0f, 2.0f, true }
    }};

    DBG ("VoxChord PitchShifter SelfTest: fixed ratio, Character=0, Glide disabled, VoiceCount=1 equivalent");
    DBG (juce::String ("VoxChord PitchShifter SelfTest: input-synced window prototype cycles ")
         + juce::String (inputSyncedPitchWindowCycles, 1)
         + ", clamp " + juce::String (inputSyncedMinWindowSamples)
         + "-" + juce::String (inputSyncedMaxWindowSamples)
         + " samples, no empirical ratio correction");

    auto summary = PitchShifterSelfTestSummary {};
    summary.fixedWindow.hasRun = true;
    summary.fixedWindow.passed = true;
    summary.inputSyncedWindow.hasRun = true;
    summary.inputSyncedWindow.passed = true;

    for (const auto testCase : testCases)
    {
        SimpleChoirEngine engine;
        engine.prepare (testSampleRate, maxBlockSize);

        auto& voice = engine.voiceStates[0];
        voice.wasActive = true;
        voice.lastMidiNote = 0;
        voice.phaseA = 0.0f;
        voice.phaseB = 0.5f;
        voice.currentPitchRatio = testCase.ratio;
        voice.targetPitchRatio = testCase.ratio;

        std::vector<float> output;
        output.reserve (totalSamples - skipSamples);

        auto phase = 0.0;
        auto* delay = engine.delayBuffer.getWritePointer (0);
        const auto windowSamples = testCase.useInputSyncedWindow
                                       ? getInputSyncedPitchWindowSamples (testCase.inputFrequencyHz, testSampleRate)
                                       : engine.pitchWindowSamples;
        const auto inputPeriodSamples = static_cast<float> (testSampleRate / static_cast<double> (testCase.inputFrequencyHz));
        voice.windowSamplesA = windowSamples;
        voice.windowSamplesB = windowSamples;

        const auto phaseDelta = (1.0f - testCase.ratio) / static_cast<float> (windowSamples);
        const auto baseDelay = static_cast<float> (engine.minimumDelaySamples);
        auto previousPhaseA = voice.phaseA;
        auto previousPhaseB = voice.phaseB;
        auto previousDelayA = baseDelay + previousPhaseA * static_cast<float> (voice.windowSamplesA);
        auto previousDelayB = baseDelay + previousPhaseB * static_cast<float> (voice.windowSamplesB);
        auto previousReadA = static_cast<float> (engine.writeIndex) - previousDelayA;
        auto previousReadB = static_cast<float> (engine.writeIndex) - previousDelayB;
        auto lastWrapA = -1;
        auto lastWrapB = -1;
        RunningStats delayStepAStats;
        RunningStats delayStepBStats;
        RunningStats readStepAStats;
        RunningStats readStepBStats;
        RunningStats wrapIntervalAStats;
        RunningStats wrapIntervalBStats;

        for (auto sample = 0; sample < totalSamples; ++sample)
        {
            const auto input = 0.35f * std::sin (static_cast<float> (phase));
            delay[engine.writeIndex] = input;

            const auto shifted = engine.renderPitchShiftedSample (voice, 1.0f, 0.0f, windowSamples);

            const auto currentDelayA = baseDelay + voice.phaseA * static_cast<float> (voice.windowSamplesA);
            const auto currentDelayB = baseDelay + voice.phaseB * static_cast<float> (voice.windowSamplesB);
            const auto currentReadA = static_cast<float> ((engine.writeIndex + 1) % engine.delayBufferSize) - currentDelayA;
            const auto currentReadB = static_cast<float> ((engine.writeIndex + 1) % engine.delayBufferSize) - currentDelayB;
            const auto wrappedA = didPhaseWrap (previousPhaseA, voice.phaseA, phaseDelta);
            const auto wrappedB = didPhaseWrap (previousPhaseB, voice.phaseB, phaseDelta);

            if (sample > 0 && ! wrappedA)
            {
                delayStepAStats.add (currentDelayA - previousDelayA);
                readStepAStats.add (circularDelta (currentReadA, previousReadA, engine.delayBufferSize));
            }

            if (sample > 0 && ! wrappedB)
            {
                delayStepBStats.add (currentDelayB - previousDelayB);
                readStepBStats.add (circularDelta (currentReadB, previousReadB, engine.delayBufferSize));
            }

            if (wrappedA)
            {
                if (lastWrapA >= 0)
                    wrapIntervalAStats.add (static_cast<float> (sample - lastWrapA));

                lastWrapA = sample;
            }

            if (wrappedB)
            {
                if (lastWrapB >= 0)
                    wrapIntervalBStats.add (static_cast<float> (sample - lastWrapB));

                lastWrapB = sample;
            }

            if (sample >= skipSamples)
                output.push_back (shifted);

            engine.writeIndex = (engine.writeIndex + 1) % engine.delayBufferSize;

            phase += juce::MathConstants<double>::twoPi
                   * static_cast<double> (testCase.inputFrequencyHz)
                   / testSampleRate;

            if (phase >= juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;

            previousPhaseA = voice.phaseA;
            previousPhaseB = voice.phaseB;
            previousDelayA = currentDelayA;
            previousDelayB = currentDelayB;
            previousReadA = currentReadA;
            previousReadB = currentReadB;
        }

        const auto expectedHz = testCase.inputFrequencyHz * testCase.ratio;
        const auto measuredHz = measureFrequencyFromPositiveZeroCrossings (output, testSampleRate);
        const auto errorCents = centsError (measuredHz, expectedHz);
        const auto absoluteErrorCents = std::abs (errorCents);
        const auto actualRatio = testCase.inputFrequencyHz > 0.0f ? measuredHz / testCase.inputFrequencyHz : 0.0f;
        const auto actualRatioOverTarget = testCase.ratio > 0.0f ? actualRatio / testCase.ratio : 0.0f;
        const auto delayStepPerSample = phaseDelta * static_cast<float> (windowSamples);
        const auto theoreticalReadSpeed = 1.0f - delayStepPerSample;
        const auto withinTenCents = std::abs (errorCents) <= 10.0f;
        const auto windowMode = testCase.useInputSyncedWindow ? "input-synced" : "fixed";

        auto& modeSummary = testCase.useInputSyncedWindow ? summary.inputSyncedWindow : summary.fixedWindow;

        if (absoluteErrorCents > modeSummary.maxErrorCents)
        {
            modeSummary.maxErrorCents = absoluteErrorCents;
            modeSummary.worstInputHz = testCase.inputFrequencyHz;
            modeSummary.worstRatio = testCase.ratio;
            modeSummary.worstActualRatio = actualRatio;
            modeSummary.worstMeasuredHz = measuredHz;
        }

        if (! withinTenCents)
            modeSummary.passed = false;

        DBG (juce::String ("PitchShifterSelfTest input ")
             + juce::String (testCase.inputFrequencyHz, 1) + " Hz"
             + ", ratio " + juce::String (testCase.ratio, 3)
             + ", windowMode " + windowMode
             + " -> expected " + juce::String (expectedHz, 1) + " Hz"
             + ", measured " + juce::String (measuredHz, 2) + " Hz"
             + ", error " + juce::String (errorCents, 2) + " cents"
             + ", actual ratio " + juce::String (actualRatio, 6)
             + ", actual/target " + juce::String (actualRatioOverTarget, 6)
             + ", phaseDelta " + juce::String (phaseDelta, 9)
             + ", delayStep " + juce::String (delayStepPerSample, 6)
             + ", theoreticalReadSpeed " + juce::String (theoreticalReadSpeed, 6)
             + ", measuredDelayStepA avg/min/max/count "
             + juce::String (delayStepAStats.average(), 6) + "/"
             + juce::String (delayStepAStats.min, 6) + "/"
             + juce::String (delayStepAStats.max, 6) + "/"
             + juce::String (delayStepAStats.count)
             + ", measuredDelayStepB avg/min/max/count "
             + juce::String (delayStepBStats.average(), 6) + "/"
             + juce::String (delayStepBStats.min, 6) + "/"
             + juce::String (delayStepBStats.max, 6) + "/"
             + juce::String (delayStepBStats.count)
             + ", measuredReadStepA avg/min/max/count "
             + juce::String (readStepAStats.average(), 6) + "/"
             + juce::String (readStepAStats.min, 6) + "/"
             + juce::String (readStepAStats.max, 6) + "/"
             + juce::String (readStepAStats.count)
             + ", measuredReadStepB avg/min/max/count "
             + juce::String (readStepBStats.average(), 6) + "/"
             + juce::String (readStepBStats.min, 6) + "/"
             + juce::String (readStepBStats.max, 6) + "/"
             + juce::String (readStepBStats.count)
             + ", phaseWrapA count/avg/min/max "
             + juce::String (wrapIntervalAStats.count) + "/"
             + juce::String (wrapIntervalAStats.average(), 3) + "/"
             + juce::String (wrapIntervalAStats.min, 3) + "/"
             + juce::String (wrapIntervalAStats.max, 3)
             + ", phaseWrapB count/avg/min/max "
             + juce::String (wrapIntervalBStats.count) + "/"
             + juce::String (wrapIntervalBStats.average(), 3) + "/"
             + juce::String (wrapIntervalBStats.min, 3) + "/"
             + juce::String (wrapIntervalBStats.max, 3)
             + ", within +/-10 cents: " + juce::String (withinTenCents ? "yes" : "no")
             + ", pitchWindowSamples: " + juce::String (windowSamples)
             + ", fixedPitchWindowSamples: " + juce::String (engine.pitchWindowSamples)
             + ", inputPeriodSamples: " + juce::String (inputPeriodSamples, 3)
             + ", inputSyncedWindowCycles: " + juce::String (inputSyncedPitchWindowCycles, 1)
             + ", minimumDelaySamples: " + juce::String (engine.minimumDelaySamples)
             + ", ratio smoothing/glide disabled: yes");

        if (shouldReportSpectrum (testCase.inputFrequencyHz, testCase.ratio))
        {
            const auto spectrum = analyseSpectrum (output, testSampleRate, expectedHz, measuredHz);
            auto spectrumText = juce::String ("PitchShifterSpectrum input ")
                              + juce::String (testCase.inputFrequencyHz, 1) + " Hz"
                              + ", ratio " + juce::String (testCase.ratio, 3)
                              + ", windowMode " + windowMode
                              + ", expected " + juce::String (expectedHz, 1) + " Hz"
                              + ", zeroCrossMeasured " + juce::String (measuredHz, 2) + " Hz"
                              + ", peak frequency used for measured result: zeroCrossMeasured"
                              + ", binResolution " + juce::String (spectrum.binResolutionHz, 3) + " Hz"
                              + ", expectedBin " + juce::String (spectrum.expectedBin.frequencyHz, 2) + " Hz/"
                              + juce::String (spectrum.expectedBin.magnitude, 6)
                              + ", measuredBin " + juce::String (spectrum.measuredBin.frequencyHz, 2) + " Hz/"
                              + juce::String (spectrum.measuredBin.magnitude, 6)
                              + ", top5";

            for (const auto peak : spectrum.topPeaks)
            {
                spectrumText += " | " + juce::String (peak.frequencyHz, 2) + " Hz/"
                              + juce::String (peak.magnitude, 6);
            }

            DBG (spectrumText);
        }
    }

    pitchShifterSelfTestSummary() = summary;

    const auto logSummary = [] (const char* label, const PitchShifterSelfTestModeSummary& modeSummary)
    {
        DBG (juce::String ("PitchShifterSelfTest summary ")
             + label + ": "
             + (modeSummary.passed ? "PASS" : "FAIL")
             + ", max error " + juce::String (modeSummary.maxErrorCents, 2) + " cents"
             + ", worst input " + juce::String (modeSummary.worstInputHz, 1) + " Hz"
             + ", worst ratio " + juce::String (modeSummary.worstRatio, 3)
             + ", worst actual ratio " + juce::String (modeSummary.worstActualRatio, 6)
             + ", worst measured " + juce::String (modeSummary.worstMeasuredHz, 2) + " Hz");
    };

    logSummary ("fixed", summary.fixedWindow);
    logSummary ("input-synced", summary.inputSyncedWindow);
}

PitchShifterSelfTestSummary SimpleChoirEngine::getPitchShifterSelfTestSummary() noexcept
{
    return pitchShifterSelfTestSummary();
}

void SimpleChoirEngine::render (const juce::AudioBuffer<float>& dryInput,
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
                                bool psolaEnabled) noexcept
{
    wetOutput.clear();
    tunedLeadOutput.clear();

    const auto samples = juce::jmin (dryInput.getNumSamples(),
                                     juce::jmin (wetOutput.getNumSamples(), tunedLeadOutput.getNumSamples()));
    const auto outputChannels = wetOutput.getNumChannels();
    const auto leadOutputChannels = tunedLeadOutput.getNumChannels();

    if (samples <= 0 || outputChannels <= 0 || leadOutputChannels <= 0 || delayBufferSize <= 0)
        return;


    const auto safeVoiceLimit = juce::jlimit (1, MidiVoiceState::maxVoices, voiceLimit);
    const auto safeGlide = juce::jlimit (0.0f, 1.0f, glide);
    const auto safeCharacterMode = sanitizeCharacterMode (characterMode);
    const auto safeCharacterAmountRaw = juce::jlimit (0.0f, 1.0f, characterAmountRaw);
    const auto safeCharacterAmount = juce::jlimit (0.0f, 1.0f, characterAmountSmoothed);
    const auto activeCount = countActiveVoices (activeNotes, safeVoiceLimit);

    // D1 low-pitch diagnostics: track the lowest-target active voice (see directions/0703_1.md, item 2/3).
    auto representativeSlot = -1;
    auto representativeMidiNote = -1;

    for (auto slot = 0; slot < safeVoiceLimit; ++slot)
    {
        const auto note = activeNotes[static_cast<size_t> (slot)];

        if (note < 0)
            continue;

        if (representativeMidiNote < 0 || note < representativeMidiNote)
        {
            representativeMidiNote = note;
            representativeSlot = slot;
        }
    }

    if (representativeSlot != wetZeroCrossingTrackedSlot)
    {
        wetZeroCrossingTrackedSlot = representativeSlot;
        wetZeroCrossingLocalSampleIndex = 0.0;
        wetZeroCrossingLastCrossingSampleIndex = -1.0;
        wetZeroCrossingPreviousSample = 0.0f;
        wetZeroCrossingEstimatedHz = 0.0f;
    }

    const auto glideCoefficient = getGlideCoefficient (safeGlide, currentSampleRate);
    const auto transitionRatioCoefficient = getNoteTransitionRatioCoefficient (currentSampleRate);
    const auto ratioSmoothingCoefficient = safeGlide <= 0.001f
                                               ? transitionRatioCoefficient
                                               : juce::jmin (transitionRatioCoefficient, glideCoefficient);
    // Retune Speed: the tuned lead's ratio snap time is driven by `tune`
    // (independent of Glide), so hard-tune is snappy and low `tune` is gentle
    // (directions/0708_9.md item 3).
    const auto leadRetuneCoefficient = getLeadRetuneCoefficient (tune, currentSampleRate);
    pitchState = pitchDetector.processBlock (dryInput);
    pitchState.ratioSmoothingCoefficient = ratioSmoothingCoefficient;
    pitchState.characterModeRaw = characterMode;
    pitchState.characterModeSanitized = safeCharacterMode;
    pitchState.characterAmountRaw = safeCharacterAmountRaw;
    pitchState.characterAmountSmoothed = safeCharacterAmount;
    pitchState.characterDeltaRms = 0.0f;
    pitchState.characterDeltaPeak = 0.0f;
    pitchState.characterDeltaRatioDb = -100.0f;
    const auto inputFrequencyHz = pitchState.correctionInputPitchHz;
    lastDetectedInputFrequencyHz = inputFrequencyHz;
    const auto windowFrequencyHz = updateWindowPitchHz (inputFrequencyHz, samples);
    const auto activePitchWindowSamples = useInputSyncedPitchWindowByDefault
                                              ? getInputSyncedPitchWindowSamples (windowFrequencyHz, currentSampleRate)
                                              : pitchWindowSamples;
    // Lead Tune uses the un-smoothed tune pitch path (directions/0708_9.md).
    const auto leadCanTune = leadTuneEnabled && pitchState.voiced && pitchState.tunePitchHz > 0.0f;
    const auto leadTargetRatio = leadCanTune ? getChromaticLeadPitchRatio (pitchState.tunePitchHz) : 1.0f;

    if (leadTuneEnabled)
    {
        if (! leadVoiceState.wasActive)
        {
            leadVoiceState.phaseA = 0.0f;
            leadVoiceState.phaseB = 0.5f;
            leadVoiceState.windowSamplesA = activePitchWindowSamples;
            leadVoiceState.windowSamplesB = activePitchWindowSamples;
            leadVoiceState.currentPitchRatio = leadTargetRatio;
            leadVoiceState.envelopeGain = 0.0f;
        }

        leadVoiceState.wasActive = true;
        leadVoiceState.targetPitchRatio = leadTargetRatio;
        leadVoiceState.targetEnvelopeGain = leadCanTune ? 1.0f : 0.0f;
    }
    else
    {
        leadVoiceState.targetEnvelopeGain = 0.0f;

        if (! leadVoiceState.wasActive || leadVoiceState.envelopeGain <= voiceEnvelopeSilenceThreshold)
            resetVoice (leadVoiceState);
    }

    // Advance the per-slot Character "Vowel" formant LFOs once per block
    // (free-running so re-triggered notes keep drifting; directions/0708_6.md).
    {
        static constexpr std::array<float, MidiVoiceState::maxVoices> lfoRatesHz {{
            0.11f, 0.17f, 0.08f, 0.22f, 0.13f, 0.19f, 0.10f, 0.15f
        }};

        for (auto slot = 0; slot < MidiVoiceState::maxVoices; ++slot)
        {
            auto& phase = characterLfoPhases[static_cast<size_t> (slot)];
            phase += juce::MathConstants<float>::twoPi * lfoRatesHz[static_cast<size_t> (slot)]
                   * static_cast<float> (samples) / static_cast<float> (currentSampleRate);

            if (phase > juce::MathConstants<float>::twoPi)
                phase -= juce::MathConstants<float>::twoPi;
        }
    }

    std::array<int, MidiVoiceState::maxVoices> renderSlots {};
    auto activeIndex = 0;
    auto renderCount = 0;

    for (auto slot = 0; slot < MidiVoiceState::maxVoices; ++slot)
    {
        auto& voice = voiceStates[static_cast<size_t> (slot)];
        const auto active = slot < safeVoiceLimit && activeNotes[static_cast<size_t> (slot)] >= 0;

        if (! active)
        {
            voice.targetEnvelopeGain = 0.0f;

            if (voice.wasActive && voice.envelopeGain > voiceEnvelopeSilenceThreshold)
                renderSlots[static_cast<size_t> (renderCount++)] = slot;
            else
                resetVoice (voice);

            continue;
        }

        const auto midiNote = activeNotes[static_cast<size_t> (slot)];
        const auto rawTargetRatio = getPitchRatioForNote (midiNote, inputFrequencyHz)
                                        * getCharacterPitchRatio (slot, safeCharacterMode, safeCharacterAmount);
        const auto targetRatio = juce::jlimit (minPitchRatio, maxPitchRatio, rawTargetRatio);

        // D3 A/B: the PSOLA path gets the wider clamp so deep downshifts (the
        // D1 low-MIDI failure at ratio < 0.25) actually reach pitch
        // (directions/0708_4.md).
        psolaTargetRatios[static_cast<size_t> (slot)] = juce::jlimit (psolaMinPitchRatio,
                                                                      psolaMaxPitchRatio,
                                                                      rawTargetRatio);

        // D1 low-pitch diagnostics only: does not feed back into targetRatio above.
        const auto diagnosticTargetFrequency = static_cast<float> (juce::MidiMessage::getMidiNoteInHertz (midiNote));
        const auto diagnosticRawRatio = inputFrequencyHz > 0.0f ? diagnosticTargetFrequency / inputFrequencyHz : 1.0f;

        if (diagnosticRawRatio < minPitchRatio || diagnosticRawRatio > maxPitchRatio)
            ratioClampHitCounter.fetch_add (1, std::memory_order_relaxed);

        if (slot == representativeSlot)
        {
            pitchState.representativeVoiceMidiNote = midiNote;
            pitchState.representativePitchRatioRaw = diagnosticRawRatio;
            pitchState.representativePitchRatioClamped = targetRatio;
        }

        const auto wasAlreadyActive = voice.wasActive;

        if (! wasAlreadyActive || voice.lastMidiNote != midiNote)
        {
            if (! wasAlreadyActive)
            {
                voice.phaseA = 0.0f;
                voice.phaseB = 0.5f;
                voice.windowSamplesA = activePitchWindowSamples;
                voice.windowSamplesB = activePitchWindowSamples;
                voice.currentPitchRatio = targetRatio;
                voice.envelopeGain = 0.0f;
                psolaCurrentRatios[static_cast<size_t> (slot)] = psolaTargetRatios[static_cast<size_t> (slot)];
            }

            voice.targetPitchRatio = targetRatio;
            voice.wasActive = true;
            voice.lastMidiNote = midiNote;
        }
        else
        {
            voice.targetPitchRatio = targetRatio;
        }

        const auto pan = getPanForVoice (activeIndex, activeCount, spread);
        const auto voiceGain = baseVoiceGain * getCharacterGain (slot, safeCharacterMode, safeCharacterAmount);

        voice.targetEnvelopeGain = 1.0f;
        voice.leftGain = voiceGain * (pan <= 0.0f ? 1.0f : 1.0f - pan);
        voice.rightGain = voiceGain * (pan >= 0.0f ? 1.0f : 1.0f + pan);
        voice.monoGain = voiceGain;
        voice.delayOffsetSamples = getCharacterDelayOffsetSamples (slot,
                                                                   safeCharacterMode,
                                                                   safeCharacterAmount,
                                                                   currentSampleRate);
        configureCharacterTone (voice,
                                slot,
                                safeCharacterMode,
                                safeCharacterAmount,
                                currentSampleRate,
                                characterLfoPhases[static_cast<size_t> (slot)]);
        renderSlots[static_cast<size_t> (renderCount++)] = slot;
        ++activeIndex;
    }

    // D3 A/B experiment: run the PSOLA bank every block regardless of mode so
    // A/B switching is instant and warm (CPU cost ~0.7% total); only the
    // per-sample source selection below depends on psolaEnabled
    // (directions/0708_4.md). Note the Character per-slot delay offsets are
    // not applied on the PSOLA path (input-side delays would fight the pitch
    // marks); Character pitch offsets, gains and tone filters apply as usual.
    auto* psolaMono = psolaScratch.getWritePointer (0);

    for (auto sample = 0; sample < samples; ++sample)
        psolaMono[sample] = readMonoInput (dryInput, sample);

    const auto psolaBlockAlpha = ratioSmoothingCoefficient >= 1.0f
                                     ? 1.0f
                                     : 1.0f - std::pow (1.0f - ratioSmoothingCoefficient,
                                                        static_cast<float> (samples));

    // D4 voiced/unvoiced hysteresis (directions/0708_8.md): consonants and
    // breath pass through the shifters as latency-matched dry audio.
    if (! psolaVoicedState && pitchState.voiced && pitchState.confidence > 0.75f)
        psolaVoicedState = true;
    else if (psolaVoicedState && (! pitchState.voiced || pitchState.confidence < 0.55f))
        psolaVoicedState = false;

    const auto psolaVoicedAmount = psolaVoicedState ? 1.0f : 0.0f;

    for (auto slot = 0; slot < MidiVoiceState::maxVoices; ++slot)
    {
        const auto slotIndex = static_cast<size_t> (slot);
        psolaCurrentRatios[slotIndex] = psolaBlockAlpha >= 1.0f
                                            ? psolaTargetRatios[slotIndex]
                                            : smoothFrequencyLog (psolaCurrentRatios[slotIndex],
                                                                  psolaTargetRatios[slotIndex],
                                                                  psolaBlockAlpha);

        auto& shifter = psolaVoiceShifters[slotIndex];
        shifter.setInputPitchHz (inputFrequencyHz);
        shifter.setTargetPitchRatio (psolaCurrentRatios[slotIndex]);
        shifter.setVoicedAmount (psolaVoicedAmount);

        auto* voiceOut = psolaScratch.getWritePointer (slot + 1);
        shifter.processBlock (psolaMono, voiceOut, samples);

        if (inputFrequencyHz > 0.0f)
        {
            const auto targetHz = psolaCurrentRatios[slotIndex] * inputFrequencyHz;
            setHighPassFilter (psolaHighpassFilters[slotIndex],
                               juce::jlimit (30.0f, 1000.0f, 0.6f * targetHz),
                               0.707f,
                               currentSampleRate);
        }

        auto& highpass = psolaHighpassFilters[slotIndex];

        for (auto sample = 0; sample < samples; ++sample)
            voiceOut[sample] = highpass.process (voiceOut[sample]);
    }

    std::array<const float*, MidiVoiceState::maxVoices> psolaVoiceOut {};

    for (auto slot = 0; slot < MidiVoiceState::maxVoices; ++slot)
        psolaVoiceOut[static_cast<size_t> (slot)] = psolaScratch.getReadPointer (slot + 1);

    auto* delay = delayBuffer.getWritePointer (0);
    auto* left = wetOutput.getWritePointer (0);
    auto* right = outputChannels > 1 ? wetOutput.getWritePointer (1) : nullptr;
    auto* leadLeft = tunedLeadOutput.getWritePointer (0);
    auto* leadRight = leadOutputChannels > 1 ? tunedLeadOutput.getWritePointer (1) : nullptr;
    auto characterDeltaSumSquares = 0.0f;
    auto characterDeltaPeak = 0.0f;
    auto characterDeltaCount = 0;
    auto characterInputSumSquares = 0.0f;
    auto characterInputCount = 0;

    for (auto sample = 0; sample < samples; ++sample)
    {
        const auto drySample = readMonoInput (dryInput, sample);
        delay[writeIndex] = drySample;

        if (leadTuneEnabled || leadVoiceState.wasActive)
        {
            const auto leadEnvelopeCoefficient = leadVoiceState.targetEnvelopeGain > leadVoiceState.envelopeGain
                                                     ? getEnvelopeCoefficient (leadAttackSeconds, currentSampleRate)
                                                     : getEnvelopeCoefficient (leadReleaseSeconds, currentSampleRate);
            leadVoiceState.envelopeGain += (leadVoiceState.targetEnvelopeGain - leadVoiceState.envelopeGain)
                                         * leadEnvelopeCoefficient;

            // The tuned lead always uses the window shifter (both engine modes),
            // with the Retune-controlled ratio snap (directions/0708_9.md).
            const auto leadShifted = renderPitchShiftedSample (leadVoiceState,
                                                               leadRetuneCoefficient,
                                                               0.0f,
                                                               activePitchWindowSamples);
            const auto leadMix = juce::jlimit (0.0f, 1.0f, leadVoiceState.envelopeGain);
            const auto leadSample = drySample + (leadShifted - drySample) * leadMix;

            leadLeft[sample] = leadSample;

            if (leadRight != nullptr)
                leadRight[sample] = leadSample;

            if (! leadTuneEnabled && leadVoiceState.envelopeGain <= voiceEnvelopeSilenceThreshold)
                resetVoice (leadVoiceState);
        }
        else
        {
            leadLeft[sample] = dryInput.getSample (0, sample);

            if (leadRight != nullptr)
                leadRight[sample] = dryInput.getNumChannels() > 1 ? dryInput.getSample (1, sample) : leadLeft[sample];
        }

        for (auto index = 0; index < renderCount; ++index)
        {
            auto& voice = voiceStates[static_cast<size_t> (renderSlots[static_cast<size_t> (index)])];
            const auto envelopeCoefficient = voice.targetEnvelopeGain > voice.envelopeGain
                                                 ? getEnvelopeCoefficient (voiceAttackSeconds, currentSampleRate)
                                                 : getEnvelopeCoefficient (voiceReleaseSeconds, currentSampleRate);
            voice.envelopeGain += (voice.targetEnvelopeGain - voice.envelopeGain) * envelopeCoefficient;

            const auto shifted = psolaEnabled
                                     ? psolaVoiceOut[static_cast<size_t> (renderSlots[static_cast<size_t> (index)])][sample]
                                     : renderPitchShiftedSample (voice,
                                                                 ratioSmoothingCoefficient,
                                                                 voice.delayOffsetSamples,
                                                                 activePitchWindowSamples);
            characterInputSumSquares += shifted * shifted;
            ++characterInputCount;

            const auto charactered = applyCharacterTone (voice,
                                                         shifted,
                                                         renderSlots[static_cast<size_t> (index)],
                                                         safeCharacterMode,
                                                         safeCharacterAmount);
            const auto characterDelta = charactered - shifted;
            characterDeltaSumSquares += characterDelta * characterDelta;
            characterDeltaPeak = juce::jmax (characterDeltaPeak, std::abs (characterDelta));
            ++characterDeltaCount;

            if (renderSlots[static_cast<size_t> (index)] == representativeSlot)
                updateWetZeroCrossing (charactered);

            const auto enveloped = charactered * voice.envelopeGain;

            left[sample] += enveloped * (outputChannels > 1 ? voice.leftGain : voice.monoGain);

            if (right != nullptr)
                right[sample] += enveloped * voice.rightGain;
        }

        writeIndex = (writeIndex + 1) % delayBufferSize;
    }

    // D1 low-pitch diagnostics: publish shifter internal state and wet-output
    // frequency estimate for the representative (lowest-target) active voice
    // (see directions/0703_1.md, item 2/3).
    pitchState.windowPitchHz = windowPitchHz;
    pitchState.ratioClampHitCount = ratioClampHitCounter.load (std::memory_order_relaxed);

    if (representativeSlot >= 0)
    {
        const auto& representativeVoice = voiceStates[static_cast<size_t> (representativeSlot)];
        const auto representativeTargetHz = static_cast<float> (juce::MidiMessage::getMidiNoteInHertz (representativeMidiNote));

        pitchState.representativeGrainWindowSamples = representativeVoice.windowSamplesA;
        pitchState.outputPeriodToWindowRatio = (representativeTargetHz > 0.0f && representativeVoice.windowSamplesA > 0)
            ? (static_cast<float> (currentSampleRate) / representativeTargetHz) / static_cast<float> (representativeVoice.windowSamplesA)
            : 0.0f;
        pitchState.wetZeroCrossingHz = wetZeroCrossingEstimatedHz;
        pitchState.wetZeroCrossingCentsDeviation = (wetZeroCrossingEstimatedHz > 0.0f && representativeTargetHz > 0.0f)
            ? 1200.0f * std::log2 (wetZeroCrossingEstimatedHz / representativeTargetHz)
            : 0.0f;
    }
    else
    {
        pitchState.representativeVoiceMidiNote = -1;
        pitchState.representativeGrainWindowSamples = 0;
        pitchState.representativePitchRatioRaw = 0.0f;
        pitchState.representativePitchRatioClamped = 0.0f;
        pitchState.outputPeriodToWindowRatio = 0.0f;
        pitchState.wetZeroCrossingHz = 0.0f;
        pitchState.wetZeroCrossingCentsDeviation = 0.0f;
    }

    if (characterDeltaCount > 0)
    {
        pitchState.characterDeltaRms = std::sqrt (characterDeltaSumSquares / static_cast<float> (characterDeltaCount));
        pitchState.characterDeltaPeak = characterDeltaPeak;

        if (characterInputCount > 0)
        {
            const auto characterInputRms = std::sqrt (characterInputSumSquares / static_cast<float> (characterInputCount));
            const auto relativeDelta = pitchState.characterDeltaRms / juce::jmax (characterInputRms, 0.000001f);
            pitchState.characterDeltaRatioDb = juce::Decibels::gainToDecibels (relativeDelta, -100.0f);
        }
    }
}

int SimpleChoirEngine::countActiveVoices (const MidiVoiceState::NoteSnapshot& activeNotes, int voiceLimit) noexcept
{
    auto count = 0;

    for (auto slot = 0; slot < juce::jlimit (1, MidiVoiceState::maxVoices, voiceLimit); ++slot)
    {
        if (activeNotes[static_cast<size_t> (slot)] >= 0)
            ++count;
    }

    return count;
}

float SimpleChoirEngine::getPanForVoice (int activeIndex, int activeCount, float spread) noexcept
{
    const auto safeSpread = juce::jlimit (0.0f, 1.0f, spread);

    if (activeCount <= 1)
        return 0.0f;

    const auto normalized = static_cast<float> (activeIndex) / static_cast<float> (activeCount - 1);
    return (normalized * 2.0f - 1.0f) * safeSpread;
}

void SimpleChoirEngine::SimplePitchDetector::prepare (double sampleRate) noexcept
{
    sampleRateHz = juce::jmax (1.0, sampleRate);
    holdSamples = juce::jmax (1, juce::roundToInt (sampleRateHz * holdTimeMs * 0.001));
    reset();
}

void SimpleChoirEngine::SimplePitchDetector::reset() noexcept
{
    writeIndex = 0;
    samplesUntilAnalysis = hopSize;
    samplesSinceAccepted = holdSamples + 1;
    consecutiveJumpFrames = 0;
    correctionCandidateMode = 0;
    correctionCandidateFrames = 0;
    highConfidenceRawFrames = 0;
    previousRawPitchHz = 0.0f;
    state = {};
    ringBuffer.fill (0.0f);
    analysisFrame.fill (0.0f);
    difference.fill (0.0f);
    cmndf.fill (0.0f);
    medianLogBuffer.fill (0.0f);
    medianWriteIndex = 0;
    medianCount = 0;
}

void SimpleChoirEngine::SimplePitchDetector::setHarmonicCorrectionEnabled (bool shouldBeEnabled) noexcept
{
    harmonicCorrectionEnabled = shouldBeEnabled;
}

PitchState SimpleChoirEngine::SimplePitchDetector::processBlock (const juce::AudioBuffer<float>& input) noexcept
{
    const auto samples = input.getNumSamples();

    if (samples <= 0)
        return state;

    for (auto sample = 0; sample < samples; ++sample)
    {
        ringBuffer[static_cast<size_t> (writeIndex)] = SimpleChoirEngine::readMonoInput (input, sample);
        writeIndex = (writeIndex + 1) % frameLength;
        ++samplesSinceAccepted;

        if (--samplesUntilAnalysis <= 0)
        {
            analyseFrame();
            samplesUntilAnalysis += hopSize;
        }
    }

    if (samplesSinceAccepted > holdSamples)
    {
        state.displayStablePitchHz = 0.0f;
        state.correctionInputPitchHz = 0.0f;
        state.stablePitchHz = 0.0f;
        state.harmonyPitchHz = state.correctionInputPitchHz;
    }

    if (state.displayStablePitchHz <= 0.0f)
        state.voiced = false;

    return state;
}

void SimpleChoirEngine::SimplePitchDetector::analyseFrame() noexcept
{
    for (auto index = 0; index < frameLength; ++index)
        analysisFrame[static_cast<size_t> (index)] = ringBuffer[static_cast<size_t> ((writeIndex + index) % frameLength)];

    state.inputRmsDb = computeRmsDb (analysisFrame);

    if (state.inputRmsDb < rmsGateDb)
    {
        state.rawPitchHz = 0.0f;
        state.correctedPitchHz = 0.0f;
        state.tunePitchHz = 0.0f;
        state.confidence = 0.0f;
        state.voiced = false;
        state.harmonicCorrectionMode = 0;
        return;
    }

    const auto rawPitch = detectPitchYin();
    state.rawPitchHz = rawPitch;

    if (rawPitch > 0.0f
        && previousRawPitchHz > 0.0f
        && centsBetween (rawPitch, previousRawPitchHz) <= repeatedRawCents
        && state.confidence >= veryHighConfidenceThreshold)
    {
        ++highConfidenceRawFrames;
    }
    else
    {
        highConfidenceRawFrames = state.confidence >= veryHighConfidenceThreshold ? 1 : 0;
    }

    previousRawPitchHz = rawPitch;

    if (rawPitch <= 0.0f || state.confidence < confidenceThreshold)
    {
        state.tunePitchHz = 0.0f;
        state.voiced = false;
        return;
    }

    const auto correctedPitch = correctHarmonicPitch (rawPitch);
    state.correctedPitchHz = correctedPitch;

    if (correctedPitch <= 0.0f)
    {
        state.tunePitchHz = 0.0f;
        state.voiced = false;
        return;
    }

    // Lead Tune (Retune) path: harmonic-corrected pitch, no median/smoothing,
    // so the tuner tracks the input directly (directions/0708_9.md item 2).
    // The display/harmony path below (correctionInputPitchHz) is unchanged.
    state.tunePitchHz = correctedPitch;

    updateCorrectionInputPitch (correctedPitch);

    const auto medianPitch = applyMedianFilter (correctedPitch);

    if (! shouldAcceptCandidate (medianPitch))
        return;

    updateStablePitch (medianPitch);
    state.voiced = true;
    samplesSinceAccepted = 0;
}

float SimpleChoirEngine::SimplePitchDetector::detectPitchYin() noexcept
{
    const auto minLag = juce::jlimit (2,
                                      frameLength - 2,
                                      static_cast<int> (std::floor (sampleRateHz / maxFrequencyHz)));
    const auto maxLag = juce::jlimit (minLag + 1,
                                      frameLength - 2,
                                      static_cast<int> (std::ceil (sampleRateHz / minFrequencyHz)));

    difference[0] = 0.0f;
    cmndf[0] = 1.0f;

    for (auto tau = 1; tau <= maxLag + 1; ++tau)
    {
        auto sum = 0.0f;
        const auto compareSamples = frameLength - tau;

        for (auto index = 0; index < compareSamples; ++index)
        {
            const auto delta = analysisFrame[static_cast<size_t> (index)]
                             - analysisFrame[static_cast<size_t> (index + tau)];
            sum += delta * delta;
        }

        difference[static_cast<size_t> (tau)] = sum;
    }

    auto runningSum = 0.0f;

    for (auto tau = 1; tau <= maxLag + 1; ++tau)
    {
        runningSum += difference[static_cast<size_t> (tau)];
        cmndf[static_cast<size_t> (tau)] = runningSum > 0.0f
                                               ? difference[static_cast<size_t> (tau)] * static_cast<float> (tau) / runningSum
                                               : 1.0f;
    }

    auto thresholdTau = -1;

    for (auto tau = minLag; tau <= maxLag; ++tau)
    {
        const auto value = cmndf[static_cast<size_t> (tau)];

        if (value < yinThreshold)
        {
            thresholdTau = tau;

            while (thresholdTau + 1 <= maxLag
                   && cmndf[static_cast<size_t> (thresholdTau + 1)] < cmndf[static_cast<size_t> (thresholdTau)])
            {
                ++thresholdTau;
            }

            break;
        }
    }

    auto bestTau = thresholdTau;
    auto bestValue = bestTau >= minLag ? cmndf[static_cast<size_t> (bestTau)] : 1.0f;

    if (bestTau < minLag)
    {
        for (auto tau = minLag; tau <= maxLag; ++tau)
        {
            const auto value = cmndf[static_cast<size_t> (tau)];

            if (value + fallbackMinimumTolerance < bestValue)
            {
                bestValue = value;
                bestTau = tau;
            }
        }
    }

    if (bestTau < minLag)
    {
        state.confidence = 0.0f;
        return 0.0f;
    }

    const auto previous = cmndf[static_cast<size_t> (juce::jmax (minLag, bestTau - 1))];
    const auto current = cmndf[static_cast<size_t> (bestTau)];
    const auto next = cmndf[static_cast<size_t> (juce::jmin (maxLag + 1, bestTau + 1))];
    const auto interpolatedTau = static_cast<float> (bestTau) + getParabolicOffset (previous, current, next);

    state.confidence = juce::jlimit (0.0f, 1.0f, 1.0f - bestValue);

    if (interpolatedTau <= 0.0f)
        return 0.0f;

    return static_cast<float> (sampleRateHz) / interpolatedTau;
}

float SimpleChoirEngine::SimplePitchDetector::correctHarmonicPitch (float rawPitchHz) noexcept
{
    if (rawPitchHz < minFrequencyHz || rawPitchHz > maxFrequencyHz)
    {
        state.harmonicCorrectionMode = 0;
        return 0.0f;
    }

    if (state.displayStablePitchHz <= 0.0f)
    {
        state.harmonicCorrectionMode = 0;
        correctionCandidateMode = 0;
        correctionCandidateFrames = 0;
        return rawPitchHz;
    }

    if (! harmonicCorrectionEnabled || highConfidenceRawFrames >= highConfidenceRawFramesForUnlock)
    {
        state.harmonicCorrectionMode = 0;
        correctionCandidateMode = 0;
        correctionCandidateFrames = 0;
        return rawPitchHz;
    }

    struct Candidate
    {
        float pitchHz = 0.0f;
        int mode = 0;
    };

    std::array<Candidate, 5> candidates {{
        { rawPitchHz, 0 },
        { rawPitchHz * 0.5f, 2 },
        { rawPitchHz / 3.0f, 3 },
        { rawPitchHz * 2.0f, -2 },
        { rawPitchHz * 3.0f, -3 }
    }};

    auto bestCandidate = rawPitchHz;
    auto bestMode = 0;
    auto bestDistance = std::numeric_limits<float>::max();

    for (const auto candidate : candidates)
    {
        if (candidate.pitchHz < minFrequencyHz || candidate.pitchHz > maxFrequencyHz)
            continue;

        const auto distance = centsBetween (candidate.pitchHz, state.displayStablePitchHz);

        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestCandidate = candidate.pitchHz;
            bestMode = candidate.mode;
        }
    }

    if (bestMode != 0)
    {
        const auto rawDistance = centsBetween (rawPitchHz, state.displayStablePitchHz);
        const auto rawIsLikelyNewPitch = rawDistance > maxJumpCents
                                      && state.confidence >= veryHighConfidenceThreshold;

        if (bestDistance > correctionNearCents || rawIsLikelyNewPitch)
            bestMode = 0;
    }

    if (bestMode != 0)
    {
        if (bestMode == correctionCandidateMode)
            ++correctionCandidateFrames;
        else
        {
            correctionCandidateMode = bestMode;
            correctionCandidateFrames = 1;
        }

        if (correctionCandidateFrames < correctionConfirmationFrames)
            bestMode = 0;
    }
    else
    {
        correctionCandidateMode = 0;
        correctionCandidateFrames = 0;
    }

    if (bestMode == 0)
        bestCandidate = rawPitchHz;

    state.harmonicCorrectionMode = bestMode;
    return bestCandidate;
}

float SimpleChoirEngine::SimplePitchDetector::applyMedianFilter (float correctedPitchHz) noexcept
{
    if (correctedPitchHz < minFrequencyHz || correctedPitchHz > maxFrequencyHz)
        return 0.0f;

    medianLogBuffer[static_cast<size_t> (medianWriteIndex)] = std::log2 (correctedPitchHz);
    medianWriteIndex = (medianWriteIndex + 1) % medianWindowSize;
    medianCount = juce::jmin (medianCount + 1, medianWindowSize);

    std::array<float, medianWindowSize> values {};

    for (auto index = 0; index < medianCount; ++index)
        values[static_cast<size_t> (index)] = medianLogBuffer[static_cast<size_t> (index)];

    for (auto outer = 1; outer < medianCount; ++outer)
    {
        auto value = values[static_cast<size_t> (outer)];
        auto inner = outer;

        while (inner > 0 && values[static_cast<size_t> (inner - 1)] > value)
        {
            values[static_cast<size_t> (inner)] = values[static_cast<size_t> (inner - 1)];
            --inner;
        }

        values[static_cast<size_t> (inner)] = value;
    }

    return std::exp2 (values[static_cast<size_t> (medianCount / 2)]);
}

bool SimpleChoirEngine::SimplePitchDetector::shouldAcceptCandidate (float candidatePitchHz) noexcept
{
    if (candidatePitchHz < minFrequencyHz || candidatePitchHz > maxFrequencyHz)
        return false;

    if (state.displayStablePitchHz <= 0.0f)
    {
        consecutiveJumpFrames = 0;
        return true;
    }

    const auto jumpCents = centsBetween (candidatePitchHz, state.displayStablePitchHz);

    if (jumpCents <= maxJumpCents)
    {
        consecutiveJumpFrames = 0;
        return true;
    }

    ++consecutiveJumpFrames;

    if (state.confidence >= veryHighConfidenceThreshold && consecutiveJumpFrames >= 2)
    {
        consecutiveJumpFrames = 0;
        return true;
    }

    state.voiced = false;
    return false;
}

void SimpleChoirEngine::SimplePitchDetector::updateStablePitch (float candidatePitchHz) noexcept
{
    if (state.displayStablePitchHz <= 0.0f)
    {
        state.displayStablePitchHz = candidatePitchHz;
        state.stablePitchHz = state.displayStablePitchHz;
        return;
    }

    state.displayStablePitchHz = smoothFrequencyLog (state.displayStablePitchHz, candidatePitchHz, smoothingAlpha);
    state.stablePitchHz = state.displayStablePitchHz;
}

void SimpleChoirEngine::SimplePitchDetector::updateCorrectionInputPitch (float targetPitchHz) noexcept
{
    if (targetPitchHz <= 0.0f)
        return;

    if (state.correctionInputPitchHz <= 0.0f)
    {
        state.correctionInputPitchHz = targetPitchHz;
        state.harmonyPitchHz = state.correctionInputPitchHz;
        return;
    }

    state.correctionInputPitchHz = smoothFrequencyLog (state.correctionInputPitchHz,
                                                       targetPitchHz,
                                                       correctionFastAttackAlpha);
    state.harmonyPitchHz = state.correctionInputPitchHz;
}

float SimpleChoirEngine::SimplePitchDetector::computeRmsDb (const std::array<float, frameLength>& frame) noexcept
{
    auto sum = 0.0f;

    for (const auto sample : frame)
        sum += sample * sample;

    const auto rms = std::sqrt (sum / static_cast<float> (frameLength));
    return juce::Decibels::gainToDecibels (rms, -100.0f);
}

float SimpleChoirEngine::SimplePitchDetector::getParabolicOffset (float previous, float current, float next) noexcept
{
    const auto denominator = previous - 2.0f * current + next;

    if (std::abs (denominator) < 0.000001f)
        return 0.0f;

    return juce::jlimit (-0.5f, 0.5f, 0.5f * (previous - next) / denominator);
}

float SimpleChoirEngine::SimplePitchDetector::centsBetween (float a, float b) noexcept
{
    if (a <= 0.0f || b <= 0.0f)
        return std::numeric_limits<float>::infinity();

    return 1200.0f * std::abs (std::log2 (a / b));
}

float SimpleChoirEngine::SimplePitchDetector::smoothFrequencyLog (float previous, float target, float alpha) noexcept
{
    if (previous <= 0.0f)
        return target;

    if (target <= 0.0f)
        return previous;

    const auto previousLog = std::log2 (previous);
    const auto targetLog = std::log2 (target);
    return std::exp2 (previousLog + juce::jlimit (0.0f, 1.0f, alpha) * (targetLog - previousLog));
}

float SimpleChoirEngine::getPitchRatioForNote (int midiNote, float inputFrequencyHz) noexcept
{
    if (inputFrequencyHz <= 0.0f)
        return 1.0f;

    const auto targetFrequency = static_cast<float> (juce::MidiMessage::getMidiNoteInHertz (midiNote));
    return juce::jlimit (minPitchRatio, maxPitchRatio, targetFrequency / inputFrequencyHz);
}

float SimpleChoirEngine::applyTuneToRatio (float pitchRatio, float tune) noexcept
{
    const auto safeTune = juce::jlimit (0.0f, 1.0f, tune);
    return juce::jlimit (minPitchRatio, maxPitchRatio, 1.0f + (pitchRatio - 1.0f) * safeTune);
}

float SimpleChoirEngine::getGlideCoefficient (float glide, double sampleRate) noexcept
{
    const auto safeGlide = juce::jlimit (0.0f, 1.0f, glide);

    if (safeGlide <= 0.001f || sampleRate <= 1.0)
        return 1.0f;

    const auto glideSeconds = 0.005f + safeGlide * safeGlide * 0.495f;
    return 1.0f - std::exp (-1.0f / (glideSeconds * static_cast<float> (sampleRate)));
}

float SimpleChoirEngine::getLeadRetuneCoefficient (float tune, double sampleRate) noexcept
{
    // `tune` (Retune Speed) sets the lead's 90%-settling time from ~200 ms at 0
    // to ~1 ms at 1.0: settleMs = 1 + 199*(1 - tune)^2 (directions/0708_9.md).
    // Convert to a per-sample first-order log-domain coefficient via
    // tau = settleMs / ln(10), alpha = 1 - exp(-1 / tauSamples).
    if (sampleRate <= 1.0)
        return 1.0f;

    const auto safeTune = juce::jlimit (0.0f, 1.0f, tune);
    const auto settleMs = 1.0f + 199.0f * (1.0f - safeTune) * (1.0f - safeTune);
    const auto tauSamples = juce::jmax (1.0f,
                                        settleMs * 0.001f * static_cast<float> (sampleRate) / 2.302585f);
    return juce::jlimit (0.0f, 1.0f, 1.0f - std::exp (-1.0f / tauSamples));
}

float SimpleChoirEngine::getNoteTransitionRatioCoefficient (double sampleRate) noexcept
{
    return getEnvelopeCoefficient (noteTransitionRatioSmoothingSeconds, sampleRate);
}

float SimpleChoirEngine::getEnvelopeCoefficient (float timeSeconds, double sampleRate) noexcept
{
    if (timeSeconds <= 0.0f || sampleRate <= 1.0)
        return 1.0f;

    return 1.0f - std::exp (-1.0f / (timeSeconds * static_cast<float> (sampleRate)));
}

int SimpleChoirEngine::sanitizeCharacterMode (int characterMode) noexcept
{
    return juce::jlimit (1, 4, characterMode);
}

float SimpleChoirEngine::getChromaticLeadPitchRatio (float inputFrequencyHz) noexcept
{
    if (inputFrequencyHz <= 0.0f)
        return 1.0f;

    const auto midiFloat = 69.0f + 12.0f * std::log2 (inputFrequencyHz / 440.0f);
    const auto nearestMidi = juce::roundToInt (midiFloat);
    const auto targetHz = static_cast<float> (juce::MidiMessage::getMidiNoteInHertz (nearestMidi));
    return juce::jlimit (minPitchRatio, maxPitchRatio, targetHz / inputFrequencyHz);
}

float SimpleChoirEngine::getCharacterPitchRatio (int slot, int characterMode, float characterAmount) noexcept
{
    static constexpr std::array<float, MidiVoiceState::maxVoices> centsBySlot {{
        -5.0f, 4.0f, -3.0f, 6.0f, -6.0f, 3.0f, -4.0f, 5.0f
    }};

    const auto mode = sanitizeCharacterMode (characterMode);
    const auto amount = juce::jlimit (0.0f, 1.0f, characterAmount);
    // Per-mode detune identity (directions/0708_6.md): Warm gets a light
    // ensemble spread, Vowel a full one; Bright and Digital stay hard-locked
    // (clean vs robotic).
    const auto multiplier = mode == 3 ? 1.0f : (mode == 1 ? 0.4f : 0.0f);
    const auto cents = centsBySlot[static_cast<size_t> (juce::jlimit (0, MidiVoiceState::maxVoices - 1, slot))]
                     * multiplier
                     * amount;

    return std::exp2 (cents / 1200.0f);
}

float SimpleChoirEngine::getCharacterGain (int slot, int characterMode, float characterAmount) noexcept
{
    static constexpr std::array<float, MidiVoiceState::maxVoices> gainOffsetsBySlot {{
        -0.025f, 0.02f, -0.015f, 0.025f, -0.02f, 0.015f, -0.025f, 0.02f
    }};

    const auto mode = sanitizeCharacterMode (characterMode);
    const auto amount = juce::jlimit (0.0f, 1.0f, characterAmount);
    const auto baseAmount = mode == 0 ? 0.0f : (mode == 4 ? 1.0f : 0.55f);
    const auto effectiveAmount = baseAmount * amount;
    return juce::jmax (0.0f,
                       1.0f + gainOffsetsBySlot[static_cast<size_t> (juce::jlimit (0, MidiVoiceState::maxVoices - 1, slot))]
                                  * effectiveAmount);
}

float SimpleChoirEngine::getCharacterDelayOffsetSamples (int slot,
                                                         int characterMode,
                                                         float characterAmount,
                                                         double sampleRate) noexcept
{
    static constexpr std::array<float, MidiVoiceState::maxVoices> delayMsBySlot {{
        0.0f, 1.6f, 3.2f, 4.8f, 2.4f, 4.0f, 5.6f, 7.2f
    }};

    const auto mode = sanitizeCharacterMode (characterMode);
    const auto amount = juce::jlimit (0.0f, 1.0f, characterAmount);
    const auto baseAmount = mode == 4 ? 1.0f : (mode == 3 ? 0.55f : 0.0f);
    const auto delayMs = delayMsBySlot[static_cast<size_t> (juce::jlimit (0, MidiVoiceState::maxVoices - 1, slot))]
                       * baseAmount
                       * amount;

    return delayMs * 0.001f * static_cast<float> (juce::jmax (1.0, sampleRate));
}

void SimpleChoirEngine::configureCharacterTone (VoicePitchState& voice,
                                                int slot,
                                                int characterMode,
                                                float characterAmount,
                                                double sampleRate,
                                                float lfoPhase) noexcept
{
    static constexpr std::array<float, MidiVoiceState::maxVoices> vowelCenterHz {{
        750.0f, 1050.0f, 1350.0f, 1700.0f, 2100.0f, 2500.0f, 950.0f, 1500.0f
    }};
    static constexpr std::array<float, MidiVoiceState::maxVoices> vowelGainDbBySlot {{
        6.0f, -4.0f, 5.0f, -3.5f, 4.5f, -3.0f, 5.5f, -4.0f
    }};

    const auto mode = sanitizeCharacterMode (characterMode);
    const auto amount = juce::jlimit (0.0f, 1.0f, characterAmount);
    const auto safeSlot = static_cast<size_t> (juce::jlimit (0, MidiVoiceState::maxVoices - 1, slot));

    if (voice.lastCharacterMode != mode)
    {
        voice.characterFilter1.reset();
        voice.characterFilter2.reset();
        voice.characterFilter3.reset();
        voice.lastCharacterMode = mode;
    }

    voice.characterFilter1.setIdentity();
    voice.characterFilter2.setIdentity();
    voice.characterFilter3.setIdentity();

    // directions/0708_6.md: sharpen each mode's identity.
    switch (mode)
    {
        case 1: // Warm: dark, thick, tape/tube-ish
            setHighShelfFilter (voice.characterFilter1, 3500.0f, -9.0f * amount, 0.7f, sampleRate);
            setPeakingFilter (voice.characterFilter2, 300.0f, 4.5f * amount, 0.9f, sampleRate);
            break;

        case 2: // Bright: hard, airy, cutting
            setHighShelfFilter (voice.characterFilter1, 6000.0f, 7.0f * amount, 0.7f, sampleRate);
            setPeakingFilter (voice.characterFilter2, 3200.0f, 4.5f * amount, 1.4f, sampleRate);
            setPeakingFilter (voice.characterFilter3, 280.0f, -4.0f * amount, 0.9f, sampleRate);
            break;

        case 3: // Vowel: "moving mouths" — slot formants slowly swept by an LFO
        {
            const auto sweep = 1.0f + 0.18f * amount * std::sin (lfoPhase);
            setPeakingFilter (voice.characterFilter1,
                              vowelCenterHz[safeSlot] * sweep,
                              vowelGainDbBySlot[safeSlot] * 2.2f * amount,
                              2.5f,
                              sampleRate);
            break;
        }

        case 4: // Digital: robotic / lo-fi (decimator lives in applyCharacterTone)
            setHighShelfFilter (voice.characterFilter1, 4500.0f, 6.0f * amount, 0.7f, sampleRate);
            setPeakingFilter (voice.characterFilter2, 2400.0f, 3.0f * amount, 1.2f, sampleRate);
            break;

        default:
            break;
    }
}

float SimpleChoirEngine::applyCharacterTone (VoicePitchState& voice,
                                             float sample,
                                             int slot,
                                             int characterMode,
                                             float characterAmount) noexcept
{
    juce::ignoreUnused (slot);

    const auto mode = sanitizeCharacterMode (characterMode);
    const auto amount = juce::jlimit (0.0f, 1.0f, characterAmount);

    if (amount <= 0.0001f)
        return sample;

    auto coloured = voice.characterFilter1.process (sample);
    coloured = voice.characterFilter2.process (coloured);
    coloured = voice.characterFilter3.process (coloured);

    if (mode == 1)
    {
        coloured = applySoftSaturation (coloured, 1.0f + 0.6f * amount, 0.5f * amount);
    }
    else if (mode == 4)
    {
        // Sample-hold decimator (amount -> hold up to 10 samples, ~4.4 kHz
        // effective rate @44.1 kHz): the iconic lo-fi/robotic digital artifact
        // (directions/0708_6.md). Then hard-ish saturation.
        const auto holdLength = 1 + juce::roundToInt (9.0f * amount);

        if (voice.decimatorHoldCounter <= 0)
        {
            voice.decimatorHoldValue = coloured;
            voice.decimatorHoldCounter = holdLength;
        }

        --voice.decimatorHoldCounter;
        coloured = voice.decimatorHoldValue;
        coloured = applySoftSaturation (coloured, 1.0f + 1.2f * amount, 0.65f * amount);
    }

    return sample + (coloured - sample) * amount;
}

void SimpleChoirEngine::setPeakingFilter (VoicePitchState::CharacterBiquad& filter,
                                          float frequencyHz,
                                          float gainDb,
                                          float q,
                                          double sampleRate) noexcept
{
    if (sampleRate <= 1.0 || std::abs (gainDb) < 0.001f)
    {
        filter.setIdentity();
        return;
    }

    const auto safeFrequency = juce::jlimit (20.0f,
                                             static_cast<float> (sampleRate * 0.45),
                                             frequencyHz);
    const auto safeQ = juce::jmax (0.1f, q);
    const auto a = std::pow (10.0f, gainDb / 40.0f);
    const auto omega = juce::MathConstants<float>::twoPi * safeFrequency / static_cast<float> (sampleRate);
    const auto sinOmega = std::sin (omega);
    const auto cosOmega = std::cos (omega);
    const auto alpha = sinOmega / (2.0f * safeQ);
    const auto b0 = 1.0f + alpha * a;
    const auto b1 = -2.0f * cosOmega;
    const auto b2 = 1.0f - alpha * a;
    const auto a0 = 1.0f + alpha / a;
    const auto a1 = -2.0f * cosOmega;
    const auto a2 = 1.0f - alpha / a;

    filter.b0 = b0 / a0;
    filter.b1 = b1 / a0;
    filter.b2 = b2 / a0;
    filter.a1 = a1 / a0;
    filter.a2 = a2 / a0;
}

void SimpleChoirEngine::setHighShelfFilter (VoicePitchState::CharacterBiquad& filter,
                                            float frequencyHz,
                                            float gainDb,
                                            float q,
                                            double sampleRate) noexcept
{
    if (sampleRate <= 1.0 || std::abs (gainDb) < 0.001f)
    {
        filter.setIdentity();
        return;
    }

    const auto safeFrequency = juce::jlimit (20.0f,
                                             static_cast<float> (sampleRate * 0.45),
                                             frequencyHz);
    const auto safeQ = juce::jmax (0.1f, q);
    const auto a = std::pow (10.0f, gainDb / 40.0f);
    const auto omega = juce::MathConstants<float>::twoPi * safeFrequency / static_cast<float> (sampleRate);
    const auto sinOmega = std::sin (omega);
    const auto cosOmega = std::cos (omega);
    const auto beta = std::sqrt (a) / safeQ;
    const auto twoSqrtAAlpha = beta * sinOmega;
    const auto aPlusOne = a + 1.0f;
    const auto aMinusOne = a - 1.0f;
    const auto b0 = a * (aPlusOne + aMinusOne * cosOmega + twoSqrtAAlpha);
    const auto b1 = -2.0f * a * (aMinusOne + aPlusOne * cosOmega);
    const auto b2 = a * (aPlusOne + aMinusOne * cosOmega - twoSqrtAAlpha);
    const auto a0 = aPlusOne - aMinusOne * cosOmega + twoSqrtAAlpha;
    const auto a1 = 2.0f * (aMinusOne - aPlusOne * cosOmega);
    const auto a2 = aPlusOne - aMinusOne * cosOmega - twoSqrtAAlpha;

    filter.b0 = b0 / a0;
    filter.b1 = b1 / a0;
    filter.b2 = b2 / a0;
    filter.a1 = a1 / a0;
    filter.a2 = a2 / a0;
}

void SimpleChoirEngine::setHighPassFilter (VoicePitchState::CharacterBiquad& filter,
                                           float frequencyHz,
                                           float q,
                                           double sampleRate) noexcept
{
    if (sampleRate <= 1.0)
    {
        filter.setIdentity();
        return;
    }

    const auto safeFrequency = juce::jlimit (20.0f,
                                             static_cast<float> (sampleRate * 0.45),
                                             frequencyHz);
    const auto safeQ = juce::jmax (0.1f, q);
    const auto omega = juce::MathConstants<float>::twoPi * safeFrequency / static_cast<float> (sampleRate);
    const auto sinOmega = std::sin (omega);
    const auto cosOmega = std::cos (omega);
    const auto alpha = sinOmega / (2.0f * safeQ);
    const auto b0 = (1.0f + cosOmega) * 0.5f;
    const auto b1 = -(1.0f + cosOmega);
    const auto b2 = (1.0f + cosOmega) * 0.5f;
    const auto a0 = 1.0f + alpha;
    const auto a1 = -2.0f * cosOmega;
    const auto a2 = 1.0f - alpha;

    filter.b0 = b0 / a0;
    filter.b1 = b1 / a0;
    filter.b2 = b2 / a0;
    filter.a1 = a1 / a0;
    filter.a2 = a2 / a0;
}

float SimpleChoirEngine::applySoftSaturation (float sample, float drive, float amount) noexcept
{
    const auto safeAmount = juce::jlimit (0.0f, 1.0f, amount);
    const auto driven = sample * juce::jmax (1.0f, drive);
    const auto clipped = driven / (1.0f + std::abs (driven));
    return sample + (clipped - sample) * safeAmount;
}

int SimpleChoirEngine::getFixedPitchWindowSamples (double sampleRate) noexcept
{
    return juce::jlimit (inputSyncedMinWindowSamples,
                         inputSyncedMaxWindowSamples,
                         juce::roundToInt (juce::jmax (1.0, sampleRate) * fixedPitchWindowSeconds));
}

int SimpleChoirEngine::getInputSyncedPitchWindowSamples (float inputFrequencyHz, double sampleRate) noexcept
{
    if (inputFrequencyHz <= 0.0f || sampleRate <= 1.0)
        return getFixedPitchWindowSamples (sampleRate);

    const auto periodSamples = static_cast<float> (sampleRate) / inputFrequencyHz;
    return juce::jlimit (inputSyncedMinWindowSamples,
                         inputSyncedMaxWindowSamples,
                         juce::roundToInt (inputSyncedPitchWindowCycles * periodSamples));
}

int SimpleChoirEngine::getLimitedPitchWindowSamples (int currentWindowSamples, int targetWindowSamples) noexcept
{
    const auto current = juce::jlimit (inputSyncedMinWindowSamples,
                                       inputSyncedMaxWindowSamples,
                                       currentWindowSamples);
    const auto target = juce::jlimit (inputSyncedMinWindowSamples,
                                      inputSyncedMaxWindowSamples,
                                      targetWindowSamples);

    if (current <= 0)
        return target;

    const auto ratioLimitedMin = juce::roundToInt (static_cast<float> (current) / maxWindowChangeRatioPerGrain);
    const auto ratioLimitedMax = juce::roundToInt (static_cast<float> (current) * maxWindowChangeRatioPerGrain);
    const auto sampleLimitedMin = current - maxWindowChangeSamplesPerGrain;
    const auto sampleLimitedMax = current + maxWindowChangeSamplesPerGrain;
    const auto limitedMin = juce::jmax (inputSyncedMinWindowSamples,
                                        juce::jmax (ratioLimitedMin, sampleLimitedMin));
    const auto limitedMax = juce::jmin (inputSyncedMaxWindowSamples,
                                        juce::jmin (ratioLimitedMax, sampleLimitedMax));

    return juce::jlimit (limitedMin, limitedMax, target);
}

float SimpleChoirEngine::getWindowPitchSmoothingCoefficient (int samples, double sampleRate) noexcept
{
    if (samples <= 0 || sampleRate <= 1.0)
        return 1.0f;

    return 1.0f - std::exp (-static_cast<float> (samples)
                            / (windowPitchSmoothingSeconds * static_cast<float> (sampleRate)));
}

float SimpleChoirEngine::smoothFrequencyLog (float previous, float target, float alpha) noexcept
{
    if (previous <= 0.0f)
        return target;

    if (target <= 0.0f)
        return previous;

    const auto previousLog = std::log2 (previous);
    const auto targetLog = std::log2 (target);
    return std::exp2 (previousLog + juce::jlimit (0.0f, 1.0f, alpha) * (targetLog - previousLog));
}

float SimpleChoirEngine::updateWindowPitchHz (float correctionInputPitchHz, int samples) noexcept
{
    if (correctionInputPitchHz <= 0.0f)
        return windowPitchHz;

    windowPitchHz = smoothFrequencyLog (windowPitchHz,
                                        correctionInputPitchHz,
                                        getWindowPitchSmoothingCoefficient (samples, currentSampleRate));
    return windowPitchHz;
}

void SimpleChoirEngine::updateWetZeroCrossing (float sampleValue) noexcept
{
    // D1 low-pitch diagnostics: lightweight positive-going zero-crossing frequency
    // estimator for the representative voice's wet output (see directions/0703_1.md,
    // item 3). Only accurate for the sine-wave self test; real vocal input is noisy
    // by nature here, which is expected.
    const auto previous = wetZeroCrossingPreviousSample;
    wetZeroCrossingPreviousSample = sampleValue;

    if (previous < 0.0f && sampleValue >= 0.0f)
    {
        const auto denominator = sampleValue - previous;
        const auto fraction = std::abs (denominator) > 0.0000001f ? (-previous) / denominator : 0.0f;
        const auto crossingIndex = wetZeroCrossingLocalSampleIndex + static_cast<double> (fraction);

        if (wetZeroCrossingLastCrossingSampleIndex >= 0.0)
        {
            const auto periodSamples = crossingIndex - wetZeroCrossingLastCrossingSampleIndex;

            if (periodSamples > 0.0)
            {
                const auto instantaneousHz = static_cast<float> (currentSampleRate / periodSamples);
                wetZeroCrossingEstimatedHz = wetZeroCrossingEstimatedHz <= 0.0f
                                                 ? instantaneousHz
                                                 : smoothFrequencyLog (wetZeroCrossingEstimatedHz, instantaneousHz, 0.35f);
            }
        }

        wetZeroCrossingLastCrossingSampleIndex = crossingIndex;
    }

    wetZeroCrossingLocalSampleIndex += 1.0;
}

float SimpleChoirEngine::readMonoInput (const juce::AudioBuffer<float>& input, int sample) noexcept
{
    const auto channels = input.getNumChannels();

    if (channels <= 0)
        return 0.0f;

    if (channels == 1)
        return input.getSample (0, sample);

    return (input.getSample (0, sample) + input.getSample (1, sample)) * 0.5f;
}

float SimpleChoirEngine::wrapPhase (float phase) noexcept
{
    while (phase >= 1.0f)
        phase -= 1.0f;

    while (phase < 0.0f)
        phase += 1.0f;

    return phase;
}

float SimpleChoirEngine::windowGain (float phase) noexcept
{
    return 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * phase);
}

void SimpleChoirEngine::resetVoice (VoicePitchState& voice) noexcept
{
    voice.lastMidiNote = -1;
    voice.wasActive = false;
    voice.lastCharacterMode = -1;
    voice.phaseA = 0.0f;
    voice.phaseB = 0.5f;
    voice.currentPitchRatio = 1.0f;
    voice.targetPitchRatio = 1.0f;
    voice.envelopeGain = 0.0f;
    voice.targetEnvelopeGain = 0.0f;
    voice.leftGain = 0.0f;
    voice.rightGain = 0.0f;
    voice.monoGain = 0.0f;
    voice.delayOffsetSamples = 0.0f;
    voice.characterFilter1.setIdentity();
    voice.characterFilter2.setIdentity();
    voice.characterFilter3.setIdentity();
    voice.characterFilter1.reset();
    voice.characterFilter2.reset();
    voice.characterFilter3.reset();
    voice.decimatorHoldValue = 0.0f;
    voice.decimatorHoldCounter = 0;
    voice.windowSamplesA = pitchWindowSamples;
    voice.windowSamplesB = pitchWindowSamples;
}

float SimpleChoirEngine::readDelayLine (float delaySamples) const noexcept
{
    auto readPosition = static_cast<float> (writeIndex) - delaySamples;

    while (readPosition < 0.0f)
        readPosition += static_cast<float> (delayBufferSize);

    while (readPosition >= static_cast<float> (delayBufferSize))
        readPosition -= static_cast<float> (delayBufferSize);

    const auto index0 = static_cast<int> (readPosition);
    const auto index1 = (index0 + 1) % delayBufferSize;
    const auto fraction = readPosition - static_cast<float> (index0);
    const auto* delay = delayBuffer.getReadPointer (0);

    return delay[index0] + (delay[index1] - delay[index0]) * fraction;
}

float SimpleChoirEngine::renderPitchShiftedSample (VoicePitchState& voice,
                                                   float glideCoefficient,
                                                   float delayOffsetSamples,
                                                   int targetWindowSamples) noexcept
{
    if (glideCoefficient >= 1.0f)
        voice.currentPitchRatio = voice.targetPitchRatio;
    else
        voice.currentPitchRatio = smoothFrequencyLog (voice.currentPitchRatio,
                                                      voice.targetPitchRatio,
                                                      glideCoefficient);

    const auto ratio = juce::jlimit (minPitchRatio, maxPitchRatio, voice.currentPitchRatio);
    const auto safeTargetWindowSamples = juce::jlimit (inputSyncedMinWindowSamples,
                                                       inputSyncedMaxWindowSamples,
                                                       targetWindowSamples);
    const auto windowSamplesA = juce::jlimit (inputSyncedMinWindowSamples,
                                              inputSyncedMaxWindowSamples,
                                              voice.windowSamplesA);
    const auto windowSamplesB = juce::jlimit (inputSyncedMinWindowSamples,
                                              inputSyncedMaxWindowSamples,
                                              voice.windowSamplesB);
    const auto windowSamplesAFloat = static_cast<float> (windowSamplesA);
    const auto windowSamplesBFloat = static_cast<float> (windowSamplesB);
    const auto phaseDeltaA = (1.0f - ratio) / windowSamplesAFloat;
    const auto phaseDeltaB = (1.0f - ratio) / windowSamplesBFloat;

    const auto baseDelay = static_cast<float> (minimumDelaySamples) + juce::jmax (0.0f, delayOffsetSamples);
    const auto delayA = baseDelay + voice.phaseA * windowSamplesAFloat;
    const auto delayB = baseDelay + voice.phaseB * windowSamplesBFloat;
    const auto gainA = windowGain (voice.phaseA);
    const auto gainB = windowGain (voice.phaseB);
    const auto gainSum = gainA + gainB + 0.000001f;
    const auto shifted = (readDelayLine (delayA) * gainA + readDelayLine (delayB) * gainB) / gainSum;

    const auto previousPhaseA = voice.phaseA;
    const auto previousPhaseB = voice.phaseB;
    voice.phaseA = wrapPhase (voice.phaseA + phaseDeltaA);
    voice.phaseB = wrapPhase (voice.phaseB + phaseDeltaB);

    if (didPhaseWrap (previousPhaseA, voice.phaseA, phaseDeltaA))
        voice.windowSamplesA = getLimitedPitchWindowSamples (windowSamplesA, safeTargetWindowSamples);
    else
        voice.windowSamplesA = windowSamplesA;

    if (didPhaseWrap (previousPhaseB, voice.phaseB, phaseDeltaB))
        voice.windowSamplesB = getLimitedPitchWindowSamples (windowSamplesB, safeTargetWindowSamples);
    else
        voice.windowSamplesB = windowSamplesB;

    return shifted;
}

} // namespace voxchord
