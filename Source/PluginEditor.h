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
    void updateMidiState();
    void updateMeters();
    void updatePitchDebug();

    class MeterBar final : public juce::Component
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

    VoxChordAudioProcessor& processorRef;

    juce::Slider voiceCountSlider;
    juce::Slider glideSlider;
    juce::Slider spreadSlider;
    juce::Slider dryWetSlider;
    juce::Slider inputGainSlider;
    juce::Slider outputSlider;

    juce::Label voiceCountLabel;
    juce::Label glideLabel;
    juce::Label characterLabel;
    juce::Label spreadLabel;
    juce::Label dryWetLabel;
    juce::Label inputGainLabel;
    juce::Label outputLabel;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label midiNotesLabel;
    juce::Label voiceSlotsLabel;
    juce::Label midiStatusLabel;
    juce::Label pitchDebugLabel;
    MeterBar inputMeter;
    MeterBar outputMeter;
    juce::Label inputSourceLabel;
    juce::ComboBox inputSourceBox;
    juce::ComboBox characterModeBox;
    juce::ToggleButton leadTuneButton { "Lead Tune" };
    juce::TextButton panicButton { "PANIC" };

    std::unique_ptr<SliderAttachment> voiceCountAttachment;
    std::unique_ptr<SliderAttachment> glideAttachment;
    std::unique_ptr<SliderAttachment> spreadAttachment;
    std::unique_ptr<SliderAttachment> dryWetAttachment;
    std::unique_ptr<SliderAttachment> inputGainAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;
    std::unique_ptr<ComboBoxAttachment> inputSourceAttachment;
    std::unique_ptr<ComboBoxAttachment> characterModeAttachment;
    std::unique_ptr<ButtonAttachment> leadTuneAttachment;

    uint32_t lastSeenMidiActivityCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxChordAudioProcessorEditor)
};
