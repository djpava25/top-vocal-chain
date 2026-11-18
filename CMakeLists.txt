cmake_minimum_required(VERSION 3.22)

# macOS: compatible desde 10.13 (se compila Universal: Intel + Apple Silicon)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

project(TopVocalChain VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# JUCE: se descarga automaticamente (no hace falta instalar nada)
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(JUCE)

# Formatos: VST3 en todas las plataformas, AU extra en macOS (Logic / GarageBand)
set(TVC_FORMATS VST3)
if(APPLE)
    list(APPEND TVC_FORMATS AU)
endif()

juce_add_plugin(TopVocalChain
    COMPANY_NAME                "TopVocal"
    COMPANY_WEBSITE             "https://github.com"
    BUNDLE_ID                   "com.topvocal.topvocalchain"
    PRODUCT_NAME                "Top Vocal Chain"
    PLUGIN_MANUFACTURER_CODE    Tvoc
    PLUGIN_CODE                 Tvc1
    FORMATS                     ${TVC_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Dynamics EQ
    AU_MAIN_TYPE                kAudioUnitType_Effect
)

target_sources(TopVocalChain PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_include_directories(TopVocalChain PRIVATE Source)

target_compile_definitions(TopVocalChain PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(TopVocalChain
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_basics
        juce::juce_dsp
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ---------------------------------------------------------------------------
# Prueba offline opcional:  cmake -DTVC_BUILD_TESTS=ON
# ---------------------------------------------------------------------------
option(TVC_BUILD_TESTS "Compilar prueba offline del DSP" OFF)
if(TVC_BUILD_TESTS)
    juce_add_console_app(TVCOfflineTest PRODUCT_NAME "TVCOfflineTest")
    target_sources(TVCOfflineTest PRIVATE tests/OfflineTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(TVCOfflineTest PRIVATE Source)
    target_compile_definitions(TVCOfflineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Top Vocal Chain")
    target_link_libraries(TVCOfflineTest PRIVATE juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics
                          juce::juce_recommended_config_flags)
endif()
