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
    inline constexpr auto characterMode = "characterMode";
    inline constexpr auto spread = "spread";
    inline constexpr auto dryWet = "dryWet";
    inline constexpr auto outputLevel = "outputLevel";
    inline constexpr auto inputGainDb = "inputGainDb";
    inline constexpr auto inputSource = "inputSource";
    inline constexpr auto leadTuneEnabled = "leadTuneEnabled";
    inline constexpr auto monoOutputEnabled = "monoOutputEnabled";
    inline constexpr auto psolaEnabled = "psolaEnabled";
}

int characterModeGuiIndexToInternalMode (int guiIndex) noexcept;
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace voxchord
