cmake_minimum_required(VERSION 3.22)

project(ShmoerghModuleur VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    # Universal binary (Apple Silicon + Intel) unless the caller overrides it.
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    endif()
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
endif()

# ---------------------------------------------------------------------------
# JUCE: fetched at configure time. Point FETCHCONTENT_SOURCE_DIR_JUCE at a
# local checkout to build offline.
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# On macOS, local builds install the AU and VST3 into ~/Library/Audio/Plug-Ins.
option(MODULEUR_COPY_AFTER_BUILD "Copy plugins to the user plugin folders after building" ${APPLE})

set(MODULEUR_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND MODULEUR_FORMATS AU)
endif()

juce_add_plugin(ShmoerghModuleur
    PRODUCT_NAME                "Moduleur"
    COMPANY_NAME                "Shmoergh Clone"
    BUNDLE_ID                   "com.shmoerghclone.moduleur"
    PLUGIN_MANUFACTURER_CODE    Shmc
    PLUGIN_CODE                 Mdlr
    FORMATS                     ${MODULEUR_FORMATS}
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     ${MODULEUR_COPY_AFTER_BUILD}
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    VST3_CATEGORIES             Instrument Synth
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT  "The Moduleur can route audio input into its EXT jacks.")

juce_add_binary_data(ModuleurAssets
    HEADER_NAME ModuleurAssets.h
    NAMESPACE   ModuleurAssets
    SOURCES
        assets/panel/silkscreen.svg
        assets/panel/copper.svg)

target_sources(ShmoerghModuleur PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/dsp/Rack.cpp
    src/dsp/Brain.cpp
    src/ui/Controls.cpp
    src/ui/CableLayer.cpp)

target_include_directories(ShmoerghModuleur PRIVATE src)

target_compile_definitions(ShmoerghModuleur PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ShmoerghModuleur
    PRIVATE
        ModuleurAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Headless DSP tests (no GUI): renders the default patch and checks the
# output is sane. Run with `ctest`.
# ---------------------------------------------------------------------------
option(MODULEUR_BUILD_TESTS "Build DSP tests" ON)
if(MODULEUR_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(ModuleurDspTest PRODUCT_NAME "ModuleurDspTest")
    target_sources(ModuleurDspTest PRIVATE
        tests/DspTest.cpp
        src/dsp/Rack.cpp
        src/dsp/Brain.cpp)
    target_include_directories(ModuleurDspTest PRIVATE src)
    target_compile_definitions(ModuleurDspTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(ModuleurDspTest PRIVATE
        juce::juce_audio_basics juce::juce_audio_formats juce::juce_dsp
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_test(NAME dsp COMMAND ModuleurDspTest)

    # Drives the real plugin processor (shared code target) like a host would.
    juce_add_console_app(ModuleurProcessorTest PRODUCT_NAME "ModuleurProcessorTest")
    target_sources(ModuleurProcessorTest PRIVATE tests/ProcessorTest.cpp)
    # Reuse the shared code's JUCE build instead of compiling the modules twice.
    target_include_directories(ModuleurProcessorTest PRIVATE src $<TARGET_PROPERTY:ShmoerghModuleur,INCLUDE_DIRECTORIES>)
    target_compile_definitions(ModuleurProcessorTest PRIVATE
        $<FILTER:$<TARGET_PROPERTY:ShmoerghModuleur,COMPILE_DEFINITIONS>,EXCLUDE,^JUCE_STANDALONE_APPLICATION>)
    target_link_libraries(ModuleurProcessorTest PRIVATE ShmoerghModuleur)
    add_test(NAME processor COMMAND ModuleurProcessorTest)
endif()
