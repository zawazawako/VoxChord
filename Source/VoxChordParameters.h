#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace voxchord
{
namespace ParameterIDs
{
    inline constexpr auto voiceCount = "voiceCount";
    inline constexpr auto tune = "tune";
    inline constexpr auto glide = "glide";
    inline constexpr auto character = "character";
    inline constexpr auto spread = "spread";
    inline constexpr auto dryWet = "dryWet";
    inline constexpr auto outputLevel = "outputLevel";
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace voxchord

