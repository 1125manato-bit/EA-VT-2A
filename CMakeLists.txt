cmake_minimum_required(VERSION 3.15)

project(VT-2A VERSION 1.0.0)

# Build Universal Binary for Mac
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Main architectures" FORCE)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(VT-2A
    COMPANY_NAME "EMU AUDIO"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE EAud
    PLUGIN_CODE Vt2f
    BUNDLE_ID "com.EMU-AUDIO.VT-2A.Final"
    FORMATS AU VST3
    PRODUCT_NAME "EA VT-2A"
)

juce_generate_juce_header(VT-2A)

target_sources(VT-2A
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

juce_add_binary_data(SharedResources
    SOURCES
        Resources/background.jpg
        Resources/knob.png
)

target_compile_definitions(VT-2A
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(VT-2A
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        SharedResources
)
