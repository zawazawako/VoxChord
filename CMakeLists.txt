cmake_minimum_required(VERSION 3.22)

project(VoxChord VERSION 0.1.15)

add_subdirectory(../JUCE JUCE)

juce_add_plugin(VoxChord
    COMPANY_NAME "VoxChord"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Vxcd
    PLUGIN_CODE VxC1
    FORMATS VST3 Standalone
    PRODUCT_NAME "VoxChord")

target_sources(VoxChord
    PRIVATE
        Source/LevelMeterState.h
        Source/MidiVoiceState.cpp
        Source/MidiVoiceState.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/SimpleChoirEngine.cpp
        Source/SimpleChoirEngine.h
        Source/VoxChordParameters.cpp
        Source/VoxChordParameters.h)

target_compile_definitions(VoxChord
    PUBLIC
        JUCE_ASIO=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(VoxChord
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
