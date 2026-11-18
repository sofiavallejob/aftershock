cmake_minimum_required(VERSION 3.22)
project(Aftershock VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
endif()

# JUCE: use a local checkout if given (-DJUCE_DIR=...), otherwise download it
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} juce EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# the earthquake pack and the coastline are compiled into the binary
juce_add_binary_data(AftershockData SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/data/aftershock_pack.bin.gz
    ${CMAKE_CURRENT_SOURCE_DIR}/data/land.json
    ${CMAKE_CURRENT_SOURCE_DIR}/presets/factory_presets.json
    ${CMAKE_CURRENT_SOURCE_DIR}/data/PixelifySans.ttf)

juce_add_plugin(Aftershock
    COMPANY_NAME "Sofia Vallejo"
    BUNDLE_ID "com.sofiavallejo.aftershock"
    PLUGIN_MANUFACTURER_CODE Sval
    PLUGIN_CODE Aftr
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Aftershock"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Reverb
    VST3_AUTO_MANIFEST FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

set(CORE_SOURCES Source/EventPack.cpp Source/IRGenerator.cpp)
set(PROC_SOURCES Source/PluginProcessor.cpp Source/PresetManager.cpp Source/Theme.cpp)
set(UI_SOURCES Source/PluginEditor.cpp Source/Widgets.cpp Source/Views.cpp Source/Panels.cpp)

target_sources(Aftershock PRIVATE ${CORE_SOURCES} ${PROC_SOURCES} ${UI_SOURCES})
target_compile_definitions(Aftershock PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(Aftershock
    PRIVATE AftershockData juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# command-line renderer: writes the IRs of the whole pack to WAV files
juce_add_console_app(aftershock_render PRODUCT_NAME "aftershock_render")
target_sources(aftershock_render PRIVATE ${CORE_SOURCES} Source/RenderTool.cpp)
target_compile_definitions(aftershock_render PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(aftershock_render
    PRIVATE AftershockData juce::juce_core juce::juce_audio_basics juce::juce_audio_formats
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# headless test of the real processor (not part of the plugin)
juce_add_console_app(aftershock_test PRODUCT_NAME "aftershock_test")
target_sources(aftershock_test PRIVATE ${CORE_SOURCES} ${PROC_SOURCES} ${UI_SOURCES} Source/ProcessorTest.cpp)
target_compile_definitions(aftershock_test PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JucePlugin_Name="Aftershock" JucePlugin_VersionString="${PROJECT_VERSION}" JucePlugin_Build_Standalone=0 JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0)
target_link_libraries(aftershock_test PRIVATE AftershockData juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags)

# renders the editor to PNG files, for the manual and for checks
juce_add_console_app(aftershock_snapshot PRODUCT_NAME "aftershock_snapshot")
target_sources(aftershock_snapshot PRIVATE ${CORE_SOURCES} ${PROC_SOURCES} ${UI_SOURCES} Source/Snapshot.cpp)
target_compile_definitions(aftershock_snapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
    JucePlugin_Name="Aftershock" JucePlugin_VersionString="${PROJECT_VERSION}" JucePlugin_Build_Standalone=0 JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0)
target_link_libraries(aftershock_snapshot PRIVATE AftershockData juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags)

# runs a dry file through the plugin with a factory preset (sound examples)
juce_add_console_app(aftershock_demo PRODUCT_NAME "aftershock_demo")
target_sources(aftershock_demo PRIVATE ${CORE_SOURCES} ${PROC_SOURCES} ${UI_SOURCES} Source/DemoRender.cpp)
target_compile_definitions(aftershock_demo PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JucePlugin_Name="Aftershock" JucePlugin_VersionString="${PROJECT_VERSION}" JucePlugin_Build_Standalone=0 JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0)
target_link_libraries(aftershock_demo PRIVATE AftershockData juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags)
