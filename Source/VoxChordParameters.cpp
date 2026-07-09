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

int characterModeGuiIndexToInternalMode (int guiIndex) noexcept
{
    switch (guiIndex)
    {
        case 0:  return 1; // Warm
        case 1:  return 2; // Bright
        case 2:  return 3; // Vowel
        case 3:  return 4; // Digital
        default: break;
    }

    return juce::jlimit (1, 4, guiIndex + 1);
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

    // "Retune" = Lead Tune snap speed / Retune Speed (directions/0708_9.md).
    // Parameter ID stays `tune` for state compatibility; only the display
    // name changes. Deliberately not given an on-screen knob (one-screen UI
    // principle): the default 1.0 = instant hard-tune snap is the intended
    // live behaviour, and the value stays host-automatable for the rest.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParameterID { ParameterIDs::tune, 1 },
        "Retune",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f },
        1.0f,
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
        juce::StringArray { "Warm", "Bright", "Vowel", "Digital" },
        0));

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

    layout.add (std::make_unique<juce::AudioParameterBool> (
        ParameterID { ParameterIDs::monoOutputEnabled, 1 },
        "Mono Out",
        false));

    // Selects the TD-PSOLA shifter for the wet harmony voices instead of the
    // classic windowed shifter. Now the default engine and presented as
    // "High Quality"; unchecking it falls back to the Classic window shifter,
    // which is kept in the codebase (directions/0709_1.md). ID is unchanged.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        ParameterID { ParameterIDs::psolaEnabled, 1 },
        "High Quality",
        true));

    return layout;
}

} // namespace voxchord
