#include "VoxChordParameters.h"

namespace voxchord
{
namespace
{
    juce::String formatPercent (float value, int)
    {
        return juce::String (juce::roundToInt (value * 100.0f)) + "%";
    }

    juce::String formatDecibels (float value, int)
    {
        return juce::String (value, 1) + " dB";
    }

    juce::AudioParameterFloatAttributes percentAttributes()
    {
        return juce::AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction (formatPercent);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
    using ParameterID = juce::ParameterID;

    Layout layout;

    layout.add (std::make_unique<juce::AudioParameterInt> (
        ParameterID { ParameterIDs::voiceCount, 1 },
        "Voice Count",
        1,
        4,
        4,
        juce::AudioParameterIntAttributes()
            .withStringFromValueFunction ([] (int value, int) { return juce::String (value); })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParameterID { ParameterIDs::tune, 1 },
        "Tune",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f },
        0.80f,
        percentAttributes()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParameterID { ParameterIDs::glide, 1 },
        "Glide",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f },
        0.15f,
        percentAttributes()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParameterID { ParameterIDs::character, 1 },
        "Character",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f },
        0.35f,
        percentAttributes()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParameterID { ParameterIDs::spread, 1 },
        "Spread",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f },
        0.55f,
        percentAttributes()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParameterID { ParameterIDs::dryWet, 1 },
        "Dry/Wet",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f },
        0.50f,
        percentAttributes()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParameterID { ParameterIDs::outputLevel, 1 },
        "Output",
        juce::NormalisableRange<float> { -24.0f, 6.0f, 0.1f },
        -3.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("dB")
            .withStringFromValueFunction (formatDecibels)));

    return layout;
}

} // namespace voxchord

