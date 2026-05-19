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

    juce::Colour dangerColour()
    {
        return juce::Colour::fromRGB (236, 94, 78);
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
#if JUCE_DEBUG
        const auto rawText = state.rawPitchHz > 0.0f ? juce::String (state.rawPitchHz, 1) + " Hz" : "--";
        const auto correctedText = state.correctedPitchHz > 0.0f ? juce::String (state.correctedPitchHz, 1) + " Hz" : "--";
        const auto displayText = state.displayStablePitchHz > 0.0f ? juce::String (state.displayStablePitchHz, 1) + " Hz" : "--";
        const auto correctionText = state.correctionInputPitchHz > 0.0f ? juce::String (state.correctionInputPitchHz, 1) + " Hz" : "--";

        return juce::String ("Build: character-type-amount-001 | ")
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
#else
        juce::ignoreUnused (state, selfTestSummary);
        return juce::String ("VoxChord v") + JucePlugin_VersionString;
#endif
    }

    void configureStatusLabel (juce::Label& label, const juce::String& text, juce::Justification justification)
    {
        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (justification);
        label.setColour (juce::Label::textColourId, juce::Colours::white);
        label.setColour (juce::Label::backgroundColourId, panelColour());
    }
}

void VoxChordAudioProcessorEditor::MeterBar::setTitle (const juce::String& newTitle)
{
    title = newTitle;
    repaint();
}

void VoxChordAudioProcessorEditor::MeterBar::setLevel (float newPeak, bool isClipped)
{
    peak = juce::jlimit (0.0f, 1.5f, newPeak);
    clipped = isClipped;
    repaint();
}

void VoxChordAudioProcessorEditor::MeterBar::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto labelArea = bounds.removeFromTop (18.0f);
    const auto meterArea = bounds.reduced (0.0f, 3.0f);
    const auto db = juce::Decibels::gainToDecibels (peak, -60.0f);
    const auto normalized = juce::jmap (juce::jlimit (-60.0f, 0.0f, db), -60.0f, 0.0f, 0.0f, 1.0f);
    const auto fillWidth = meterArea.getWidth() * normalized;

    g.setColour (juce::Colour::fromRGB (200, 211, 214));
    g.setFont (juce::FontOptions { 12.5f, juce::Font::bold });
    g.drawFittedText (title + ": " + formatPeak (peak), labelArea.toNearestInt(), juce::Justification::centredLeft, 1);

    g.setColour (juce::Colour::fromRGB (20, 25, 29));
    g.fillRoundedRectangle (meterArea, 5.0f);

    auto fill = meterArea.withWidth (fillWidth);
    g.setGradientFill (juce::ColourGradient (accentColour(),
                                             fill.getX(),
                                             fill.getY(),
                                             clipped ? dangerColour() : juce::Colour::fromRGB (223, 234, 150),
                                             fill.getRight(),
                                             fill.getY(),
                                             false));
    g.fillRoundedRectangle (fill, 5.0f);

    g.setColour (clipped ? dangerColour() : accentColour().withAlpha (0.65f));
    g.drawRoundedRectangle (meterArea, 5.0f, 1.2f);
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
    configureSlider (glideSlider, glideLabel, "Glide");
    configureSlider (characterAmountSlider, characterLabel, "Amount");
    configureSlider (spreadSlider, spreadLabel, "Spread");
    configureSlider (dryWetSlider, dryWetLabel, "Dry/Wet");
    configureCompactSlider (inputGainSlider, inputGainLabel, "Input Gain");
    configureCompactSlider (outputSlider, outputLabel, "Output");

    voiceCountSlider.setNumDecimalPlacesToDisplay (0);
    characterAmountSlider.setNumDecimalPlacesToDisplay (0);
    inputGainSlider.setNumDecimalPlacesToDisplay (1);
    outputSlider.setNumDecimalPlacesToDisplay (1);

    voiceCountAttachment = std::make_unique<SliderAttachment> (state, voxchord::ParameterIDs::voiceCount, voiceCountSlider);
    glideAttachment = std::make_unique<SliderAttachment> (state, voxchord::ParameterIDs::glide, glideSlider);
    characterAmountAttachment = std::make_unique<SliderAttachment> (state,
                                                                    voxchord::ParameterIDs::character,
                                                                    characterAmountSlider);
    spreadAttachment = std::make_unique<SliderAttachment> (state, voxchord::ParameterIDs::spread, spreadSlider);
    dryWetAttachment = std::make_unique<SliderAttachment> (state, voxchord::ParameterIDs::dryWet, dryWetSlider);
    inputGainAttachment = std::make_unique<SliderAttachment> (state, voxchord::ParameterIDs::inputGainDb, inputGainSlider);
    outputAttachment = std::make_unique<SliderAttachment> (state, voxchord::ParameterIDs::outputLevel, outputSlider);

    characterTypeLabel.setText ("Char Type", juce::dontSendNotification);
    characterTypeLabel.setJustificationType (juce::Justification::centred);
    characterTypeLabel.setColour (juce::Label::textColourId, juce::Colour::fromRGB (210, 220, 222));
    characterTypeLabel.setFont (juce::FontOptions { 14.0f, juce::Font::bold });
    addAndMakeVisible (characterTypeLabel);

    characterModeBox.addItem ("Warm", 2);
    characterModeBox.addItem ("Bright", 3);
    characterModeBox.addItem ("Vowel", 4);
    characterModeBox.addItem ("Digital", 5);
    characterModeBox.setColour (juce::ComboBox::backgroundColourId, panelColour());
    characterModeBox.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    characterModeBox.setColour (juce::ComboBox::outlineColourId, accentColour().withAlpha (0.55f));
    addAndMakeVisible (characterModeBox);
    characterModeAttachment = std::make_unique<ComboBoxAttachment> (state,
                                                                    voxchord::ParameterIDs::characterMode,
                                                                    characterModeBox);

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

    leadTuneButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    leadTuneButton.setColour (juce::ToggleButton::tickColourId, accentColour());
    addAndMakeVisible (leadTuneButton);
    leadTuneAttachment = std::make_unique<ButtonAttachment> (state,
                                                             voxchord::ParameterIDs::leadTuneEnabled,
                                                             leadTuneButton);

    configureStatusLabel (midiNotesLabel, "MIDI: -- | Pitch: --", juce::Justification::centredLeft);
    addAndMakeVisible (midiNotesLabel);

    configureStatusLabel (voiceSlotsLabel, "Slots: V1 -- | V2 -- | V3 -- | V4 -- | V5 off | V6 off | V7 off | V8 off",
                          juce::Justification::centredLeft);
    addAndMakeVisible (voiceSlotsLabel);

    configureStatusLabel (midiStatusLabel, "Last: --", juce::Justification::centredLeft);
    addAndMakeVisible (midiStatusLabel);

    configureStatusLabel (pitchDebugLabel, "Pitch: --", juce::Justification::centred);
    addAndMakeVisible (pitchDebugLabel);

    inputMeter.setTitle ("Input");
    outputMeter.setTitle ("Output");
    addAndMakeVisible (inputMeter);
    addAndMakeVisible (outputMeter);

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
    auto body = bounds.reduced (0, 8);
    auto rightColumn = body.removeFromRight (236);
    body.removeFromRight (14);
    auto controls = body.removeFromTop (198);
    auto status = body.reduced (0, 10);
    auto rightTop = rightColumn.removeFromTop (244);
    rightColumn.removeFromTop (14);
    auto rightBottom = rightColumn;

    g.setColour (panelColour());
    g.fillRoundedRectangle (header.toFloat(), 16.0f);
    g.fillRoundedRectangle (controls.toFloat(), 18.0f);
    g.fillRoundedRectangle (status.toFloat(), 16.0f);
    g.fillRoundedRectangle (rightTop.toFloat(), 18.0f);
    g.fillRoundedRectangle (rightBottom.toFloat(), 18.0f);

    g.setColour (accentColour().withAlpha (0.55f));
    g.drawRoundedRectangle (controls.toFloat(), 18.0f, 1.5f);
    g.drawRoundedRectangle (rightTop.toFloat(), 18.0f, 1.5f);
    g.drawRoundedRectangle (rightBottom.toFloat(), 18.0f, 1.5f);
}

void VoxChordAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (26);

    auto header = bounds.removeFromTop (56);
    titleLabel.setBounds (header.removeFromTop (34));
    subtitleLabel.setBounds (header);

    bounds.removeFromTop (18);

    auto rightColumn = bounds.removeFromRight (220);
    bounds.removeFromRight (18);

    auto rightTop = rightColumn.removeFromTop (228).reduced (10);
    auto inputRow = rightTop.removeFromTop (34);
    inputSourceLabel.setBounds (inputRow.removeFromLeft (48));
    inputSourceBox.setBounds (inputRow);
    rightTop.removeFromTop (8);
    layoutCompactSlider (inputGainSlider, inputGainLabel, rightTop.removeFromTop (42));
    rightTop.removeFromTop (6);
    layoutCompactSlider (outputSlider, outputLabel, rightTop.removeFromTop (42));
    rightTop.removeFromTop (8);
    leadTuneButton.setBounds (rightTop.removeFromTop (26).reduced (18, 0));
    rightTop.removeFromTop (8);
    panicButton.setBounds (rightTop.removeFromTop (34).reduced (18, 0));

    rightColumn.removeFromTop (28);
    auto meterArea = rightColumn.reduced (10);
    inputMeter.setBounds (meterArea.removeFromTop (54));
    meterArea.removeFromTop (10);
    outputMeter.setBounds (meterArea.removeFromTop (54));

    auto controls = bounds.removeFromTop (190);
    const auto knobWidth = controls.getWidth() / 6;

    layoutSlider (voiceCountSlider, voiceCountLabel, controls.removeFromLeft (knobWidth).reduced (5));
    layoutSlider (glideSlider, glideLabel, controls.removeFromLeft (knobWidth).reduced (5));
    layoutComboBox (characterModeBox, characterTypeLabel, controls.removeFromLeft (knobWidth).reduced (5));
    layoutSlider (characterAmountSlider, characterLabel, controls.removeFromLeft (knobWidth).reduced (5));
    layoutSlider (spreadSlider, spreadLabel, controls.removeFromLeft (knobWidth).reduced (5));
    layoutSlider (dryWetSlider, dryWetLabel, controls.reduced (5));

    bounds.removeFromTop (24);

    auto status = bounds.removeFromTop (128);
    auto topRow = status.removeFromTop (42);
    midiNotesLabel.setBounds (topRow.reduced (6));

    auto slotRow = status.removeFromTop (42);
    voiceSlotsLabel.setBounds (slotRow.reduced (6));

    auto eventRow = status.removeFromTop (42);
    pitchDebugLabel.setBounds (eventRow.removeFromRight (170).reduced (6));
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

void VoxChordAudioProcessorEditor::configureCompactSlider (juce::Slider& slider,
                                                           juce::Label& label,
                                                           const juce::String& text)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 22);
    slider.setColour (juce::Slider::trackColourId, accentColour());
    slider.setColour (juce::Slider::backgroundColourId, juce::Colour::fromRGB (58, 66, 70));
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, juce::Colour::fromRGB (210, 220, 222));
    label.setFont (juce::FontOptions { 12.5f, juce::Font::bold });
    addAndMakeVisible (label);
}

void VoxChordAudioProcessorEditor::layoutSlider (juce::Slider& slider,
                                                 juce::Label& label,
                                                 juce::Rectangle<int> bounds)
{
    label.setBounds (bounds.removeFromTop (24));
    slider.setBounds (bounds);
}

void VoxChordAudioProcessorEditor::layoutComboBox (juce::ComboBox& comboBox,
                                                   juce::Label& label,
                                                   juce::Rectangle<int> bounds)
{
    label.setBounds (bounds.removeFromTop (24));
    comboBox.setBounds (bounds.withSizeKeepingCentre (juce::jmin (bounds.getWidth() - 8, 116), 34));
}

void VoxChordAudioProcessorEditor::layoutCompactSlider (juce::Slider& slider,
                                                        juce::Label& label,
                                                        juce::Rectangle<int> bounds)
{
    label.setBounds (bounds.removeFromTop (18));
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

    inputMeter.setLevel (meters.getInputPeak(), meters.getInputClipped());
    outputMeter.setLevel (meters.getOutputPeak(), meters.getOutputClipped());
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
