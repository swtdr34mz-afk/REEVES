# REEVES — the plugin (VST3, AU on the Mac) and the standalone app, with JUCE 8.
#   cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release
# JUCE comes from GitHub (or -DREEVES_JUCE_DIR=/path/to/JUCE).
cmake_minimum_required(VERSION 3.22)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    endif()
endif()
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

project(REEVES VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REEVES_JUCE_DIR "" CACHE PATH "a JUCE checkout (empty: from GitHub)")
if(REEVES_JUCE_DIR)
    add_subdirectory(${REEVES_JUCE_DIR} juce)
else()
    include(FetchContent)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif()

set(REEVES_FORMATS VST3 Standalone)
if(REEVES_LOCAL_TEST)
    set(REEVES_FORMATS VST3)
endif()
if(APPLE)
    list(APPEND REEVES_FORMATS AU)
endif()

juce_add_plugin(Reeves
    PRODUCT_NAME "REEVES"
    COMPANY_NAME "swtdr34mz"
    BUNDLE_ID "com.swtdr34mz.reeves"
    PLUGIN_MANUFACTURER_CODE Swtd
    PLUGIN_CODE Rvs2
    FORMATS ${REEVES_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    MICROPHONE_PERMISSION_ENABLED FALSE)

juce_add_binary_data(ReevesData SOURCES
    fonts/FiraSans-SemiBold.ttf
    fonts/FiraSans-Bold.ttf
    fonts/FiraSans-ExtraBold.ttf
    fonts/AnonymousPro-Bold.ttf
    res/swtdr34mz-tag.png)

target_sources(Reeves PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/ui/Look.cpp
    dsp/Engine.cpp
    dsp/Params.cpp
    dsp/Presets.cpp
    dsp/Rev2Syx.cpp)

target_include_directories(Reeves PRIVATE dsp src)

target_compile_definitions(Reeves PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Reeves PRIVATE
    ReevesData
    juce::juce_audio_utils
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# a picture of the editor, without a window (tools/snapshot.cpp): -DREEVES_SNAPSHOT=ON
option(REEVES_SNAPSHOT "build ReevesShot, the editor's picture" OFF)
if(REEVES_SNAPSHOT)
    juce_add_console_app(ReevesShot PRODUCT_NAME "ReevesShot")
    target_sources(ReevesShot PRIVATE tools/snapshot.cpp src/PluginProcessor.cpp src/PluginEditor.cpp src/ui/Look.cpp
        dsp/Engine.cpp dsp/Params.cpp dsp/Presets.cpp dsp/Rev2Syx.cpp)
    target_include_directories(ReevesShot PRIVATE dsp src)
    target_compile_definitions(ReevesShot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1
        JucePlugin_Build_Standalone=0)
    target_link_libraries(ReevesShot PRIVATE ReevesData juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
