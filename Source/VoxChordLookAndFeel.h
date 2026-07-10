#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <melatonin_blur/melatonin_blur.h>

namespace voxchord
{
// Central palette + metrics: the single source of truth for the editor and the
// LookAndFeel, so colours and radii stay consistent across every control
// (vst-ui ui-quality-guide.md section 4/5).
namespace ui
{
    inline juce::Colour background()    { return juce::Colour::fromRGB (9, 12, 15); }
    inline juce::Colour backgroundTop() { return juce::Colour::fromRGB (17, 21, 26); }
    inline juce::Colour panel()         { return juce::Colour::fromRGB (31, 38, 44); }
    inline juce::Colour panelRecessed() { return juce::Colour::fromRGB (18, 23, 27); }
    inline juce::Colour well()          { return juce::Colour::fromRGB (11, 15, 18); }
    inline juce::Colour knobFace()      { return juce::Colour::fromRGB (44, 53, 60); }
    inline juce::Colour trackNeutral()  { return juce::Colour::fromRGB (54, 63, 69); }
    inline juce::Colour accent()        { return juce::Colour::fromRGB (146, 214, 197); }
    inline juce::Colour danger()        { return juce::Colour::fromRGB (236, 94, 78); }
    inline juce::Colour textHigh()      { return juce::Colour::fromRGB (240, 246, 246); }
    inline juce::Colour textMid()       { return juce::Colour::fromRGB (198, 210, 212); }
    inline juce::Colour textLow()       { return juce::Colour::fromRGB (128, 143, 150); }
    inline juce::Colour hairline()      { return juce::Colours::white.withAlpha (0.06f); }

    constexpr float cardCornerRadius = 16.0f;
    constexpr float controlCornerRadius = 7.0f;
} // namespace ui

// Custom LookAndFeel: solid knobs with cached drop shadows, rounded recessed
// combo boxes, accent-disciplined toggles, and a shadowed PANIC button.
// Toggle-button metrics intentionally match LookAndFeel_V4 (tick = font * 1.1,
// text offset +10) because the header layout math relies on them.
class VoxChordLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    VoxChordLookAndFeel();

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override;

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override;

    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override;

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    // Cached blurs (must be members, never constructed inside paint —
    // ui-quality-guide.md section 2).
    melatonin::DropShadow knobShadow { juce::Colours::black.withAlpha (0.45f), 9, { 0, 3 } };
    melatonin::DropShadow buttonShadow { juce::Colours::black.withAlpha (0.35f), 7, { 0, 2 } };
};

} // namespace voxchord
