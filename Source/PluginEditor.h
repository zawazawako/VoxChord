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

    void timerCallback() override;
    void configureSlider (juce::Slider& slider, juce::Label& label, const juce::String& text);
    void layoutSlider (juce::Slider& slider, juce::Label& label, juce::Rectangle<int> bounds);
    void updateMidiState();
    void updateMeters();
    void updatePitchDebug();

    VoxChordAudioProcessor& processorRef;

    juce::Slider voiceCountSlider;
    juce::Slider tuneSlider;
    juce::Slider glideSlider;
    juce::Slider characterSlider;
    juce::Slider spreadSlider;
    juce::Slider dryWetSlider;
    juce::Slider inputGainSlider;
    juce::Slider outputSlider;

    juce::Label voiceCountLabel;
    juce::Label tuneLabel;
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
    juce::Label inputMeterLabel;
    juce::Label outputMeterLabel;
    juce::Label inputSourceLabel;
    juce::ComboBox inputSourceBox;
    juce::TextButton panicButton { "PANIC" };

    std::unique_ptr<SliderAttachment> voiceCountAttachment;
    std::unique_ptr<SliderAttachment> tuneAttachment;
    std::unique_ptr<SliderAttachment> glideAttachment;
    std::unique_ptr<SliderAttachment> characterAttachment;
    std::unique_ptr<SliderAttachment> spreadAttachment;
    std::unique_ptr<SliderAttachment> dryWetAttachment;
    std::unique_ptr<SliderAttachment> inputGainAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;
    std::unique_ptr<ComboBoxAttachment> inputSourceAttachment;

    uint32_t lastSeenMidiActivityCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxChordAudioProcessorEditor)
};
