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

#if JUCE_DEBUG
    juce::String formatD1LowPitchDiagnostics (const voxchord::PitchState& state)
    {
        const auto noteName = state.representativeVoiceMidiNote >= 0
                                  ? juce::MidiMessage::getMidiNoteName (state.representativeVoiceMidiNote, true, true, 3)
                                  : juce::String ("--");
        const auto windowPitchText = state.windowPitchHz > 0.0f ? juce::String (state.windowPitchHz, 1) + "Hz" : juce::String ("--");
        const auto wetHzText = state.wetZeroCrossingHz > 0.0f ? juce::String (state.wetZeroCrossingHz, 1) + "Hz" : juce::String ("--");

        // Front-load the three key shifter indicators (Ratio / Per/Win / WetHz)
        // so they stay visible even if the line is width-clipped; secondary
        // fields (Note / Grain / Win / Clamp#) follow.
        juce::ignoreUnused (windowPitchText);

        return "D1  Ratio " + juce::String (state.representativePitchRatioRaw, 3)
             + juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92")) + juce::String (state.representativePitchRatioClamped, 3)
             + "  |  Per/Win " + juce::String (state.outputPeriodToWindowRatio, 2)
             + "  |  Wet " + wetHzText
             + " (" + juce::String (state.wetZeroCrossingCentsDeviation, 0) + "c)"
             + "  |  " + noteName
             + "  Grain " + juce::String (state.representativeGrainWindowSamples) + "smp"
             + "  Clamp#" + juce::String (state.ratioClampHitCount);
    }

    juce::String formatLeadRetuneDiagnostics (const voxchord::PitchState& state, float tune)
    {
        // Retune (Lead Tune) diagnostics: effective snap time, the un-smoothed
        // tune pitch and its divergence from the display pitch, and the lead's
        // chromatic target note (directions/0708_9.md item 4).
        const auto settleMs = 1.0f + 199.0f * (1.0f - tune) * (1.0f - tune);
        auto targetNote = juce::String ("--");
        auto divergence = juce::String ("--");

        if (state.tunePitchHz > 0.0f)
        {
            const auto midiFloat = 69.0f + 12.0f * std::log2 (state.tunePitchHz / 440.0f);
            targetNote = juce::MidiMessage::getMidiNoteName (juce::roundToInt (midiFloat), true, true, 3);

            if (state.displayStablePitchHz > 0.0f)
                divergence = juce::String (1200.0f * std::log2 (state.tunePitchHz / state.displayStablePitchHz), 0) + "c";
        }

        return "Retune " + juce::String (settleMs, 0) + "ms  Tune "
             + (state.tunePitchHz > 0.0f ? juce::String (state.tunePitchHz, 1) + "Hz" : juce::String ("--"))
             + " (" + divergence + " vs disp)  Lead" + juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92")) + targetNote;
    }
#endif

    juce::String formatDetailedPitchDebug (const voxchord::PitchState& state,
                                           const voxchord::PitchShifterSelfTestSummary& selfTestSummary)
    {
#if JUCE_DEBUG
        auto text = juce::String();

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

        juce::ignoreUnused (state.characterModeRaw,
                            state.characterModeSanitized,
                            state.characterAmountRaw,
                            state.characterAmountSmoothed,
                            state.characterDeltaRms,
                            state.characterDeltaPeak,
                            state.characterDeltaRatioDb);

        return text.isNotEmpty() ? text : juce::String ("VoxChord v") + JucePlugin_VersionString;
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
    const auto labelArea = bounds.removeFromTop (20.0f);
    const auto valueArea = bounds.removeFromBottom (18.0f);
    const auto meterArea = bounds.reduced (5.0f, 3.0f);
    const auto db = juce::Decibels::gainToDecibels (peak, -60.0f);
    const auto normalized = juce::jmap (juce::jlimit (-60.0f, 0.0f, db), -60.0f, 0.0f, 0.0f, 1.0f);
    const auto fillHeight = meterArea.getHeight() * normalized;

    g.setColour (juce::Colour::fromRGB (200, 211, 214));
    g.setFont (juce::FontOptions { 13.5f, juce::Font::bold });
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

    g.setColour (juce::Colour::fromRGB (135, 155, 158));
    g.setFont (juce::FontOptions { 11.5f, juce::Font::bold });
    g.drawFittedText (formatPeak (peak), valueArea.toNearestInt(), juce::Justification::centred, 1);
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
    voiceCountSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    glideSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    spreadSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    dryWetSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    inputGainSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    outputSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    characterAmountSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);

    configureEditableValueLabel (inputGainValueLabel);
    inputGainValueLabel.onTextChange = [this]
    {
        commitEditableValueLabel (inputGainValueLabel, voxchord::ParameterIDs::inputGainDb, EditableValueFormat::decibels);
    };
    addAndMakeVisible (inputGainValueLabel);

    configureEditableValueLabel (outputValueLabel);
    outputValueLabel.onTextChange = [this]
    {
        commitEditableValueLabel (outputValueLabel, voxchord::ParameterIDs::outputLevel, EditableValueFormat::decibels);
    };
    addAndMakeVisible (outputValueLabel);

    configureEditableValueLabel (voiceCountValueLabel);
    voiceCountValueLabel.onTextChange = [this]
    {
        commitEditableValueLabel (voiceCountValueLabel, voxchord::ParameterIDs::voiceCount, EditableValueFormat::integer);
    };
    addAndMakeVisible (voiceCountValueLabel);

    configureEditableValueLabel (glideValueLabel);
    glideValueLabel.onTextChange = [this]
    {
        commitEditableValueLabel (glideValueLabel, voxchord::ParameterIDs::glide, EditableValueFormat::percent);
    };
    addAndMakeVisible (glideValueLabel);

    configureEditableValueLabel (characterAmountValueLabel);
    characterAmountValueLabel.onTextChange = [this]
    {
        commitEditableValueLabel (characterAmountValueLabel, voxchord::ParameterIDs::character, EditableValueFormat::percent);
    };
    addAndMakeVisible (characterAmountValueLabel);

    configureEditableValueLabel (spreadValueLabel);
    spreadValueLabel.onTextChange = [this]
    {
        commitEditableValueLabel (spreadValueLabel, voxchord::ParameterIDs::spread, EditableValueFormat::percent);
    };
    addAndMakeVisible (spreadValueLabel);

    configureEditableValueLabel (dryWetValueLabel);
    dryWetValueLabel.onTextChange = [this]
    {
        commitEditableValueLabel (dryWetValueLabel, voxchord::ParameterIDs::dryWet, EditableValueFormat::percent);
    };
    addAndMakeVisible (dryWetValueLabel);

    voiceCountAttachment = std::make_unique<SliderAttachment> (state, voxchord::ParameterIDs::voiceCount, voiceCountSlider);
    glideAttachment = std::make_unique<SliderAttachment> (state, voxchord::ParameterIDs::glide, glideSlider);
    characterAmountAttachment = std::make_unique<SliderAttachment> (state,
                                                                    voxchord::ParameterIDs::character,
                                                                    characterAmountSlider);
    spreadAttachment = std::make_unique<SliderAttachment> (state, voxchord::ParameterIDs::spread, spreadSlider);
    dryWetAttachment = std::make_unique<SliderAttachment> (state, voxchord::ParameterIDs::dryWet, dryWetSlider);
    inputGainAttachment = std::make_unique<SliderAttachment> (state, voxchord::ParameterIDs::inputGainDb, inputGainSlider);
    outputAttachment = std::make_unique<SliderAttachment> (state, voxchord::ParameterIDs::outputLevel, outputSlider);

    characterTypeLabel.setText ("Type", juce::dontSendNotification);
    characterTypeLabel.setJustificationType (juce::Justification::centred);
    characterTypeLabel.setColour (juce::Label::textColourId, juce::Colour::fromRGB (210, 220, 222));
    characterTypeLabel.setFont (juce::FontOptions { 15.0f, juce::Font::bold });
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

    // Engine selector: on = High Quality (PSOLA), off = Classic window shifter.
    psolaButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    psolaButton.setColour (juce::ToggleButton::tickColourId, accentColour());
    addAndMakeVisible (psolaButton);
    psolaAttachment = std::make_unique<ButtonAttachment> (state,
                                                          voxchord::ParameterIDs::psolaEnabled,
                                                          psolaButton);

    configureStatusLabel (midiNotesLabel, "MIDI: -- | Pitch: --", juce::Justification::centredLeft);
    addAndMakeVisible (midiNotesLabel);

    configureStatusLabel (voiceSlotsLabel, "Slots: V1 -- | V2 -- | V3 -- | V4 -- | V5 off | V6 off | V7 off | V8 off",
                          juce::Justification::centredLeft);
    addAndMakeVisible (voiceSlotsLabel);
    voiceSlotsLabel.setVisible (false);

    configureStatusLabel (midiStatusLabel, "Last: --", juce::Justification::centredLeft);
    midiStatusLabel.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (midiStatusLabel);

    configureStatusLabel (pitchDebugLabel, "D1: --", juce::Justification::centredLeft);
    pitchDebugLabel.setFont (juce::FontOptions { 13.5f, juce::Font::bold });
    addAndMakeVisible (pitchDebugLabel);
#if JUCE_DEBUG
    pitchDebugLabel.setVisible (true);
#else
    pitchDebugLabel.setVisible (false);
#endif

    configureStatusLabel (compactPitchLabel, "Pitch: --", juce::Justification::centredLeft);
    addAndMakeVisible (compactPitchLabel);
    compactPitchLabel.setVisible (false);

    configureStatusLabel (compactLastLabel, "Last: --", juce::Justification::centredLeft);
    addAndMakeVisible (compactLastLabel);
    compactLastLabel.setVisible (false);

    inputMeter.setTitle ("Input");
    outputLeftMeter.setTitle ("OutL");
    outputRightMeter.setTitle ("OutR");
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
    auto bottom = bounds.removeFromBottom (174);
    bounds.removeFromBottom (8);
    auto harmony = bounds;
    auto level = bottom.removeFromRight (266);
    bottom.removeFromRight (4);
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

        auto titleArea = characterCardBounds.reduced (14, 8).removeFromTop (20);
        g.setColour (juce::Colour::fromRGB (220, 232, 231));
        g.setFont (juce::FontOptions { 15.0f, juce::Font::bold });
        g.drawFittedText ("Character", titleArea, juce::Justification::centred, 1);
    }

    layoutSectionTitle (g, harmony, "Harmony");
    layoutSectionTitle (g, midi, "MIDI");
    layoutSectionTitle (g, level, "Level");
}

void VoxChordAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (26);

    auto header = bounds.removeFromTop (74);
    // Utility column widened by 20 px (taken from the logo column, not the gain
    // column) so the "High Quality" toggle label fits (directions/0709_1.md).
    auto logoArea = header.removeFromLeft (200).reduced (8, 4);
    auto utilityArea = header.removeFromRight (316).reduced (8, 5);
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
    auto inputRow = utilityTop.removeFromRight (132);
    inputSourceLabel.setBounds (inputRow.removeFromLeft (42));
    inputSourceBox.setBounds (inputRow.reduced (0, 1));
    leadTuneButton.setBounds (utilityTop.reduced (4, 1));

    utilityArea.removeFromTop (4);
    auto panicArea = utilityArea.removeFromRight (86);
    panicButton.setBounds (panicArea.reduced (0, 1));
    auto psolaArea = utilityArea.removeFromRight (116);
    psolaButton.setBounds (psolaArea.reduced (4, 2));
    monoOutputButton.setBounds (utilityArea.reduced (4, 2));

    bounds.removeFromTop (18);

    auto bottom = bounds.removeFromBottom (158);
    bounds.removeFromBottom (12);

    auto harmony = bounds.reduced (12);
    harmony.removeFromTop (22);
    auto controlsTop = harmony.removeFromTop (juce::jmin (176, harmony.getHeight()));
    const auto characterWidth = juce::jlimit (250, 320, controlsTop.getWidth() / 3);
    const auto knobWidth = (controlsTop.getWidth() - characterWidth) / 4;

    layoutEditableSlider (voiceCountSlider, voiceCountLabel, voiceCountValueLabel, controlsTop.removeFromLeft (knobWidth).reduced (5));
    layoutEditableSlider (glideSlider, glideLabel, glideValueLabel, controlsTop.removeFromLeft (knobWidth).reduced (5));

    auto characterOuter = controlsTop.removeFromLeft (characterWidth).reduced (5, 0);
    characterCardBounds = characterOuter;
    auto characterGroup = characterOuter.reduced (14, 8);
    characterGroup.removeFromTop (18);
    auto typeRow = characterGroup.removeFromTop (22);
    characterTypeLabel.setBounds (typeRow.removeFromLeft (48));
    characterModeBox.setBounds (typeRow.withSizeKeepingCentre (juce::jmin (typeRow.getWidth(), 112), 22));
    characterGroup.removeFromTop (1);
    characterLabel.setBounds (characterGroup.removeFromTop (16));
    characterAmountValueLabel.setBounds (characterGroup.removeFromBottom (20).withSizeKeepingCentre (76, 20));
    characterAmountSlider.setBounds (characterGroup);

    layoutEditableSlider (spreadSlider, spreadLabel, spreadValueLabel, controlsTop.removeFromLeft (knobWidth).reduced (5));
    layoutEditableSlider (dryWetSlider, dryWetLabel, dryWetValueLabel, controlsTop.reduced (5));

    auto level = bottom.removeFromRight (250).reduced (12);
    bottom.removeFromRight (4);
    auto midi = bottom.reduced (12);

    level.removeFromTop (22);
    auto meterRow = level;
    constexpr auto inputMeterWidth = 54;
    constexpr auto meterGap = 18;
    constexpr auto outputMeterPairWidth = 112;
    const auto meterGroupWidth = inputMeterWidth + meterGap + outputMeterPairWidth;
    meterRow.removeFromLeft (juce::jmax (0, (meterRow.getWidth() - meterGroupWidth) / 2));
    inputMeter.setBounds (meterRow.removeFromLeft (inputMeterWidth));
    meterRow.removeFromLeft (meterGap);
    auto outputMeters = meterRow.removeFromLeft (outputMeterPairWidth);
    outputLeftMeter.setBounds (outputMeters.removeFromLeft (56));
    outputRightMeter.setBounds (outputMeters);

    midi.removeFromTop (22);
#if JUCE_DEBUG
    // Show the D1 readout in the previously-empty band above the keyboard so it
    // is actually legible (the old 16px bottom slot buried it). Debug only.
    pitchDebugLabel.setBounds (midi.removeFromTop (26).reduced (4, 3));
#else
    midi.removeFromTop (26);
#endif
    midiNotesLabel.setBounds (midi.removeFromTop (22));
    miniKeyboard.setBounds (midi.removeFromTop (54));
    auto debugRow = midi.removeFromTop (32);
    midiStatusLabel.setBounds (debugRow.removeFromTop (16).reduced (4, 1));
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
    label.setFont (juce::FontOptions { 15.0f, juce::Font::bold });
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
    label.setFont (juce::FontOptions { 13.5f, juce::Font::bold });
    addAndMakeVisible (label);
}

void VoxChordAudioProcessorEditor::layoutEditableSlider (juce::Slider& slider,
                                                         juce::Label& label,
                                                         juce::Label& valueLabel,
                                                         juce::Rectangle<int> bounds)
{
    label.setBounds (bounds.removeFromTop (22));
    valueLabel.setBounds (bounds.removeFromBottom (22).withSizeKeepingCentre (76, 22));
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
    g.setFont (juce::FontOptions { 14.5f, juce::Font::bold });
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
    label.setFont (juce::FontOptions { 13.5f, juce::Font::bold });
}

void VoxChordAudioProcessorEditor::updateEditableValueLabels()
{
    if (inputGainValueLabel.isBeingEdited()
        || outputValueLabel.isBeingEdited()
        || voiceCountValueLabel.isBeingEdited()
        || glideValueLabel.isBeingEdited()
        || spreadValueLabel.isBeingEdited()
        || dryWetValueLabel.isBeingEdited()
        || characterAmountValueLabel.isBeingEdited())
    {
        return;
    }

    const juce::ScopedValueSetter<bool> updateGuard (isUpdatingEditableValueLabels, true);

    updateEditableValueLabel (inputGainValueLabel, voxchord::ParameterIDs::inputGainDb, EditableValueFormat::decibels);
    updateEditableValueLabel (outputValueLabel, voxchord::ParameterIDs::outputLevel, EditableValueFormat::decibels);
    updateEditableValueLabel (voiceCountValueLabel, voxchord::ParameterIDs::voiceCount, EditableValueFormat::integer);
    updateEditableValueLabel (glideValueLabel, voxchord::ParameterIDs::glide, EditableValueFormat::percent);
    updateEditableValueLabel (characterAmountValueLabel, voxchord::ParameterIDs::character, EditableValueFormat::percent);
    updateEditableValueLabel (spreadValueLabel, voxchord::ParameterIDs::spread, EditableValueFormat::percent);
    updateEditableValueLabel (dryWetValueLabel, voxchord::ParameterIDs::dryWet, EditableValueFormat::percent);
}

void VoxChordAudioProcessorEditor::updateEditableValueLabel (juce::Label& label,
                                                             const juce::String& parameterId,
                                                             EditableValueFormat format)
{
    label.setText (formatEditableValue (parameterId, format), juce::dontSendNotification);
}

void VoxChordAudioProcessorEditor::commitEditableValueLabel (juce::Label& label,
                                                             const juce::String& parameterId,
                                                             EditableValueFormat format)
{
    if (isUpdatingEditableValueLabels)
        return;

    float value = 0.0f;

    if (! parseEditableValue (label.getText(), format, value))
    {
        updateEditableValueLabel (label, parameterId, format);
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

    updateEditableValueLabel (label, parameterId, format);
}

juce::String VoxChordAudioProcessorEditor::formatEditableValue (const juce::String& parameterId,
                                                                EditableValueFormat format) const
{
    if (auto* value = processorRef.getValueTreeState().getRawParameterValue (parameterId))
    {
        switch (format)
        {
            case EditableValueFormat::decibels: return formatDecibelValue (value->load());
            case EditableValueFormat::percent:  return formatPercentValue (value->load());
            case EditableValueFormat::integer:  return formatIntegerValue (value->load());
        }
    }

    switch (format)
    {
        case EditableValueFormat::decibels: return formatDecibelValue (0.0f);
        case EditableValueFormat::percent:  return formatPercentValue (0.0f);
        case EditableValueFormat::integer:  return formatIntegerValue (0.0f);
    }

    return {};
}

bool VoxChordAudioProcessorEditor::parseEditableValue (const juce::String& text,
                                                       EditableValueFormat format,
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

    switch (format)
    {
        case EditableValueFormat::decibels:
            value = parsed;
            break;
        case EditableValueFormat::percent:
            value = parsed / 100.0f;
            break;
        case EditableValueFormat::integer:
            value = static_cast<float> (juce::roundToInt (parsed));
            break;
    }

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

juce::String VoxChordAudioProcessorEditor::formatIntegerValue (float value)
{
    return juce::String (juce::roundToInt (value));
}

void VoxChordAudioProcessorEditor::updateMidiState()
{
    const auto notes = processorRef.getActiveMidiNotes();
    const auto voiceLimit = processorRef.getCurrentVoiceLimit();
    auto text = juce::String ("Notes: ");
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

    midiNotesLabel.setText (text, juce::dontSendNotification);
    voiceSlotsLabel.setText (slots, juce::dontSendNotification);
    miniKeyboard.setActiveNotes (notes);

    const auto midiInputDebug = processorRef.getMidiInputDebugSnapshot();
    midiStatusLabel.setText ("MIDI In: blocks "
                                 + juce::String (midiInputDebug.processBlockCounter)
                                 + " | last "
                                 + juce::String (midiInputDebug.lastBlockEventCount)
                                 + " | total "
                                 + juce::String (midiInputDebug.totalEventCounter)
                                 + " | nonempty "
                                 + juce::String (midiInputDebug.nonEmptyBlockCounter),
                             juce::dontSendNotification);

    const auto activity = processorRef.getMidiActivitySnapshot();

    if (activity.counter != lastSeenMidiActivityCounter)
    {
        lastSeenMidiActivityCounter = activity.counter;
        const auto lastText = "Last: " + midiActivityToString (activity.activity);
        compactLastLabel.setText (lastText, juce::dontSendNotification);
    }
}

void VoxChordAudioProcessorEditor::updateMeters()
{
    const auto& meters = processorRef.getLevelMeterState();
    const auto mono = processorRef.isMonoOutputEnabledForUi();

    inputMeter.setLevel (meters.getInputPeak(), meters.getInputClipped());
    outputLeftMeter.setTitle (mono ? "Mono" : "OutL");
    outputRightMeter.setTitle (mono ? "Mono" : "OutR");
    outputLeftMeter.setLevel (meters.getOutputLeftPeak(), meters.getOutputLeftClipped());
    outputRightMeter.setLevel (meters.getOutputRightPeak(), meters.getOutputRightClipped());
}

void VoxChordAudioProcessorEditor::updatePitchDebug()
{
#if JUCE_DEBUG
    subtitleLabel.setText (juce::String ("VoxChord v") + JucePlugin_VersionString + " | Build: lead-retune-001",
                           juce::dontSendNotification);

    const auto pitchState = processorRef.getPitchState();
    auto tuneValue = 1.0f;

    if (auto* raw = processorRef.getValueTreeState().getRawParameterValue (voxchord::ParameterIDs::tune))
        tuneValue = raw->load (std::memory_order_relaxed);

    pitchDebugLabel.setText (juce::String (processorRef.isPsolaEnabledForUi() ? "[PSOLA] " : "[WIN] ")
                                 + formatD1LowPitchDiagnostics (pitchState)
                                 + "  ||  " + formatLeadRetuneDiagnostics (pitchState, tuneValue),
                             juce::dontSendNotification);
#else
    subtitleLabel.setText (juce::String ("VoxChord v") + JucePlugin_VersionString,
                           juce::dontSendNotification);
#endif
}
