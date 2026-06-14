cmake_minimum_required(VERSION 3.22)

project(BackingTrackTrigger VERSION 2.0.0)

# Build only what the current platform supports unless told otherwise.
if(APPLE)
    set(BTT_FORMATS AU VST3 Standalone)
else()
    set(BTT_FORMATS VST3 Standalone)
endif()

# Add JUCE as a subdirectory (git submodule).
add_subdirectory(JUCE)

juce_add_plugin(BackingTrackTrigger
    COMPANY_NAME "LEEDALE"
    BUNDLE_ID com.leedale.backingtracktrigger
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Ldal
    PLUGIN_CODE Bktr
    FORMATS ${BTT_FORMATS}
    PRODUCT_NAME "Backing Track Trigger"
    VST3_PRODUCT_NAME "Backing Track Trigger (VST3)"
    AU_PRODUCT_NAME "Backing Track Trigger (AU)"
)

juce_generate_juce_header(BackingTrackTrigger)

target_sources(BackingTrackTrigger
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_compile_features(BackingTrackTrigger PRIVATE cxx_std_17)

target_compile_definitions(BackingTrackTrigger
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_USE_FLAC=1
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(BackingTrackTrigger
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

#==============================================================================
# Unit tests (off by default): cmake -B build -DBTT_BUILD_TESTS=ON
#==============================================================================
option(BTT_BUILD_TESTS "Build BackingTrackTrigger unit tests" OFF)

if(BTT_BUILD_TESTS)
    enable_testing()

    juce_add_console_app(BackingTrackTriggerTests
        PRODUCT_NAME "BackingTrackTriggerTests")

    juce_generate_juce_header(BackingTrackTriggerTests)

    target_sources(BackingTrackTriggerTests
        PRIVATE
            Tests/PluginTests.cpp
            Source/PluginProcessor.cpp
            Source/PluginEditor.cpp)

    target_compile_features(BackingTrackTriggerTests PRIVATE cxx_std_17)

    target_compile_definitions(BackingTrackTriggerTests
        PRIVATE
            JucePlugin_Name="Backing Track Trigger"
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_USE_FLAC=1
            JUCE_STANDALONE_APPLICATION=1
            JUCE_MODAL_LOOPS_PERMITTED=1)

    target_link_libraries(BackingTrackTriggerTests
        PRIVATE
            juce::juce_audio_utils
            juce::juce_audio_formats
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    add_test(NAME BackingTrackTriggerTests COMMAND BackingTrackTriggerTests)

    # Offline UI snapshot tool (renders the editor to a PNG).
    juce_add_console_app(BackingTrackTriggerSnapshot
        PRODUCT_NAME "BackingTrackTriggerSnapshot")
    juce_generate_juce_header(BackingTrackTriggerSnapshot)
    target_sources(BackingTrackTriggerSnapshot
        PRIVATE
            Tools/Snapshot.cpp
            Source/PluginProcessor.cpp
            Source/PluginEditor.cpp)
    target_compile_features(BackingTrackTriggerSnapshot PRIVATE cxx_std_17)
    target_compile_definitions(BackingTrackTriggerSnapshot
        PRIVATE
            JucePlugin_Name="Backing Track Trigger"
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_USE_FLAC=1
            JUCE_STANDALONE_APPLICATION=1
            JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(BackingTrackTriggerSnapshot
        PRIVATE
            juce::juce_audio_utils
            juce::juce_audio_formats
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()
