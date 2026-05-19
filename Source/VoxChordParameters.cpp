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
        8,
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
        0.0f,
        percentAttributes()));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        ParameterID { ParameterIDs::characterMode, 1 },
        "Character",
        juce::StringArray { "Clean", "Warm", "Bright", "Vowel", "Digital" },
        1));

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

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParameterID { ParameterIDs::inputGainDb, 1 },
        "Input Gain",
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("dB")
            .withStringFromValueFunction (formatDecibels)));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        ParameterID { ParameterIDs::inputSource, 1 },
        "Input Source",
        juce::StringArray { "Auto", "Input 1", "Input 2", "Mix 1+2" },
        0));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        ParameterID { ParameterIDs::leadTuneEnabled, 1 },
        "Lead Tune",
        false));

    return layout;
}

} // namespace voxchord
