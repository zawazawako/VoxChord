#include "PluginEditor.h"

namespace
{
    juce::Colour backgroundColour()
    {
        return juce::Colour::fromRGB (12, 15, 18);
    }

    juce::Colour panelColour()
    {
        return juce::Colour::fromRGB (28, 34, 39);
    }

    juce::Colour accentColour()
    {
        return juce::Colour::fromRGB (146, 214, 197);
    }

    juce::String formatPeak (float gain)
    {
        if (gain <= 0.000001f)
            return "-inf dB";

        return juce::String (juce::Decibels::gainToDecibels (gain), 1) + " dB";
    }
}

VoxChordAudioProcessorEditor::VoxChordAudioProcessorEditor (VoxChordAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    auto& state = processorRef.getValueTreeState();

    titleLabel.setText ("VoxChord", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    titleLabel.setFont (juce::FontOptions { 34.0f, juce::Font::bold });
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("MIDI-controlled digital choir", juce::dontSendNotification);
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colour::fromRGB (165, 176, 181));
    subtitleLabel.setFont (juce::FontOptions { 15.0f });
    addAndMakeVisible (subtitleLabel);

    configureSlider (voiceCountSlider, voiceCountLabel, "Voices");
    configureSlider (tuneSlider, tuneLabel, "Tune");
    configureSlider (glideSlider, glideLabel, "Glide");
    configureSlider (characterSlider, characterLabel, "Character");
    configureSlider (spreadSlider, spreadLabel, "Spread");
    configureSlider (dryWetSlider, dryWetLabel, "Dry/Wet");
    configureSlider (outputSlider, outputLabel, "Output");

    voiceCountSlider.setNumDecimalPlacesToDisplay (0);
    outputSlider.setNumDecimalPlacesToDisplay (1);

    voiceCountAttachment = std::make_unique<SliderAttachment> (state, voxchord::ParameterIDs::voiceCount, voiceCountSlider);
    tuneAttachment = std::make_unique<SliderAttachment> (state, voxchord::ParameterIDs::tune, tuneSlider);
    glideAttachment = std::make_unique<SliderAttachment> (state, voxchord::ParameterIDs::glide, glideSlider);
    characterAttachment = std::make_unique<SliderAttachment> (state, voxchord::ParameterIDs::character, characterSlider);
    spreadAttachment = std::make_unique<SliderAttachment> (state, voxchord::ParameterIDs::spread, spreadSlider);
    dryWetAttachment = std::make_unique<SliderAttachment> (state, voxchord::ParameterIDs::dryWet, dryWetSlider);
    outputAttachment = std::make_unique<SliderAttachment> (state, voxchord::ParameterIDs::outputLevel, outputSlider);

    midiNotesLabel.setText ("MIDI: --", juce::dontSendNotification);
    midiNotesLabel.setJustificationType (juce::Justification::centredLeft);
    midiNotesLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    midiNotesLabel.setColour (juce::Label::backgroundColourId, panelColour());
    addAndMakeVisible (midiNotesLabel);

    inputMeterLabel.setText ("In: -inf dB", juce::dontSendNotification);
    inputMeterLabel.setJustificationType (juce::Justification::centred);
    inputMeterLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    inputMeterLabel.setColour (juce::Label::backgroundColourId, panelColour());
    addAndMakeVisible (inputMeterLabel);

    outputMeterLabel.setText ("Out: -inf dB", juce::dontSendNotification);
    outputMeterLabel.setJustificationType (juce::Justification::centred);
    outputMeterLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    outputMeterLabel.setColour (juce::Label::backgroundColourId, panelColour());
    addAndMakeVisible (outputMeterLabel);

    panicButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (126, 39, 45));
    panicButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (166, 52, 59));
    panicButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    panicButton.onClick = [this]
    {
        processorRef.panic();
        processorRef.clearClipFlags();
    };
    addAndMakeVisible (panicButton);

    setSize (760, 420);
    startTimerHz (30);
}

VoxChordAudioProcessorEditor::~VoxChordAudioProcessorEditor()
{
    stopTimer();
}

void VoxChordAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (backgroundColour());

    auto bounds = getLocalBounds().reduced (18);
    auto header = bounds.removeFromTop (72);
    auto controls = bounds.removeFromTop (208).reduced (0, 8);
    auto status = bounds.reduced (0, 10);

    g.setColour (panelColour());
    g.fillRoundedRectangle (header.toFloat(), 16.0f);
    g.fillRoundedRectangle (controls.toFloat(), 18.0f);
    g.fillRoundedRectangle (status.toFloat(), 16.0f);

    g.setColour (accentColour().withAlpha (0.55f));
    g.drawRoundedRectangle (controls.toFloat(), 18.0f, 1.5f);
}

void VoxChordAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (26);

    auto header = bounds.removeFromTop (56);
    titleLabel.setBounds (header.removeFromTop (36));
    subtitleLabel.setBounds (header);

    bounds.removeFromTop (18);

    auto controls = bounds.removeFromTop (190);
    const auto knobWidth = controls.getWidth() / 7;

    layoutSlider (voiceCountSlider, voiceCountLabel, controls.removeFromLeft (knobWidth).reduced (5));
    layoutSlider (tuneSlider, tuneLabel, controls.removeFromLeft (knobWidth).reduced (5));
    layoutSlider (glideSlider, glideLabel, controls.removeFromLeft (knobWidth).reduced (5));
    layoutSlider (characterSlider, characterLabel, controls.removeFromLeft (knobWidth).reduced (5));
    layoutSlider (spreadSlider, spreadLabel, controls.removeFromLeft (knobWidth).reduced (5));
    layoutSlider (dryWetSlider, dryWetLabel, controls.removeFromLeft (knobWidth).reduced (5));
    layoutSlider (outputSlider, outputLabel, controls.reduced (5));

    bounds.removeFromTop (24);

    auto status = bounds.removeFromTop (74);
    panicButton.setBounds (status.removeFromRight (122).reduced (6));
    outputMeterLabel.setBounds (status.removeFromRight (132).reduced (6));
    inputMeterLabel.setBounds (status.removeFromRight (132).reduced (6));
    midiNotesLabel.setBounds (status.reduced (6));
}

void VoxChordAudioProcessorEditor::timerCallback()
{
    updateMidiNotes();
    updateMeters();
}

void VoxChordAudioProcessorEditor::configureSlider (juce::Slider& slider,
                                                    juce::Label& label,
                                                    const juce::String& text)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 24);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accentColour());
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour::fromRGB (70, 79, 84));
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colour::fromRGB (210, 220, 222));
    label.setFont (juce::FontOptions { 14.0f, juce::Font::bold });
    addAndMakeVisible (label);
}

void VoxChordAudioProcessorEditor::layoutSlider (juce::Slider& slider,
                                                 juce::Label& label,
                                                 juce::Rectangle<int> bounds)
{
    label.setBounds (bounds.removeFromTop (24));
    slider.setBounds (bounds);
}

void VoxChordAudioProcessorEditor::updateMidiNotes()
{
    const auto notes = processorRef.getActiveMidiNotes();
    auto text = juce::String ("MIDI: ");
    auto anyNotes = false;

    for (const auto note : notes)
    {
        if (note < 0)
            continue;

        if (anyNotes)
            text += " ";

        text += juce::MidiMessage::getMidiNoteName (note, true, true, 3);
        anyNotes = true;
    }

    if (! anyNotes)
        text += "--";

    midiNotesLabel.setText (text, juce::dontSendNotification);
}

void VoxChordAudioProcessorEditor::updateMeters()
{
    const auto& meters = processorRef.getLevelMeterState();

    inputMeterLabel.setText ("In: " + formatPeak (meters.getInputPeak()), juce::dontSendNotification);
    outputMeterLabel.setText ("Out: " + formatPeak (meters.getOutputPeak()), juce::dontSendNotification);

    inputMeterLabel.setColour (juce::Label::textColourId,
                               meters.getInputClipped() ? juce::Colours::orangered : juce::Colours::white);
    outputMeterLabel.setColour (juce::Label::textColourId,
                                meters.getOutputClipped() ? juce::Colours::orangered : juce::Colours::white);
}

