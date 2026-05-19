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
    pitchWindowSamples = juce::jlimit (256, 4096, juce::roundToInt (currentSampleRate * 0.018));
    minimumDelaySamples = juce::jlimit (32, 1024, juce::roundToInt (currentSampleRate * 0.004));
    delayBufferSize = juce::jmax (minimumDelaySamples + pitchWindowSamples * 2 + maxBlockSize + 8,
                                  juce::roundToInt (currentSampleRate * 0.25));

    delayBuffer.setSize (1, delayBufferSize, false, false, true);
    pitchDetector.prepare (currentSampleRate);

    reset();
}

void SimpleChoirEngine::reset() noexcept
{
    delayBuffer.clear();
    writeIndex = 0;
    pitchDetector.reset();
    lastDetectedInputFrequencyHz = 0.0f;
    pitchState = {};

    for (auto& voice : voiceStates)
        resetVoice (voice);
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
}

void SimpleChoirEngine::runPitchShifterSelfTest()
{
    struct TestCase
    {
        float inputFrequencyHz = 0.0f;
        float ratio = 1.0f;
    };

    constexpr double testSampleRate = 48000.0;
    constexpr auto maxBlockSize = 512;
    constexpr auto totalSamples = 57600;
    constexpr auto skipSamples = 16800;
    constexpr std::array<TestCase, 9> testCases {{
        { 440.0f, 1.0f },
        { 660.0f, 1.0f },
        { 880.0f, 1.0f },
        { 220.0f, 2.0f },
        { 440.0f, 2.0f },
        { 440.0f, 1.5f },
        { 440.0f, 0.5f },
        { 660.0f, 0.5f },
        { 880.0f, 0.5f }
    }};

    DBG ("VoxChord PitchShifter SelfTest: fixed ratio, Character=0, Glide disabled, VoiceCount=1 equivalent");

    auto summary = PitchShifterSelfTestSummary {};
    summary.hasRun = true;
    summary.passed = true;

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
        const auto phaseDelta = (1.0f - testCase.ratio) / static_cast<float> (engine.pitchWindowSamples);
        const auto baseDelay = static_cast<float> (engine.minimumDelaySamples);
        auto previousPhaseA = voice.phaseA;
        auto previousPhaseB = voice.phaseB;
        auto previousDelayA = baseDelay + previousPhaseA * static_cast<float> (engine.pitchWindowSamples);
        auto previousDelayB = baseDelay + previousPhaseB * static_cast<float> (engine.pitchWindowSamples);
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

            const auto shifted = engine.renderPitchShiftedSample (voice, 1.0f, 0.0f);

            const auto currentDelayA = baseDelay + voice.phaseA * static_cast<float> (engine.pitchWindowSamples);
            const auto currentDelayB = baseDelay + voice.phaseB * static_cast<float> (engine.pitchWindowSamples);
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
        const auto delayStepPerSample = phaseDelta * static_cast<float> (engine.pitchWindowSamples);
        const auto theoreticalReadSpeed = 1.0f - delayStepPerSample;
        const auto withinTenCents = std::abs (errorCents) <= 10.0f;

        if (absoluteErrorCents > summary.maxErrorCents)
        {
            summary.maxErrorCents = absoluteErrorCents;
            summary.worstInputHz = testCase.inputFrequencyHz;
            summary.worstRatio = testCase.ratio;
            summary.worstActualRatio = actualRatio;
            summary.worstMeasuredHz = measuredHz;
        }

        if (! withinTenCents)
            summary.passed = false;

        DBG (juce::String ("PitchShifterSelfTest input ")
             + juce::String (testCase.inputFrequencyHz, 1) + " Hz"
             + ", ratio " + juce::String (testCase.ratio, 3)
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
             + ", pitchWindowSamples: " + juce::String (engine.pitchWindowSamples)
             + ", minimumDelaySamples: " + juce::String (engine.minimumDelaySamples)
             + ", ratio smoothing/glide disabled: yes");

        if (shouldReportSpectrum (testCase.inputFrequencyHz, testCase.ratio))
        {
            const auto spectrum = analyseSpectrum (output, testSampleRate, expectedHz, measuredHz);
            auto spectrumText = juce::String ("PitchShifterSpectrum input ")
                              + juce::String (testCase.inputFrequencyHz, 1) + " Hz"
                              + ", ratio " + juce::String (testCase.ratio, 3)
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

    DBG (juce::String ("PitchShifterSelfTest summary: ")
         + (summary.passed ? "PASS" : "FAIL")
         + ", max error " + juce::String (summary.maxErrorCents, 2) + " cents"
         + ", worst input " + juce::String (summary.worstInputHz, 1) + " Hz"
         + ", worst ratio " + juce::String (summary.worstRatio, 3)
         + ", worst actual ratio " + juce::String (summary.worstActualRatio, 6)
         + ", worst measured " + juce::String (summary.worstMeasuredHz, 2) + " Hz");
}

PitchShifterSelfTestSummary SimpleChoirEngine::getPitchShifterSelfTestSummary() noexcept
{
    return pitchShifterSelfTestSummary();
}

void SimpleChoirEngine::render (const juce::AudioBuffer<float>& dryInput,
                                juce::AudioBuffer<float>& wetOutput,
                                const MidiVoiceState::NoteSnapshot& activeNotes,
                                int voiceLimit,
                                float spread,
                                float tune,
                                float glide,
                                float character) noexcept
{
    wetOutput.clear();

    const auto samples = juce::jmin (dryInput.getNumSamples(), wetOutput.getNumSamples());
    const auto outputChannels = wetOutput.getNumChannels();

    if (samples <= 0 || outputChannels <= 0 || delayBufferSize <= 0)
        return;

    juce::ignoreUnused (tune);

    const auto safeVoiceLimit = juce::jlimit (1, MidiVoiceState::maxVoices, voiceLimit);
    const auto safeGlide = juce::jlimit (0.0f, 1.0f, glide);
    const auto safeCharacter = juce::jlimit (0.0f, 1.0f, character);
    const auto activeCount = countActiveVoices (activeNotes, safeVoiceLimit);
    const auto glideCoefficient = getGlideCoefficient (safeGlide, currentSampleRate);
    const auto ratioSmoothingCoefficient = safeGlide <= 0.001f
                                               ? ratioSmoothingAlpha
                                               : juce::jmin (ratioSmoothingAlpha, glideCoefficient);
    pitchState = pitchDetector.processBlock (dryInput);
    pitchState.ratioSmoothingCoefficient = ratioSmoothingCoefficient;
    const auto inputFrequencyHz = pitchState.correctionInputPitchHz;
    lastDetectedInputFrequencyHz = inputFrequencyHz;

    std::array<int, MidiVoiceState::maxVoices> activeSlots {};
    std::array<float, MidiVoiceState::maxVoices> leftGains {};
    std::array<float, MidiVoiceState::maxVoices> rightGains {};
    std::array<float, MidiVoiceState::maxVoices> delayOffsets {};
    auto activeIndex = 0;

    for (auto slot = 0; slot < MidiVoiceState::maxVoices; ++slot)
    {
        auto& voice = voiceStates[static_cast<size_t> (slot)];
        const auto active = slot < safeVoiceLimit && activeNotes[static_cast<size_t> (slot)] >= 0;

        if (! active)
        {
            voice.wasActive = false;
            voice.lastMidiNote = -1;
            continue;
        }

        const auto midiNote = activeNotes[static_cast<size_t> (slot)];
        const auto targetRatio = juce::jlimit (minPitchRatio,
                                               maxPitchRatio,
                                               getPitchRatioForNote (midiNote, inputFrequencyHz)
                                                   * getCharacterPitchRatio (slot, safeCharacter));

        const auto wasAlreadyActive = voice.wasActive;

        if (! wasAlreadyActive || voice.lastMidiNote != midiNote)
        {
            if (! wasAlreadyActive)
            {
                voice.phaseA = 0.0f;
                voice.phaseB = 0.5f;
            }

            voice.targetPitchRatio = targetRatio;

            if (! wasAlreadyActive || safeGlide <= 0.001f)
                voice.currentPitchRatio = targetRatio;

            voice.wasActive = true;
            voice.lastMidiNote = midiNote;
        }
        else
        {
            voice.targetPitchRatio = targetRatio;
        }

        const auto pan = getPanForVoice (activeIndex, activeCount, spread);
        const auto voiceGain = getCharacterGain (slot, safeCharacter) / static_cast<float> (activeCount);

        activeSlots[static_cast<size_t> (activeIndex)] = slot;
        leftGains[static_cast<size_t> (activeIndex)] = voiceGain * (pan <= 0.0f ? 1.0f : 1.0f - pan);
        rightGains[static_cast<size_t> (activeIndex)] = voiceGain * (pan >= 0.0f ? 1.0f : 1.0f + pan);
        delayOffsets[static_cast<size_t> (activeIndex)] = getCharacterDelayOffsetSamples (slot,
                                                                                          safeCharacter,
                                                                                          currentSampleRate);
        ++activeIndex;
    }

    auto* delay = delayBuffer.getWritePointer (0);
    auto* left = wetOutput.getWritePointer (0);
    auto* right = outputChannels > 1 ? wetOutput.getWritePointer (1) : nullptr;

    for (auto sample = 0; sample < samples; ++sample)
    {
        delay[writeIndex] = readMonoInput (dryInput, sample);

        for (auto index = 0; index < activeCount; ++index)
        {
            auto& voice = voiceStates[static_cast<size_t> (activeSlots[static_cast<size_t> (index)])];
            const auto shifted = renderPitchShiftedSample (voice,
                                                           ratioSmoothingCoefficient,
                                                           delayOffsets[static_cast<size_t> (index)]);

            left[sample] += shifted * (outputChannels > 1
                                            ? leftGains[static_cast<size_t> (index)]
                                            : 1.0f / static_cast<float> (activeCount));

            if (right != nullptr)
                right[sample] += shifted * rightGains[static_cast<size_t> (index)];
        }

        writeIndex = (writeIndex + 1) % delayBufferSize;
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
        state.voiced = false;
        return;
    }

    const auto correctedPitch = correctHarmonicPitch (rawPitch);
    state.correctedPitchHz = correctedPitch;

    if (correctedPitch <= 0.0f)
    {
        state.voiced = false;
        return;
    }

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

float SimpleChoirEngine::getCharacterPitchRatio (int slot, float character) noexcept
{
    static constexpr std::array<float, MidiVoiceState::maxVoices> centsBySlot { -14.0f, 10.0f, -9.0f, 18.0f };

    const auto cents = centsBySlot[static_cast<size_t> (juce::jlimit (0, MidiVoiceState::maxVoices - 1, slot))]
                     * juce::jlimit (0.0f, 1.0f, character);

    return std::exp2 (cents / 1200.0f);
}

float SimpleChoirEngine::getCharacterGain (int slot, float character) noexcept
{
    static constexpr std::array<float, MidiVoiceState::maxVoices> gainOffsetsBySlot { -0.07f, 0.05f, -0.04f, 0.06f };

    return juce::jmax (0.0f,
                       1.0f + gainOffsetsBySlot[static_cast<size_t> (juce::jlimit (0, MidiVoiceState::maxVoices - 1, slot))]
                                  * juce::jlimit (0.0f, 1.0f, character));
}

float SimpleChoirEngine::getCharacterDelayOffsetSamples (int slot, float character, double sampleRate) noexcept
{
    static constexpr std::array<float, MidiVoiceState::maxVoices> delayMsBySlot { 0.0f, 4.0f, 8.0f, 12.0f };

    const auto delayMs = delayMsBySlot[static_cast<size_t> (juce::jlimit (0, MidiVoiceState::maxVoices - 1, slot))]
                       * juce::jlimit (0.0f, 1.0f, character);

    return delayMs * 0.001f * static_cast<float> (juce::jmax (1.0, sampleRate));
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
    voice.phaseA = 0.0f;
    voice.phaseB = 0.5f;
    voice.currentPitchRatio = 1.0f;
    voice.targetPitchRatio = 1.0f;
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
                                                   float delayOffsetSamples) noexcept
{
    if (glideCoefficient >= 1.0f)
        voice.currentPitchRatio = voice.targetPitchRatio;
    else
        voice.currentPitchRatio += (voice.targetPitchRatio - voice.currentPitchRatio) * glideCoefficient;

    const auto ratio = juce::jlimit (minPitchRatio, maxPitchRatio, voice.currentPitchRatio);
    const auto phaseDelta = (1.0f - ratio) / static_cast<float> (pitchWindowSamples);

    const auto baseDelay = static_cast<float> (minimumDelaySamples) + juce::jmax (0.0f, delayOffsetSamples);
    const auto delayA = baseDelay + voice.phaseA * static_cast<float> (pitchWindowSamples);
    const auto delayB = baseDelay + voice.phaseB * static_cast<float> (pitchWindowSamples);
    const auto gainA = windowGain (voice.phaseA);
    const auto gainB = windowGain (voice.phaseB);
    const auto gainSum = gainA + gainB + 0.000001f;
    const auto shifted = (readDelayLine (delayA) * gainA + readDelayLine (delayB) * gainB) / gainSum;

    voice.phaseA = wrapPhase (voice.phaseA + phaseDelta);
    voice.phaseB = wrapPhase (voice.phaseB + phaseDelta);

    return shifted;
}

} // namespace voxchord
