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

    juce::String midiActivityToString (VoxChordAudioProcessor::MidiActivity activity)
    {
        switch (activity)
        {
            case VoxChordAudioProcessor::MidiActivity::noteOn:       return "Note On";
            case VoxChordAudioProcessor::MidiActivity::noteOff:      return "Note Off";
            case VoxChordAudioProcessor::MidiActivity::allNotesOff:  return "All Notes Off";
            case VoxChordAudioProcessor::MidiActivity::panic:        return "Panic";
            case VoxChordAudioProcessor::MidiActivity::none:         break;
        }

        return "--";
    }

    juce::String formatPitchDebug (float pitchHz)
    {
        if (pitchHz <= 0.0f)
            return "Pitch: --";

        return "Pitch: " + juce::String (pitchHz, 1) + " Hz";
    }

    juce::String harmonicCorrectionToString (int mode)
    {
        switch (mode)
        {
            case 2:  return "raw/2";
            case 3:  return "raw/3";
            case -2: return "raw*2";
            case -3: return "raw*3";
            default: break;
        }

        return "none";
    }

    juce::String formatPitchShifterSelfTestModeSummary (const char* label,
                                                        const voxchord::PitchShifterSelfTestModeSummary& summary)
    {
        if (! summary.hasRun)
            return juce::String (label) + ": NOT RUN";

        return juce::String (label) + ": " + juce::String (summary.passed ? "PASS" : "FAIL")
             + " | MaxErr: " + juce::String (summary.maxErrorCents, 2) + " c"
             + " | WorstIn: " + juce::String (summary.worstInputHz, 1) + " Hz"
             + " | Ratio: " + juce::String (summary.worstRatio, 3)
             + " | Actual: " + juce::String (summary.worstActualRatio, 5)
             + " | Meas: " + juce::String (summary.worstMeasuredHz, 2) + " Hz";
    }

    juce::String formatDetailedPitchDebug (const voxchord::PitchState& state,
                                           const voxchord::PitchShifterSelfTestSummary& selfTestSummary)
    {
        const auto rawText = state.rawPitchHz > 0.0f ? juce::String (state.rawPitchHz, 1) + " Hz" : "--";
        const auto correctedText = state.correctedPitchHz > 0.0f ? juce::String (state.correctedPitchHz, 1) + " Hz" : "--";
        const auto displayText = state.displayStablePitchHz > 0.0f ? juce::String (state.displayStablePitchHz, 1) + " Hz" : "--";
        const auto correctionText = state.correctionInputPitchHz > 0.0f ? juce::String (state.correctionInputPitchHz, 1) + " Hz" : "--";

        return juce::String ("Build: input-synced-window-continuity-001 | ")
             + "Pitch Shifter SelfTest | "
             + formatPitchShifterSelfTestModeSummary ("Fixed", selfTestSummary.fixedWindow)
             + " | "
             + formatPitchShifterSelfTestModeSummary ("InputSync", selfTestSummary.inputSyncedWindow)
             + " | RMS: " + juce::String (state.inputRmsDb, 1) + " dB"
             + " | Raw: " + rawText
             + " | Corr: " + correctedText
             + " | Disp: " + displayText
             + " | RatioIn: " + correctionText
             + " | Conf: " + juce::String (state.confidence, 2)
             + " | Voiced: " + juce::String (state.voiced ? "yes" : "no")
             + " | Fix: " + harmonicCorrectionToString (state.harmonicCorrectionMode)
             + " | RatioSmooth: " + juce::String (state.ratioSmoothingCoefficient, 2);
    }

    void configureStatusLabel (juce::Label& label, const juce::String& text, juce::Justification justification)
    {
        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (justification);
        label.setColour (juce::Label::textColourId, juce::Colours::white);
        label.setColour (juce::Label::backgroundColourId, panelColour());
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

    subtitleLabel.setText ("MIDI-controlled digital choir | C3 = MIDI 60", juce::dontSendNotification);
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

    inputSourceLabel.setText ("Input", juce::dontSendNotification);
    inputSourceLabel.setJustificationType (juce::Justification::centredRight);
    inputSourceLabel.setColour (juce::Label::textColourId, juce::Colour::fromRGB (210, 220, 222));
    addAndMakeVisible (inputSourceLabel);

    inputSourceBox.addItem ("Auto", 1);
    inputSourceBox.addItem ("Input 1", 2);
    inputSourceBox.addItem ("Input 2", 3);
    inputSourceBox.addItem ("Mix 1+2", 4);
    inputSourceBox.setColour (juce::ComboBox::backgroundColourId, panelColour());
    inputSourceBox.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    inputSourceBox.setColour (juce::ComboBox::outlineColourId, accentColour().withAlpha (0.55f));
    addAndMakeVisible (inputSourceBox);
    inputSourceAttachment = std::make_unique<ComboBoxAttachment> (state,
                                                                  voxchord::ParameterIDs::inputSource,
                                                                  inputSourceBox);

    configureStatusLabel (midiNotesLabel, "MIDI: -- | Pitch: --", juce::Justification::centredLeft);
    addAndMakeVisible (midiNotesLabel);

    configureStatusLabel (voiceSlotsLabel, "Slots: V1 -- | V2 -- | V3 -- | V4 --", juce::Justification::centredLeft);
    addAndMakeVisible (voiceSlotsLabel);

    configureStatusLabel (midiStatusLabel, "Last: --", juce::Justification::centredLeft);
    addAndMakeVisible (midiStatusLabel);

    configureStatusLabel (pitchDebugLabel, "Pitch: --", juce::Justification::centred);
    addAndMakeVisible (pitchDebugLabel);

    configureStatusLabel (inputMeterLabel, "In: -inf dB", juce::Justification::centred);
    addAndMakeVisible (inputMeterLabel);

    configureStatusLabel (outputMeterLabel, "Out: -inf dB", juce::Justification::centred);
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

    setSize (800, 470);
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

    auto status = bounds.removeFromTop (128);
    panicButton.setBounds (status.removeFromRight (126).reduced (6));

    auto topRow = status.removeFromTop (42);
    outputMeterLabel.setBounds (topRow.removeFromRight (136).reduced (6));
    inputMeterLabel.setBounds (topRow.removeFromRight (136).reduced (6));
    midiNotesLabel.setBounds (topRow.reduced (6));

    auto slotRow = status.removeFromTop (42);
    voiceSlotsLabel.setBounds (slotRow.reduced (6));

    auto eventRow = status.removeFromTop (42);
    pitchDebugLabel.setBounds (eventRow.removeFromRight (170).reduced (6));
    auto inputSourceArea = eventRow.removeFromRight (186).reduced (6);
    inputSourceLabel.setBounds (inputSourceArea.removeFromLeft (46));
    inputSourceBox.setBounds (inputSourceArea);
    midiStatusLabel.setBounds (eventRow.reduced (6));
}

void VoxChordAudioProcessorEditor::timerCallback()
{
    updateMidiState();
    updateMeters();
    updatePitchDebug();
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

void VoxChordAudioProcessorEditor::updateMidiState()
{
    const auto notes = processorRef.getActiveMidiNotes();
    const auto voiceLimit = processorRef.getCurrentVoiceLimit();
    auto text = juce::String ("MIDI: ");
    auto slots = juce::String ("Slots: ");
    auto anyNotes = false;

    for (auto index = 0; index < voxchord::MidiVoiceState::maxVoices; ++index)
    {
        const auto note = notes[static_cast<size_t> (index)];

        if (note < 0)
        {
            if (index < voiceLimit)
            {
                if (index > 0)
                    slots += " | ";

                slots += "V" + juce::String (index + 1) + " --";
            }
            else
            {
                if (index > 0)
                    slots += " | ";

                slots += "V" + juce::String (index + 1) + " off";
            }

            continue;
        }

        if (anyNotes)
            text += " ";

        const auto noteName = juce::MidiMessage::getMidiNoteName (note, true, true, 3);
        text += noteName;

        if (index > 0)
            slots += " | ";

        slots += "V" + juce::String (index + 1) + " " + noteName;
        anyNotes = true;
    }

    if (! anyNotes)
        text += "--";

    text += " | " + formatPitchDebug (processorRef.getDetectedInputPitchHz());

    midiNotesLabel.setText (text, juce::dontSendNotification);
    voiceSlotsLabel.setText (slots, juce::dontSendNotification);

    const auto activity = processorRef.getMidiActivitySnapshot();

    if (activity.counter != lastSeenMidiActivityCounter)
    {
        lastSeenMidiActivityCounter = activity.counter;
        midiStatusLabel.setText ("Last: " + midiActivityToString (activity.activity), juce::dontSendNotification);
    }
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

void VoxChordAudioProcessorEditor::updatePitchDebug()
{
    const auto pitchState = processorRef.getPitchState();
    const auto pitchShifterSelfTestSummary = processorRef.getPitchShifterSelfTestSummary();
    const auto pitchText = formatPitchDebug (pitchState.correctionInputPitchHz);

    pitchDebugLabel.setText (pitchText, juce::dontSendNotification);
    subtitleLabel.setText (formatDetailedPitchDebug (pitchState, pitchShifterSelfTestSummary),
                           juce::dontSendNotification);
}
