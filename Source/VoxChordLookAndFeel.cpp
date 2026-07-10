#include "VoxChordLookAndFeel.h"

namespace voxchord
{

VoxChordLookAndFeel::VoxChordLookAndFeel()
{
    // Popup menus (combo box dropdowns).
    setColour (juce::PopupMenu::backgroundColourId, ui::panelRecessed());
    setColour (juce::PopupMenu::textColourId, ui::textMid());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::accent().withAlpha (0.22f));
    setColour (juce::PopupMenu::highlightedTextColourId, ui::textHigh());

    setColour (juce::ComboBox::textColourId, ui::textHigh());
    setColour (juce::ComboBox::arrowColourId, ui::textLow());

    setColour (juce::TextEditor::backgroundColourId, ui::well());
    setColour (juce::TextEditor::textColourId, ui::textHigh());
    setColour (juce::TextEditor::highlightColourId, ui::accent().withAlpha (0.35f));
    setColour (juce::TextEditor::focusedOutlineColourId, ui::accent().withAlpha (0.6f));
}

void VoxChordLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPosProportional, float rotaryStartAngle,
                                            float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> { x, y, width, height }.toFloat().reduced (6.0f);
    const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto square = bounds.withSizeKeepingCentre (size, size);
    const auto centre = square.getCentre();
    const auto trackThickness = juce::jmax (3.5f, size * 0.08f);
    const auto arcRadius = size * 0.5f - trackThickness * 0.5f;
    const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // Neutral track: the accent is reserved for the value arc.
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (ui::trackNeutral());
    g.strokePath (track, { trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

    if (sliderPosProportional > 0.002f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                             rotaryStartAngle, angle, true);
        g.setColour (slider.isEnabled() ? ui::accent() : ui::accent().withAlpha (0.4f));
        g.strokePath (value, { trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
    }

    // Knob body: shadow first, then fill, then a faint top rim (elevation cue).
    const auto bodyRadius = arcRadius - trackThickness * 1.35f;
    const auto bodyRect = juce::Rectangle<float> {}.withSize (bodyRadius * 2.0f, bodyRadius * 2.0f)
                              .withCentre (centre);

    juce::Path body;
    body.addEllipse (bodyRect);
    knobShadow.render (g, body);
    g.setColour (ui::knobFace());
    g.fillPath (body);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawEllipse (bodyRect.reduced (0.6f), 1.2f);

    // Pointer.
    const auto pointerOuter = bodyRadius - 3.0f;
    const auto pointerInner = bodyRadius * 0.42f;
    const juce::Point<float> tip { centre.x + std::sin (angle) * pointerOuter,
                                   centre.y - std::cos (angle) * pointerOuter };
    const juce::Point<float> base { centre.x + std::sin (angle) * pointerInner,
                                    centre.y - std::cos (angle) * pointerInner };
    g.setColour (ui::textHigh());
    g.drawLine ({ base, tip }, 2.6f);
}

void VoxChordLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float minSliderPos, float maxSliderPos,
                                            juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                                minSliderPos, maxSliderPos, style, slider);
        return;
    }

    const auto bounds = juce::Rectangle<int> { x, y, width, height }.toFloat();
    const auto trackHeight = 5.0f;
    const auto track = bounds.withSizeKeepingCentre (bounds.getWidth() - 8.0f, trackHeight);

    // Recessed neutral track, accent-filled up to the value.
    g.setColour (ui::well());
    g.fillRoundedRectangle (track, trackHeight * 0.5f);

    const auto fillRight = juce::jlimit (track.getX(), track.getRight(), sliderPos);
    g.setColour (slider.isEnabled() ? ui::accent() : ui::accent().withAlpha (0.4f));
    g.fillRoundedRectangle (track.withRight (fillRight), trackHeight * 0.5f);

    const auto thumbRadius = 7.0f;
    const juce::Rectangle<float> thumb { fillRight - thumbRadius, bounds.getCentreY() - thumbRadius,
                                         thumbRadius * 2.0f, thumbRadius * 2.0f };
    g.setColour (ui::knobFace());
    g.fillEllipse (thumb);
    g.setColour (ui::accent());
    g.drawEllipse (thumb.reduced (0.8f), 1.6f);
}

void VoxChordLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                        int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<int> { 0, 0, width, height }.toFloat().reduced (0.5f);

    // Recessed field (a value container, so it sits *into* the panel).
    g.setColour (ui::panelRecessed());
    g.fillRoundedRectangle (bounds, ui::controlCornerRadius);
    // Dropdowns read as clickable fields, so their outline is deliberately
    // stronger than the hairline used for static panels.
    g.setColour (box.hasKeyboardFocus (true) ? ui::accent().withAlpha (0.75f)
                                             : juce::Colours::white.withAlpha (0.30f));
    g.drawRoundedRectangle (bounds, ui::controlCornerRadius, 1.4f);

    // Chevron.
    const auto arrowZone = bounds.removeFromRight (26.0f);
    juce::Path chevron;
    const auto cx = arrowZone.getCentreX();
    const auto cy = arrowZone.getCentreY() - 1.0f;
    chevron.startNewSubPath (cx - 4.5f, cy - 1.5f);
    chevron.lineTo (cx, cy + 3.0f);
    chevron.lineTo (cx + 4.5f, cy - 1.5f);
    g.setColour (ui::textLow());
    g.strokePath (chevron, { 1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
}

void VoxChordLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                            bool shouldDrawButtonAsHighlighted, bool)
{
    // Metrics mirror LookAndFeel_V4 so header alignment math keeps working.
    const auto fontSize = juce::jmin (15.0f, static_cast<float> (button.getHeight()) * 0.75f);
    const auto tickWidth = fontSize * 1.1f;
    const auto on = button.getToggleState();

    const juce::Rectangle<float> tickBounds { 4.0f,
                                              (static_cast<float> (button.getHeight()) - tickWidth) * 0.5f,
                                              tickWidth, tickWidth };

    if (on)
    {
        g.setColour (ui::accent());
        g.fillRoundedRectangle (tickBounds, 4.5f);
        // Check mark in the background colour so the accent carries the state.
        juce::Path check;
        const auto b = tickBounds.reduced (tickWidth * 0.24f);
        check.startNewSubPath (b.getX(), b.getCentreY() + b.getHeight() * 0.08f);
        check.lineTo (b.getX() + b.getWidth() * 0.36f, b.getBottom() - b.getHeight() * 0.12f);
        check.lineTo (b.getRight(), b.getY() + b.getHeight() * 0.1f);
        g.setColour (ui::background());
        g.strokePath (check, { 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
    }
    else
    {
        g.setColour (ui::well());
        g.fillRoundedRectangle (tickBounds, 4.5f);
        g.setColour (juce::Colours::white.withAlpha (shouldDrawButtonAsHighlighted ? 0.30f : 0.16f));
        g.drawRoundedRectangle (tickBounds.reduced (0.5f), 4.5f, 1.2f);
    }

    g.setColour (on ? ui::textHigh() : ui::textMid());
    g.setFont (juce::FontOptions { fontSize });
    g.drawFittedText (button.getButtonText(),
                      button.getLocalBounds()
                          .withTrimmedLeft (juce::roundToInt (tickWidth) + 10)
                          .withTrimmedRight (2),
                      juce::Justification::centredLeft, 10);
}

void VoxChordLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                const juce::Colour& backgroundColour,
                                                bool shouldDrawButtonAsHighlighted,
                                                bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (1.5f);
    auto colour = backgroundColour;

    if (shouldDrawButtonAsDown)
        colour = colour.brighter (0.25f);
    else if (shouldDrawButtonAsHighlighted)
        colour = colour.brighter (0.12f);

    juce::Path shape;
    shape.addRoundedRectangle (bounds, ui::controlCornerRadius);

    if (! shouldDrawButtonAsDown)
        buttonShadow.render (g, shape);

    g.setColour (colour);
    g.fillPath (shape);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), ui::controlCornerRadius, 1.0f);
}

} // namespace voxchord
