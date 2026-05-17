#include "SimpleChoirEngine.h"

namespace voxchord
{
void SimpleChoirEngine::prepare (int maxBlockSize)
{
    preparedBlockSize = juce::jmax (0, maxBlockSize);
}

void SimpleChoirEngine::reset() noexcept
{
}

void SimpleChoirEngine::render (const juce::AudioBuffer<float>& dryInput,
                                juce::AudioBuffer<float>& wetOutput,
                                const MidiVoiceState::NoteSnapshot& activeNotes,
                                int voiceLimit,
                                float spread) noexcept
{
    juce::ignoreUnused (preparedBlockSize);

    wetOutput.clear();

    const auto samples = juce::jmin (dryInput.getNumSamples(), wetOutput.getNumSamples());
    const auto outputChannels = wetOutput.getNumChannels();

    if (samples <= 0 || outputChannels <= 0)
        return;

    const auto activeCount = countActiveVoices (activeNotes, voiceLimit);

    if (activeCount <= 0)
        return;

    const auto voiceGain = 1.0f / static_cast<float> (activeCount);
    auto activeIndex = 0;

    for (auto slot = 0; slot < juce::jlimit (1, MidiVoiceState::maxVoices, voiceLimit); ++slot)
    {
        if (activeNotes[static_cast<size_t> (slot)] < 0)
            continue;

        const auto pan = getPanForVoice (activeIndex, activeCount, spread);
        const auto leftGain = voiceGain * (pan <= 0.0f ? 1.0f : 1.0f - pan);
        const auto rightGain = voiceGain * (pan >= 0.0f ? 1.0f : 1.0f + pan);

        auto* left = wetOutput.getWritePointer (0);
        auto* right = outputChannels > 1 ? wetOutput.getWritePointer (1) : nullptr;

        for (auto sample = 0; sample < samples; ++sample)
        {
            const auto mono = readMonoInput (dryInput, sample);

            left[sample] += mono * (outputChannels > 1 ? leftGain : voiceGain);

            if (right != nullptr)
                right[sample] += mono * rightGain;
        }

        ++activeIndex;
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

float SimpleChoirEngine::readMonoInput (const juce::AudioBuffer<float>& input, int sample) noexcept
{
    const auto channels = input.getNumChannels();

    if (channels <= 0)
        return 0.0f;

    if (channels == 1)
        return input.getSample (0, sample);

    return (input.getSample (0, sample) + input.getSample (1, sample)) * 0.5f;
}

} // namespace voxchord
