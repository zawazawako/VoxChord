#pragma once

#include <memory>

#include "PluginProcessor.h"
#include "VoxChordLookAndFeel.h"

#if VoxChord_ENABLE_INSPECTOR
 #include "melatonin_inspector/melatonin_inspector.h"
#endif

class VoxChordAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit VoxChordAudioProcessorEditor (VoxChordAudioProcessor&);
    ~VoxChordAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

#if VoxChord_ENABLE_INSPECTOR
    bool keyPressed (const juce::KeyPress& key) override;
#endif

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void configureSlider (juce::Slider& slider, juce::Label& label, const juce::String& text);
    void configureCompactSlider (juce::Slider& slider, juce::Label& label, const juce::String& text);
    void layoutEditableSlider (juce::Slider& slider, juce::Label& label, juce::Label& valueLabel, juce::Rectangle<int> bounds);
    void layoutComboBox (juce::ComboBox& comboBox, juce::Label& label, juce::Rectangle<int> bounds);
    void layoutCompactSlider (juce::Slider& slider, juce::Label& label, juce::Rectangle<int> bounds);
    void layoutSectionTitle (juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title);
    void updateMidiState();
    void updateMeters();
    void updatePitchDebug();
    void updateGlideBoxFromParameter();
    void commitGlideBoxSelection();
    // Width of `text` when drawn at `fontHeight`, used to align header labels.
    static int textWidthFor (const juce::String& text, float fontHeight, bool bold);
    void configureEditableValueLabel (juce::Label& label);
    void updateEditableValueLabels();
    enum class EditableValueFormat
    {
        decibels,
        percent,
        integer
    };

    void updateEditableValueLabel (juce::Label& label, const juce::String& parameterId, EditableValueFormat format);
    void commitEditableValueLabel (juce::Label& label, const juce::String& parameterId, EditableValueFormat format);
    juce::String formatEditableValue (const juce::String& parameterId, EditableValueFormat format) const;
    bool parseEditableValue (const juce::String& text, EditableValueFormat format, float& value) const;
    static juce::String formatDecibelValue (float value);
    static juce::String formatPercentValue (float value);
    static juce::String formatIntegerValue (float value);

    class VerticalMeter final : public juce::Component
    {
    public:
        void setTitle (const juce::String& newTitle);
        void setLevel (float newPeak, bool isClipped);
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& event) override;

        // Called when the meter is clicked; used to clear latched clip flags.
        std::function<void()> onClick;

    private:
        // Meter ballistics run on the editor's 30 Hz timer: instant attack,
        // ~250 ms release, plus a 1.5 s peak-hold marker that then decays.
        static constexpr float floorDb = -60.0f;
        static constexpr float releaseCoefficient = 0.28f;
        static constexpr int peakHoldFrameCount = 45;
        static constexpr float peakHoldDecayPerFrame = 0.012f;

        static float normalizedFor (float linearPeak) noexcept;

        juce::String title;
        float peak = 0.0f;         // raw linear peak, for the dB readout
        float displayLevel = 0.0f; // ballistic bar height, 0..1
        float peakHold = 0.0f;     // peak-hold marker, 0..1
        int peakHoldFrames = 0;
        bool clipped = false;
        melatonin::InnerShadow wellShadow { juce::Colours::black.withAlpha (0.55f), 6, { 0, 2 } };
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
        melatonin::InnerShadow wellShadow { juce::Colours::black.withAlpha (0.5f), 8, { 0, 3 } };
    };

    VoxChordAudioProcessor& processorRef;

    // LookAndFeel + cached card geometry/shadows (built once in resized(),
    // painted cheaply per frame — ui-quality-guide.md sections 1/2).
    voxchord::VoxChordLookAndFeel lookAndFeel;
    std::array<juce::Path, 4> cardPaths;                  // header, harmony, midi, level
    std::array<juce::Rectangle<int>, 4> cardRects;
    std::array<melatonin::DropShadow, 4> cardShadows {
        melatonin::DropShadow { juce::Colours::black.withAlpha (0.40f), 14, { 0, 5 } },
        melatonin::DropShadow { juce::Colours::black.withAlpha (0.40f), 14, { 0, 5 } },
        melatonin::DropShadow { juce::Colours::black.withAlpha (0.40f), 14, { 0, 5 } },
        melatonin::DropShadow { juce::Colours::black.withAlpha (0.40f), 14, { 0, 5 } },
    };
    juce::Path characterCardPath;
    melatonin::InnerShadow characterCardShadow { juce::Colours::black.withAlpha (0.45f), 10, { 0, 3 } };
    juce::ColourGradient backgroundGradient;

    juce::ComboBox voiceCountBox;
    juce::ComboBox glideBox;
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
    juce::Label spreadValueLabel;
    juce::Label dryWetValueLabel;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label midiNotesLabel;
    juce::Label voiceSlotsLabel;
    juce::Label midiStatusLabel;
    juce::Label pitchDebugLabel;
    juce::Label compactPitchLabel;
    juce::Label compactLastLabel;
    VerticalMeter inputMeter;
    VerticalMeter outputLeftMeter;
    VerticalMeter outputRightMeter;
    MiniKeyboard miniKeyboard;
    juce::Label inputSourceLabel;
    juce::ComboBox inputSourceBox;
    juce::ComboBox characterModeBox;
    juce::ToggleButton leadTuneButton { "Auto Tune" };
    juce::ToggleButton monoOutputButton { "Mono Out" };
    juce::ToggleButton psolaButton { "High Quality" };
    juce::TextButton panicButton { "PANIC" };
    juce::Rectangle<int> characterCardBounds;

    std::unique_ptr<ComboBoxAttachment> voiceCountAttachment;
    std::unique_ptr<SliderAttachment> characterAmountAttachment;
    std::unique_ptr<SliderAttachment> spreadAttachment;
    std::unique_ptr<SliderAttachment> dryWetAttachment;
    std::unique_ptr<SliderAttachment> inputGainAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;
    std::unique_ptr<ComboBoxAttachment> inputSourceAttachment;
    std::unique_ptr<ComboBoxAttachment> characterModeAttachment;
    std::unique_ptr<ButtonAttachment> leadTuneAttachment;
    std::unique_ptr<ButtonAttachment> monoOutputAttachment;
    std::unique_ptr<ButtonAttachment> psolaAttachment;

    uint32_t lastSeenMidiActivityCounter = 0;
    bool isUpdatingEditableValueLabels = false;

#if VoxChord_ENABLE_INSPECTOR
    melatonin::Inspector inspector { *this, false }; // toggled with F12, Debug only
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxChordAudioProcessorEditor)
};
