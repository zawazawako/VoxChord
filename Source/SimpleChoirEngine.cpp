#include "SimpleChoirEngine.h"

#include <cmath>
#include <limits>

namespace voxchord
{
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

    const auto safeVoiceLimit = juce::jlimit (1, MidiVoiceState::maxVoices, voiceLimit);
    const auto safeTune = juce::jlimit (0.0f, 1.0f, tune);
    const auto safeGlide = juce::jlimit (0.0f, 1.0f, glide);
    const auto safeCharacter = juce::jlimit (0.0f, 1.0f, character);
    const auto activeCount = countActiveVoices (activeNotes, safeVoiceLimit);
    const auto glideCoefficient = getGlideCoefficient (safeGlide, currentSampleRate);
    pitchState = pitchDetector.processBlock (dryInput);
    const auto inputFrequencyHz = pitchState.stablePitchHz;
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
                                               applyTuneToRatio (getPitchRatioForNote (midiNote, inputFrequencyHz), safeTune)
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
                                                           glideCoefficient,
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
    state = {};
    ringBuffer.fill (0.0f);
    analysisFrame.fill (0.0f);
    difference.fill (0.0f);
    cmndf.fill (0.0f);
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
        state.stablePitchHz = 0.0f;

    if (state.stablePitchHz <= 0.0f)
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
        state.confidence = 0.0f;
        state.voiced = false;
        return;
    }

    const auto rawPitch = detectPitchYin();
    state.rawPitchHz = rawPitch;

    if (rawPitch <= 0.0f || state.confidence < confidenceThreshold)
    {
        state.voiced = false;
        return;
    }

    const auto candidatePitch = chooseStableCandidate (rawPitch);

    if (candidatePitch <= 0.0f)
    {
        state.voiced = false;
        return;
    }

    if (state.stablePitchHz <= 0.0f)
    {
        state.stablePitchHz = candidatePitch;
    }
    else
    {
        state.stablePitchHz += (candidatePitch - state.stablePitchHz) * smoothingAlpha;
    }

    state.voiced = true;
    samplesSinceAccepted = 0;
}

float SimpleChoirEngine::SimplePitchDetector::detectPitchYin() noexcept
{
    const auto minLag = juce::jlimit (2, frameLength - 2, juce::roundToInt (sampleRateHz / maxFrequencyHz));
    const auto maxLag = juce::jlimit (minLag + 1, frameLength - 2, juce::roundToInt (sampleRateHz / minFrequencyHz));

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

    auto bestTau = -1;
    auto bestValue = 1.0f;

    for (auto tau = minLag; tau <= maxLag; ++tau)
    {
        const auto value = cmndf[static_cast<size_t> (tau)];

        if (value < bestValue)
        {
            bestValue = value;
            bestTau = tau;
        }

        if (value < yinThreshold)
        {
            bestTau = tau;

            while (bestTau + 1 <= maxLag && cmndf[static_cast<size_t> (bestTau + 1)] < cmndf[static_cast<size_t> (bestTau)])
                ++bestTau;

            bestValue = cmndf[static_cast<size_t> (bestTau)];
            break;
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

float SimpleChoirEngine::SimplePitchDetector::chooseStableCandidate (float rawPitchHz) const noexcept
{
    if (rawPitchHz < minFrequencyHz || rawPitchHz > maxFrequencyHz)
        return 0.0f;

    if (state.stablePitchHz <= 0.0f)
        return rawPitchHz;

    std::array<float, 3> candidates { rawPitchHz * 0.5f, rawPitchHz, rawPitchHz * 2.0f };
    auto bestCandidate = rawPitchHz;
    auto bestDistance = std::numeric_limits<float>::max();

    for (const auto candidate : candidates)
    {
        if (candidate < minFrequencyHz || candidate > maxFrequencyHz)
            continue;

        const auto distance = std::abs (std::log2 (candidate / state.stablePitchHz));

        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestCandidate = candidate;
        }
    }

    const auto centsFromStable = std::abs (1200.0f * std::log2 (bestCandidate / state.stablePitchHz));

    if (centsFromStable > 600.0f && state.confidence < 0.9f)
        return 0.0f;

    return bestCandidate;
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

float SimpleChoirEngine::getPitchRatioForNote (int midiNote, float inputFrequencyHz) noexcept
{
    const auto targetFrequency = static_cast<float> (juce::MidiMessage::getMidiNoteInHertz (midiNote));
    const auto referenceFrequency = inputFrequencyHz > 0.0f ? inputFrequencyHz : fallbackReferenceFrequencyHz;

    return juce::jlimit (minPitchRatio, maxPitchRatio, targetFrequency / referenceFrequency);
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
