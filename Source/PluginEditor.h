#pragma once

#include <memory>

#include "PluginProcessor.h"

class VoxChordAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit VoxChordAudioProcessorEditor (VoxChordAudioProcessor&);
    ~VoxChordAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void configureSlider (juce::Slider& slider, juce::Label& label, const juce::String& text);
    void configureCompactSlider (juce::Slider& slider, juce::Label& label, const juce::String& text);
    void layoutSlider (juce::Slider& slider, juce::Label& label, juce::Rectangle<int> bounds);
    void layoutComboBox (juce::ComboBox& comboBox, juce::Label& label, juce::Rectangle<int> bounds);
    void layoutCompactSlider (juce::Slider& slider, juce::Label& label, juce::Rectangle<int> bounds);
    void layoutSectionTitle (juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title);
    void updateMidiState();
    void updateMeters();
    void updatePitchDebug();
    void configureEditableValueLabel (juce::Label& label);
    void updateEditableValueLabels();
    void updateEditableValueLabel (juce::Label& label, const juce::String& parameterId, bool isDecibels);
    void commitEditableValueLabel (juce::Label& label, const juce::String& parameterId, bool isDecibels);
    juce::String formatEditableValue (const juce::String& parameterId, bool isDecibels) const;
    bool parseEditableValue (const juce::String& text, bool isDecibels, float& value) const;
    static juce::String formatDecibelValue (float value);
    static juce::String formatPercentValue (float value);

    class VerticalMeter final : public juce::Component
    {
    public:
        void setTitle (const juce::String& newTitle);
        void setLevel (float newPeak, bool isClipped);
        void paint (juce::Graphics& g) override;

    private:
        juce::String title;
        float peak = 0.0f;
        bool clipped = false;
    };

    class MiniKeyboard final : public juce::Component
    {
    public:
        void setActiveNotes (const voxchord::MidiVoiceState::NoteSnapshot& newNotes);
        void paint (juce::Graphics& g) override;

    private:
        static constexpr int firstNote = 36;
        static constexpr int lastNote = 84;

        static bool isBlackKey (int midiNote) noexcept;
        bool isActive (int midiNote) const noexcept;

        voxchord::MidiVoiceState::NoteSnapshot notes {};
    };

    VoxChordAudioProcessor& processorRef;

    juce::Slider voiceCountSlider;
    juce::Slider glideSlider;
    juce::Slider characterAmountSlider;
    juce::Slider spreadSlider;
    juce::Slider dryWetSlider;
    juce::Slider inputGainSlider;
    juce::Slider outputSlider;

    juce::Label voiceCountLabel;
    juce::Label glideLabel;
    juce::Label characterTypeLabel;
    juce::Label characterLabel;
    juce::Label spreadLabel;
    juce::Label dryWetLabel;
    juce::Label inputGainLabel;
    juce::Label outputLabel;
    juce::Label inputGainValueLabel;
    juce::Label outputValueLabel;
    juce::Label characterAmountValueLabel;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label midiNotesLabel;
    juce::Label voiceSlotsLabel;
    juce::Label midiStatusLabel;
    juce::Label pitchDebugLabel;
    juce::Label compactPitchLabel;
    juce::Label compactLastLabel;
    juce::Label compactActiveLabel;
    VerticalMeter inputMeter;
    VerticalMeter outputLeftMeter;
    VerticalMeter outputRightMeter;
    MiniKeyboard miniKeyboard;
    juce::Label inputSourceLabel;
    juce::ComboBox inputSourceBox;
    juce::ComboBox characterModeBox;
    juce::ToggleButton leadTuneButton { "Lead" };
    juce::ToggleButton monoOutputButton { "Mono" };
    juce::TextButton panicButton { "PANIC" };
    juce::Rectangle<int> characterCardBounds;

    std::unique_ptr<SliderAttachment> voiceCountAttachment;
    std::unique_ptr<SliderAttachment> glideAttachment;
    std::unique_ptr<SliderAttachment> characterAmountAttachment;
    std::unique_ptr<SliderAttachment> spreadAttachment;
    std::unique_ptr<SliderAttachment> dryWetAttachment;
    std::unique_ptr<SliderAttachment> inputGainAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;
    std::unique_ptr<ComboBoxAttachment> inputSourceAttachment;
    std::unique_ptr<ComboBoxAttachment> characterModeAttachment;
    std::unique_ptr<ButtonAttachment> leadTuneAttachment;
    std::unique_ptr<ButtonAttachment> monoOutputAttachment;

    uint32_t lastSeenMidiActivityCounter = 0;
    bool isUpdatingEditableValueLabels = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxChordAudioProcessorEditor)
};
