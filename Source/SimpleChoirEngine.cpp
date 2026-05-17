#include "SimpleChoirEngine.h"

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

    for (auto& voice : voiceStates)
    {
        voice.pitchRatio.reset (currentSampleRate, 0.03);
        resetVoice (voice);
    }

    reset();
}

void SimpleChoirEngine::reset() noexcept
{
    delayBuffer.clear();
    writeIndex = 0;

    for (auto& voice : voiceStates)
        resetVoice (voice);
}

void SimpleChoirEngine::render (const juce::AudioBuffer<float>& dryInput,
                                juce::AudioBuffer<float>& wetOutput,
                                const MidiVoiceState::NoteSnapshot& activeNotes,
                                int voiceLimit,
                                float spread) noexcept
{
    wetOutput.clear();

    const auto samples = juce::jmin (dryInput.getNumSamples(), wetOutput.getNumSamples());
    const auto outputChannels = wetOutput.getNumChannels();

    if (samples <= 0 || outputChannels <= 0 || delayBufferSize <= 0)
        return;

    const auto safeVoiceLimit = juce::jlimit (1, MidiVoiceState::maxVoices, voiceLimit);
    const auto activeCount = countActiveVoices (activeNotes, safeVoiceLimit);

    std::array<int, MidiVoiceState::maxVoices> activeSlots {};
    std::array<float, MidiVoiceState::maxVoices> leftGains {};
    std::array<float, MidiVoiceState::maxVoices> rightGains {};
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
        const auto targetRatio = getPitchRatioForNote (midiNote);

        if (! voice.wasActive || voice.lastMidiNote != midiNote)
        {
            voice.phaseA = 0.0f;
            voice.phaseB = 0.5f;
            voice.pitchRatio.setCurrentAndTargetValue (targetRatio);
            voice.wasActive = true;
            voice.lastMidiNote = midiNote;
        }
        else
        {
            voice.pitchRatio.setTargetValue (targetRatio);
        }

        const auto pan = getPanForVoice (activeIndex, activeCount, spread);
        const auto voiceGain = 1.0f / static_cast<float> (activeCount);

        activeSlots[static_cast<size_t> (activeIndex)] = slot;
        leftGains[static_cast<size_t> (activeIndex)] = voiceGain * (pan <= 0.0f ? 1.0f : 1.0f - pan);
        rightGains[static_cast<size_t> (activeIndex)] = voiceGain * (pan >= 0.0f ? 1.0f : 1.0f + pan);
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
            const auto shifted = renderPitchShiftedSample (voice);

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

float SimpleChoirEngine::getPitchRatioForNote (int midiNote) noexcept
{
    const auto targetFrequency = static_cast<float> (juce::MidiMessage::getMidiNoteInHertz (midiNote));
    return juce::jlimit (minPitchRatio, maxPitchRatio, targetFrequency / referenceFrequencyHz);
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
    voice.pitchRatio.setCurrentAndTargetValue (1.0f);
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

float SimpleChoirEngine::renderPitchShiftedSample (VoicePitchState& voice) noexcept
{
    const auto ratio = voice.pitchRatio.getNextValue();
    const auto phaseDelta = (1.0f - ratio) / static_cast<float> (pitchWindowSamples);

    const auto delayA = static_cast<float> (minimumDelaySamples) + voice.phaseA * static_cast<float> (pitchWindowSamples);
    const auto delayB = static_cast<float> (minimumDelaySamples) + voice.phaseB * static_cast<float> (pitchWindowSamples);
    const auto gainA = windowGain (voice.phaseA);
    const auto gainB = windowGain (voice.phaseB);
    const auto gainSum = gainA + gainB + 0.000001f;
    const auto shifted = (readDelayLine (delayA) * gainA + readDelayLine (delayB) * gainB) / gainSum;

    voice.phaseA = wrapPhase (voice.phaseA + phaseDelta);
    voice.phaseB = wrapPhase (voice.phaseB + phaseDelta);

    return shifted;
}

} // namespace voxchord
