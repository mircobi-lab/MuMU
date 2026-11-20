cmake_minimum_required(VERSION 3.22)

# MBStudio91 MuMU — sintetizzatore polifonico analogico virtuale ispirato all'architettura del Moog Muse (VST3 / AU / Standalone)
# Progetto distribuito sotto AGPL v3 (vedi LICENSE.md). Moog e Muse sono marchi di Moog Music Inc.: MuMU non è affiliato.
project(MBStudio91MuMU VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "" FORCE)
    endif()
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
    endif()
endif()

set(JUCE_DIR "" CACHE PATH "Percorso di una copia locale di JUCE")
if(JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_DIR}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.5
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(MUMU_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND MUMU_FORMATS AU)
endif()

juce_add_plugin(MBStudio91MuMU
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/icons/mumu-macos-1024.png"
    COMPANY_NAME "MBStudio91"
    COMPANY_WEBSITE "https://mbstudio91.fun"
    BUNDLE_ID "fun.mbstudio91.mumu"
    PRODUCT_NAME "MBStudio91 MuMU"
    VERSION ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE Mbst
    PLUGIN_CODE Mmu1
    FORMATS ${MUMU_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    HARDENED_RUNTIME_ENABLED TRUE)

include(MBStudioUI/MBStudioUI.cmake)
mbstudio_ui_add(MBStudio91MuMU)

target_sources(MBStudio91MuMU PRIVATE
    Source/Params.cpp
    Source/Engine.cpp
    Source/Presets.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

if(TARGET MBStudio91MuMU_Standalone)
    target_sources(MBStudio91MuMU_Standalone PRIVATE Source/StandaloneApp.cpp)
    target_compile_definitions(MBStudio91MuMU_Standalone PRIVATE JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)
endif()

target_compile_definitions(MBStudio91MuMU PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_JACK=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(MBStudio91MuMU
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# strumento di prova offline (render dei preset in WAV), non fa parte del plugin
option(MUMU_BUILD_TOOLS "Compila lo strumento di render di prova" OFF)
if(MUMU_BUILD_TOOLS)
    juce_add_console_app(mumu_render PRODUCT_NAME "mumu_render")
    target_sources(mumu_render PRIVATE tools/render.cpp Source/Params.cpp Source/Engine.cpp Source/Presets.cpp)
    target_include_directories(mumu_render PRIVATE Source)
    target_compile_definitions(mumu_render PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(mumu_render PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_audio_formats PUBLIC juce::juce_recommended_config_flags)
endif()
