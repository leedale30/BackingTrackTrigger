cmake_minimum_required(VERSION 3.22)

project(BackingTrackTrigger VERSION 1.0.0)

# Add JUCE as a subdirectory
add_subdirectory(JUCE)

# Plugin configuration
juce_add_plugin(BackingTrackTrigger
    COMPANY_NAME "AntonyleeDale"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Adal
    PLUGIN_CODE Bktr
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Backing Track Trigger"
)

# Source files
target_sources(BackingTrackTrigger
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

# Preprocessor definitions
target_compile_definitions(BackingTrackTrigger
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# Link JUCE modules
target_link_libraries(BackingTrackTrigger
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
