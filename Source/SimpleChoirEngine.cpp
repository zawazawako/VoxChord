#include "SimpleChoirEngine.h"

#include <cmath>

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
    const auto inputFrequencyHz = pitchDetector.processBlock (dryInput);

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
    reset();
}

void SimpleChoirEngine::SimplePitchDetector::reset() noexcept
{
    samplesSinceCrossing = 0;
    smoothedFrequencyHz = 0.0f;
    previousSample = 0.0f;
    wasBelowLowThreshold = false;
}

float SimpleChoirEngine::SimplePitchDetector::processBlock (const juce::AudioBuffer<float>& input) noexcept
{
    static constexpr auto lowThreshold = -0.015f;
    static constexpr auto highThreshold = 0.015f;
    static constexpr auto minFrequencyHz = 70.0f;
    static constexpr auto maxFrequencyHz = 1000.0f;

    const auto samples = input.getNumSamples();

    if (samples <= 0)
        return smoothedFrequencyHz;

    const auto minPeriodSamples = juce::jmax (1, juce::roundToInt (sampleRateHz / maxFrequencyHz));
    const auto maxPeriodSamples = juce::roundToInt (sampleRateHz / minFrequencyHz);

    for (auto sample = 0; sample < samples; ++sample)
    {
        const auto monoSample = SimpleChoirEngine::readMonoInput (input, sample);
        ++samplesSinceCrossing;

        if (monoSample < lowThreshold)
            wasBelowLowThreshold = true;

        if (wasBelowLowThreshold && previousSample <= highThreshold && monoSample > highThreshold)
        {
            if (samplesSinceCrossing >= minPeriodSamples && samplesSinceCrossing <= maxPeriodSamples)
            {
                const auto detectedFrequency = static_cast<float> (sampleRateHz / static_cast<double> (samplesSinceCrossing));
                smoothedFrequencyHz = smoothedFrequencyHz <= 0.0f
                                          ? detectedFrequency
                                          : smoothedFrequencyHz + (detectedFrequency - smoothedFrequencyHz) * 0.2f;
            }

            samplesSinceCrossing = 0;
            wasBelowLowThreshold = false;
        }

        if (samplesSinceCrossing > maxPeriodSamples * 2)
        {
            smoothedFrequencyHz *= 0.98f;

            if (smoothedFrequencyHz < minFrequencyHz)
                smoothedFrequencyHz = 0.0f;

            samplesSinceCrossing = maxPeriodSamples;
        }

        previousSample = monoSample;
    }

    return smoothedFrequencyHz;
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
