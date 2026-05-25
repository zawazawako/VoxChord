#include "PluginEditor.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>

namespace
{
    constexpr auto showDebugSelfTestSummary = false;
    constexpr auto showDebugPitchRuntimeDetails = false;

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
        auto text = juce::String ("Build: gui-final-controls-001 | ");

        if constexpr (showDebugSelfTestSummary)
        {
            text += "Pitch Shifter SelfTest | "
                  + formatPitchShifterSelfTestModeSummary ("Fixed", selfTestSummary.fixedWindow)
                  + " | "
                  + formatPitchShifterSelfTestModeSummary ("InputSync", selfTestSummary.inputSyncedWindow)
                  + " | ";
        }
        else
        {
            juce::ignoreUnused (selfTestSummary);
        }

        if constexpr (showDebugPitchRuntimeDetails)
        {
            const auto rawText = state.rawPitchHz > 0.0f ? juce::String (state.rawPitchHz, 1) + " Hz" : "--";
            const auto correctedText = state.correctedPitchHz > 0.0f ? juce::String (state.correctedPitchHz, 1) + " Hz" : "--";
            const auto displayText = state.displayStablePitchHz > 0.0f ? juce::String (state.displayStablePitchHz, 1) + " Hz" : "--";
            const auto correctionText = state.correctionInputPitchHz > 0.0f ? juce::String (state.correctionInputPitchHz, 1) + " Hz" : "--";

            text += "RMS: " + juce::String (state.inputRmsDb, 1) + " dB"
                 + " | Raw: " + rawText
                 + " | Corr: " + correctedText
                 + " | Disp: " + displayText
                 + " | RatioIn: " + correctionText
                 + " | Conf: " + juce::String (state.confidence, 2)
                 + " | Voiced: " + juce::String (state.voiced ? "yes" : "no")
                 + " | Fix: " + harmonicCorrectionToString (state.harmonicCorrectionMode)
                 + " | RatioSmooth: " + juce::String (state.ratioSmoothingCoefficient, 2)
                 + " | ";
        }
        else
        {
            juce::ignoreUnused (state.inputRmsDb,
                                state.rawPitchHz,
                                state.correctedPitchHz,
                                state.displayStablePitchHz,
                                state.correctionInputPitchHz,
                                state.confidence,
                                state.voiced,
                                state.harmonicCorrectionMode,
                                state.ratioSmoothingCoefficient);
        }

        text += "CharMode internal/safe: " + juce::String (state.characterModeRaw)
             + "/" + juce::String (state.characterModeSanitized)
             + " | CharAmt raw/sm: " + juce::String (state.characterAmountRaw, 2)
             + "/" + juce::String (state.characterAmountSmoothed, 2)
             + " | CharIn: " + juce::String (state.inputRmsDb, 1) + " dB"
             + " | CharDelta rms/pk/rel: " + juce::String (state.characterDeltaRms, 5)
             + "/" + juce::String (state.characterDeltaPeak, 5)
             + "/" + juce::String (state.characterDeltaRatioDb, 1) + " dB";

        return text;
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

void VoxChordAudioProcessorEditor::VerticalMeter::setTitle (const juce::String& newTitle)
{
    title = newTitle;
    repaint();
}

void VoxChordAudioProcessorEditor::VerticalMeter::setLevel (float newPeak, bool isClipped)
{
    peak = juce::jlimit (0.0f, 1.5f, newPeak);
    clipped = isClipped;
    repaint();
}

void VoxChordAudioProcessorEditor::VerticalMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto labelArea = bounds.removeFromTop (18.0f);
    const auto meterArea = bounds.reduced (5.0f, 3.0f);
    const auto db = juce::Decibels::gainToDecibels (peak, -60.0f);
    const auto normalized = juce::jmap (juce::jlimit (-60.0f, 0.0f, db), -60.0f, 0.0f, 0.0f, 1.0f);
    const auto fillHeight = meterArea.getHeight() * normalized;

    g.setColour (juce::Colour::fromRGB (200, 211, 214));
    g.setFont (juce::FontOptions { 12.5f, juce::Font::bold });
    g.drawFittedText (title, labelArea.toNearestInt(), juce::Justification::centred, 1);

    g.setColour (juce::Colour::fromRGB (20, 25, 29));
    g.fillRoundedRectangle (meterArea, 5.0f);

    for (auto markDb : { 0.0f, -6.0f, -12.0f, -24.0f, -48.0f })
    {
        const auto y = juce::jmap (markDb, -60.0f, 0.0f, meterArea.getBottom(), meterArea.getY());
        g.setColour (juce::Colour::fromRGB (68, 77, 82));
        g.drawHorizontalLine (juce::roundToInt (y), meterArea.getX(), meterArea.getRight());
    }

    auto fill = meterArea.withY (meterArea.getBottom() - fillHeight).withHeight (fillHeight);
    g.setGradientFill (juce::ColourGradient (accentColour(),
                                             fill.getX(),
                                             fill.getBottom(),
                                             clipped ? dangerColour() : juce::Colour::fromRGB (223, 234, 150),
                                             fill.getX(),
                                             fill.getY(),
                                             false));
    g.fillRoundedRectangle (fill, 5.0f);

    g.setColour (clipped ? dangerColour() : accentColour().withAlpha (0.65f));
    g.drawRoundedRectangle (meterArea, 5.0f, 1.2f);

    g.setColour (juce::Colour::fromRGB (165, 176, 181));
    g.setFont (juce::FontOptions { 10.5f });
    g.drawFittedText (formatPeak (peak), bounds.removeFromBottom (15.0f).toNearestInt(), juce::Justification::centred, 1);
}

void VoxChordAudioProcessorEditor::MiniKeyboard::setActiveNotes (const voxchord::MidiVoiceState::NoteSnapshot& newNotes)
{
    notes = newNotes;
    repaint();
}

bool VoxChordAudioProcessorEditor::MiniKeyboard::isBlackKey (int midiNote) noexcept
{
    const auto note = midiNote % 12;
    return note == 1 || note == 3 || note == 6 || note == 8 || note == 10;
}

bool VoxChordAudioProcessorEditor::MiniKeyboard::isActive (int midiNote) const noexcept
{
    for (const auto note : notes)
    {
        if (note == midiNote)
            return true;
    }

    return false;
}

void VoxChordAudioProcessorEditor::MiniKeyboard::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().reduced (8, 6).toFloat();
    auto whiteKeyCount = 0;

    for (auto note = firstNote; note <= lastNote; ++note)
    {
        if (! isBlackKey (note))
            ++whiteKeyCount;
    }

    const auto whiteKeyWidth = bounds.getWidth() / static_cast<float> (juce::jmax (1, whiteKeyCount));
    const auto blackHeight = bounds.getHeight() * 0.58f;
    const auto blackWidth = whiteKeyWidth * 0.58f;

    const auto whiteIndexForNote = [] (int midiNote)
    {
        auto whiteIndex = 0;

        for (auto note = MiniKeyboard::firstNote; note < midiNote; ++note)
        {
            if (! MiniKeyboard::isBlackKey (note))
                ++whiteIndex;
        }

        return whiteIndex;
    };

    g.setColour (juce::Colour::fromRGB (14, 18, 22));
    g.fillRoundedRectangle (bounds, 6.0f);

    for (auto note = firstNote; note <= lastNote; ++note)
    {
        if (isBlackKey (note))
            continue;

        const auto x = bounds.getX() + static_cast<float> (whiteIndexForNote (note)) * whiteKeyWidth;
        auto key = juce::Rectangle<float> { x, bounds.getY(), whiteKeyWidth - 1.0f, bounds.getHeight() };
        const auto active = isActive (note);
        g.setColour (active ? accentColour() : juce::Colour::fromRGB (214, 222, 222));
        g.fillRoundedRectangle (key.reduced (0.5f), 2.5f);
        g.setColour (juce::Colour::fromRGB (38, 45, 49));
        g.drawRoundedRectangle (key.reduced (0.5f), 2.5f, 0.8f);
    }

    for (auto note = firstNote; note <= lastNote; ++note)
    {
        if (! isBlackKey (note))
            continue;

        const auto x = bounds.getX() + static_cast<float> (whiteIndexForNote (note)) * whiteKeyWidth;
        auto key = juce::Rectangle<float> { x - blackWidth * 0.5f, bounds.getY(), blackWidth, blackHeight };
        const auto active = isActive (note);
        g.setColour (active ? juce::Colour::fromRGB (223, 234, 150) : juce::Colour::fromRGB (31, 36, 39));
        g.fillRoundedRectangle (key, 2.5f);
        g.setColour (juce::Colour::fromRGB (8, 11, 13));
        g.drawRoundedRectangle (key, 2.5f, 0.8f);
    }

    g.setColour (juce::Colour::fromRGB (165, 176, 181));
    g.setFont (juce::FontOptions { 11.0f, juce::Font::bold });
    for (auto octaveNote : { 36, 48, 60, 72, 84 })
    {
        const auto x = bounds.getX() + static_cast<float> (whiteIndexForNote (octaveNote)) * whiteKeyWidth;
        g.drawFittedText (juce::MidiMessage::getMidiNoteName (octaveNote, true, true, 3),
                          juce::Rectangle<int> { juce::roundToInt (x), getHeight() - 16, 36, 14 },
                          juce::Justification::centredLeft,
                          1);
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

    subtitleLabel.setText (juce::String ("VoxChord v") + JucePlugin_VersionString, juce::dontSendNotification);
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
    inputGainSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    outputSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    characterAmountSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);

    configureEditableValueLabel (inputGainValueLabel);
    inputGainValueLabel.onTextChange = [this]
    {
        commitEditableValueLabel (inputGainValueLabel, voxchord::ParameterIDs::inputGainDb, true);
    };
    addAndMakeVisible (inputGainValueLabel);

    configureEditableValueLabel (outputValueLabel);
    outputValueLabel.onTextChange = [this]
    {
        commitEditableValueLabel (outputValueLabel, voxchord::ParameterIDs::outputLevel, true);
    };
    addAndMakeVisible (outputValueLabel);

    configureEditableValueLabel (characterAmountValueLabel);
    characterAmountValueLabel.onTextChange = [this]
    {
        commitEditableValueLabel (characterAmountValueLabel, voxchord::ParameterIDs::character, false);
    };
    addAndMakeVisible (characterAmountValueLabel);

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

    characterModeBox.addItem ("Warm", 1);
    characterModeBox.addItem ("Bright", 2);
    characterModeBox.addItem ("Vowel", 3);
    characterModeBox.addItem ("Digital", 4);
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

    monoOutputButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    monoOutputButton.setColour (juce::ToggleButton::tickColourId, accentColour());
    addAndMakeVisible (monoOutputButton);
    monoOutputAttachment = std::make_unique<ButtonAttachment> (state,
                                                               voxchord::ParameterIDs::monoOutputEnabled,
                                                               monoOutputButton);

    configureStatusLabel (midiNotesLabel, "MIDI: -- | Pitch: --", juce::Justification::centredLeft);
    addAndMakeVisible (midiNotesLabel);

    configureStatusLabel (voiceSlotsLabel, "Slots: V1 -- | V2 -- | V3 -- | V4 -- | V5 off | V6 off | V7 off | V8 off",
                          juce::Justification::centredLeft);
    addAndMakeVisible (voiceSlotsLabel);

    configureStatusLabel (midiStatusLabel, "Last: --", juce::Justification::centredLeft);
    addAndMakeVisible (midiStatusLabel);

    configureStatusLabel (pitchDebugLabel, "Pitch: --", juce::Justification::centred);
    addAndMakeVisible (pitchDebugLabel);

    configureStatusLabel (compactPitchLabel, "Pitch: --", juce::Justification::centredLeft);
    addAndMakeVisible (compactPitchLabel);

    configureStatusLabel (compactLastLabel, "Last: --", juce::Justification::centredLeft);
    addAndMakeVisible (compactLastLabel);

    configureStatusLabel (compactActiveLabel, "Notes: 0", juce::Justification::centredLeft);
    addAndMakeVisible (compactActiveLabel);

    inputMeter.setTitle ("Input");
    outputLeftMeter.setTitle ("Out L");
    outputRightMeter.setTitle ("Out R");
    addAndMakeVisible (inputMeter);
    addAndMakeVisible (outputLeftMeter);
    addAndMakeVisible (outputRightMeter);
    addAndMakeVisible (miniKeyboard);

    panicButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (126, 39, 45));
    panicButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (166, 52, 59));
    panicButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    panicButton.onClick = [this]
    {
        processorRef.panic();
        processorRef.clearClipFlags();
    };
    addAndMakeVisible (panicButton);

    updateEditableValueLabels();
    setSize (860, 540);
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
    auto header = bounds.removeFromTop (82);
    bounds.removeFromTop (14);
    auto bottom = bounds.removeFromBottom (150);
    bounds.removeFromBottom (14);
    auto harmony = bounds;
    auto level = bottom.removeFromRight (246);
    bottom.removeFromRight (14);
    auto midi = bottom;

    g.setColour (panelColour());
    g.fillRoundedRectangle (header.toFloat(), 16.0f);
    g.fillRoundedRectangle (harmony.toFloat(), 18.0f);
    g.fillRoundedRectangle (midi.toFloat(), 18.0f);
    g.fillRoundedRectangle (level.toFloat(), 18.0f);

    g.setColour (accentColour().withAlpha (0.55f));
    g.drawRoundedRectangle (harmony.toFloat(), 18.0f, 1.5f);
    g.drawRoundedRectangle (midi.toFloat(), 18.0f, 1.5f);
    g.drawRoundedRectangle (level.toFloat(), 18.0f, 1.5f);

    if (! characterCardBounds.isEmpty())
    {
        auto characterCard = characterCardBounds.toFloat();
        g.setColour (juce::Colour::fromRGB (21, 27, 31));
        g.fillRoundedRectangle (characterCard, 10.0f);
        g.setColour (accentColour().withAlpha (0.42f));
        g.drawRoundedRectangle (characterCard, 10.0f, 1.2f);
    }

    layoutSectionTitle (g, harmony, "Harmony");
    layoutSectionTitle (g, midi, "MIDI");
    layoutSectionTitle (g, level, "Level");
}

void VoxChordAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (26);

    auto header = bounds.removeFromTop (74);
    auto logoArea = header.removeFromLeft (220).reduced (8, 4);
    auto utilityArea = header.removeFromRight (250).reduced (8, 5);
    auto gainArea = header.reduced (8, 4);

    titleLabel.setBounds (logoArea.removeFromTop (36));
    subtitleLabel.setBounds (logoArea);

    auto gainRow = gainArea.removeFromTop (31);
    inputGainLabel.setBounds (gainRow.removeFromLeft (82));
    inputGainValueLabel.setBounds (gainRow.removeFromRight (76).reduced (2, 3));
    inputGainSlider.setBounds (gainRow.reduced (4, 6));

    gainArea.removeFromTop (4);
    auto outputRow = gainArea.removeFromTop (31);
    outputLabel.setBounds (outputRow.removeFromLeft (82));
    outputValueLabel.setBounds (outputRow.removeFromRight (76).reduced (2, 3));
    outputSlider.setBounds (outputRow.reduced (4, 6));

    auto utilityTop = utilityArea.removeFromTop (30);
    auto inputRow = utilityTop.removeFromLeft (132);
    inputSourceLabel.setBounds (inputRow.removeFromLeft (42));
    inputSourceBox.setBounds (inputRow.reduced (0, 1));
    leadTuneButton.setBounds (utilityTop.removeFromLeft (92).reduced (4, 1));

    utilityArea.removeFromTop (4);
    monoOutputButton.setBounds (utilityArea.removeFromLeft (108).reduced (4, 2));
    panicButton.setBounds (utilityArea.reduced (4, 1));

    bounds.removeFromTop (18);

    auto bottom = bounds.removeFromBottom (134);
    bounds.removeFromBottom (18);

    auto harmony = bounds.reduced (12);
    harmony.removeFromTop (22);
    auto controlsTop = harmony.removeFromTop (168);
    const auto characterWidth = juce::jlimit (220, 280, controlsTop.getWidth() / 3);
    const auto knobWidth = (controlsTop.getWidth() - characterWidth) / 4;

    layoutSlider (voiceCountSlider, voiceCountLabel, controlsTop.removeFromLeft (knobWidth).reduced (5));
    layoutSlider (glideSlider, glideLabel, controlsTop.removeFromLeft (knobWidth).reduced (5));

    auto characterOuter = controlsTop.removeFromLeft (characterWidth).reduced (5, 0);
    characterCardBounds = characterOuter;
    auto characterGroup = characterOuter.reduced (14, 10);
    characterTypeLabel.setBounds (characterGroup.removeFromTop (22));
    characterModeBox.setBounds (characterGroup.removeFromTop (32));
    characterGroup.removeFromTop (5);
    characterLabel.setBounds (characterGroup.removeFromTop (18));
    characterAmountValueLabel.setBounds (characterGroup.removeFromBottom (23).withSizeKeepingCentre (72, 23));
    characterAmountSlider.setBounds (characterGroup.reduced (2, 0));

    layoutSlider (spreadSlider, spreadLabel, controlsTop.removeFromLeft (knobWidth).reduced (5));
    layoutSlider (dryWetSlider, dryWetLabel, controlsTop.reduced (5));

    auto level = bottom.removeFromRight (230).reduced (12);
    bottom.removeFromRight (14);
    auto midi = bottom.reduced (12);

    level.removeFromTop (22);
    auto meterRow = level;
    inputMeter.setBounds (meterRow.removeFromLeft (66));
    meterRow.removeFromLeft (10);
    outputLeftMeter.setBounds (meterRow.removeFromLeft (66));
    meterRow.removeFromLeft (10);
    outputRightMeter.setBounds (meterRow.removeFromLeft (66));

    midi.removeFromTop (22);
    auto midiStatusRow = midi.removeFromTop (26);
    compactPitchLabel.setBounds (midiStatusRow.removeFromLeft (170).reduced (4, 0));
    compactLastLabel.setBounds (midiStatusRow.removeFromLeft (170).reduced (4, 0));
    compactActiveLabel.setBounds (midiStatusRow.removeFromLeft (132).reduced (4, 0));
    midiNotesLabel.setBounds (midi.removeFromTop (22));
    miniKeyboard.setBounds (midi.removeFromTop (54));
    auto debugRow = midi.removeFromTop (32);
    pitchDebugLabel.setBounds (debugRow.removeFromRight (180).reduced (4));
    midiStatusLabel.setBounds (debugRow.removeFromRight (180).reduced (4));
    voiceSlotsLabel.setBounds (debugRow.reduced (4));
}

void VoxChordAudioProcessorEditor::timerCallback()
{
    updateMidiState();
    updateMeters();
    updatePitchDebug();
    updateEditableValueLabels();
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

void VoxChordAudioProcessorEditor::layoutSectionTitle (juce::Graphics& g,
                                                       juce::Rectangle<int> bounds,
                                                       const juce::String& title)
{
    g.setColour (juce::Colour::fromRGB (190, 205, 205));
    g.setFont (juce::FontOptions { 13.0f, juce::Font::bold });
    g.drawFittedText (title, bounds.reduced (14).removeFromTop (20), juce::Justification::centredLeft, 1);
}

void VoxChordAudioProcessorEditor::configureEditableValueLabel (juce::Label& label)
{
    label.setEditable (true, true, false);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colours::white);
    label.setColour (juce::Label::backgroundColourId, juce::Colour::fromRGB (18, 23, 27));
    label.setColour (juce::Label::outlineColourId, accentColour().withAlpha (0.45f));
    label.setColour (juce::Label::textWhenEditingColourId, juce::Colours::white);
    label.setColour (juce::Label::backgroundWhenEditingColourId, juce::Colour::fromRGB (18, 23, 27));
    label.setFont (juce::FontOptions { 12.5f, juce::Font::bold });
}

void VoxChordAudioProcessorEditor::updateEditableValueLabels()
{
    if (inputGainValueLabel.isBeingEdited()
        || outputValueLabel.isBeingEdited()
        || characterAmountValueLabel.isBeingEdited())
    {
        return;
    }

    const juce::ScopedValueSetter<bool> updateGuard (isUpdatingEditableValueLabels, true);

    updateEditableValueLabel (inputGainValueLabel, voxchord::ParameterIDs::inputGainDb, true);
    updateEditableValueLabel (outputValueLabel, voxchord::ParameterIDs::outputLevel, true);
    updateEditableValueLabel (characterAmountValueLabel, voxchord::ParameterIDs::character, false);
}

void VoxChordAudioProcessorEditor::updateEditableValueLabel (juce::Label& label,
                                                             const juce::String& parameterId,
                                                             bool isDecibels)
{
    label.setText (formatEditableValue (parameterId, isDecibels), juce::dontSendNotification);
}

void VoxChordAudioProcessorEditor::commitEditableValueLabel (juce::Label& label,
                                                             const juce::String& parameterId,
                                                             bool isDecibels)
{
    if (isUpdatingEditableValueLabels)
        return;

    float value = 0.0f;

    if (! parseEditableValue (label.getText(), isDecibels, value))
    {
        updateEditableValueLabel (label, parameterId, isDecibels);
        return;
    }

    auto& state = processorRef.getValueTreeState();

    if (auto* parameter = state.getParameter (parameterId))
    {
        value = juce::jlimit (parameter->getNormalisableRange().start,
                              parameter->getNormalisableRange().end,
                              value);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        parameter->endChangeGesture();
    }

    updateEditableValueLabel (label, parameterId, isDecibels);
}

juce::String VoxChordAudioProcessorEditor::formatEditableValue (const juce::String& parameterId,
                                                                bool isDecibels) const
{
    if (auto* value = processorRef.getValueTreeState().getRawParameterValue (parameterId))
        return isDecibels ? formatDecibelValue (value->load()) : formatPercentValue (value->load());

    return isDecibels ? formatDecibelValue (0.0f) : formatPercentValue (0.0f);
}

bool VoxChordAudioProcessorEditor::parseEditableValue (const juce::String& text,
                                                       bool isDecibels,
                                                       float& value) const
{
    auto cleaned = text.trim().toLowerCase();
    cleaned = cleaned.replace ("db", "");
    cleaned = cleaned.replace ("%", "");
    cleaned = cleaned.removeCharacters (" \t\r\n");

    if (cleaned.isEmpty())
        return false;

    const auto raw = cleaned.toStdString();
    char* end = nullptr;
    errno = 0;
    const auto parsed = std::strtof (raw.c_str(), &end);

    if (end == raw.c_str() || *end != '\0' || errno == ERANGE || ! std::isfinite (parsed))
        return false;

    value = isDecibels ? parsed : parsed / 100.0f;
    return true;
}

juce::String VoxChordAudioProcessorEditor::formatDecibelValue (float value)
{
    return juce::String (value > 0.0f ? "+" : "") + juce::String (value, 1) + " dB";
}

juce::String VoxChordAudioProcessorEditor::formatPercentValue (float value)
{
    return juce::String (juce::roundToInt (juce::jlimit (0.0f, 1.0f, value) * 100.0f)) + "%";
}

void VoxChordAudioProcessorEditor::updateMidiState()
{
    const auto notes = processorRef.getActiveMidiNotes();
    const auto voiceLimit = processorRef.getCurrentVoiceLimit();
    auto text = juce::String ("Notes: ");
    auto slots = juce::String ("Slots: ");
    auto anyNotes = false;
    auto activeCount = 0;

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
        ++activeCount;

        if (index > 0)
            slots += " | ";

        slots += "V" + juce::String (index + 1) + " " + noteName;
        anyNotes = true;
    }

    if (! anyNotes)
        text += "--";

    midiNotesLabel.setText (text, juce::dontSendNotification);
    voiceSlotsLabel.setText (slots, juce::dontSendNotification);
    miniKeyboard.setActiveNotes (notes);
    compactActiveLabel.setText ("Notes: " + juce::String (activeCount), juce::dontSendNotification);

    const auto activity = processorRef.getMidiActivitySnapshot();

    if (activity.counter != lastSeenMidiActivityCounter)
    {
        lastSeenMidiActivityCounter = activity.counter;
        const auto lastText = "Last: " + midiActivityToString (activity.activity);
        midiStatusLabel.setText (lastText, juce::dontSendNotification);
        compactLastLabel.setText (lastText, juce::dontSendNotification);
    }
}

void VoxChordAudioProcessorEditor::updateMeters()
{
    const auto& meters = processorRef.getLevelMeterState();
    const auto mono = processorRef.isMonoOutputEnabledForUi();

    inputMeter.setLevel (meters.getInputPeak(), meters.getInputClipped());
    outputLeftMeter.setTitle (mono ? "Mono" : "Out L");
    outputRightMeter.setTitle (mono ? "Mono" : "Out R");
    outputLeftMeter.setLevel (meters.getOutputLeftPeak(), meters.getOutputLeftClipped());
    outputRightMeter.setLevel (meters.getOutputRightPeak(), meters.getOutputRightClipped());
}

void VoxChordAudioProcessorEditor::updatePitchDebug()
{
    const auto pitchState = processorRef.getPitchState();
    const auto pitchShifterSelfTestSummary = processorRef.getPitchShifterSelfTestSummary();
    const auto pitchText = formatPitchDebug (pitchState.correctionInputPitchHz);

    pitchDebugLabel.setText (pitchText, juce::dontSendNotification);
    compactPitchLabel.setText (pitchText, juce::dontSendNotification);
    subtitleLabel.setText (formatDetailedPitchDebug (pitchState, pitchShifterSelfTestSummary),
                           juce::dontSendNotification);
}
