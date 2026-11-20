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

    juce_add_console_app(TopStripTest PRODUCT_NAME "TopStripTest")
    target_sources(TopStripTest PRIVATE tests/TopStripTest.cpp TopStrip/Source/TopStripProcessor.cpp
                   TopStrip/Source/TopStripEditor.cpp Source/UI/RackWidgets.cpp)
    target_include_directories(TopStripTest PRIVATE Source TopStrip/Source)
    target_compile_definitions(TopStripTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Top Strip 737")
    target_link_libraries(TopStripTest PRIVATE juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics juce::juce_recommended_config_flags)

    juce_add_console_app(TopDrumsTest PRODUCT_NAME "TopDrumsTest")
    target_sources(TopDrumsTest PRIVATE tests/TopDrumsTest.cpp TopDrums/Source/TopDrumsProcessor.cpp
                   TopDrums/Source/TopDrumsEditor.cpp Source/UI/RackWidgets.cpp)
    target_include_directories(TopDrumsTest PRIVATE Source TopDrums/Source)
    target_compile_definitions(TopDrumsTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Top Drums")
    target_link_libraries(TopDrumsTest PRIVATE juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics juce::juce_recommended_config_flags)

    juce_add_console_app(TVCV2Test PRODUCT_NAME "TVCV2Test")
    target_sources(TVCV2Test PRIVATE tests/V2Test.cpp Source/PluginProcessor.cpp Source/V2/EditorV2.cpp)
    target_include_directories(TVCV2Test PRIVATE Source)
    target_compile_definitions(TVCV2Test PRIVATE TVC_V2=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Top Vocal Chain V2")
    target_link_libraries(TVCV2Test PRIVATE juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics juce::juce_recommended_config_flags)
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
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Dynamics EQ
    AU_MAIN_TYPE                kAudioUnitType_Effect
)

target_sources(TopVocalChain PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/ConsoleWidgets.cpp
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
# TOP TUNE: afinador en tiempo real (pitch, formant, MIDI, detector de tonalidad)
# ---------------------------------------------------------------------------
juce_add_plugin(TopTune
    COMPANY_NAME                "TopVocal"
    BUNDLE_ID                   "com.topvocal.toptune"
    PRODUCT_NAME                "Top Tune"
    PLUGIN_MANUFACTURER_CODE    Tvoc
    PLUGIN_CODE                 Ttn1
    FORMATS                     ${TVC_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Pitch Shift
    AU_MAIN_TYPE                kAudioUnitType_MusicEffect
)

target_sources(TopTune PRIVATE
    TopTune/Source/TopTuneProcessor.cpp
    TopTune/Source/TopTuneEditor.cpp
)
target_include_directories(TopTune PRIVATE Source TopTune/Source)
target_compile_definitions(TopTune PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(TopTune
    PRIVATE juce::juce_audio_processors juce::juce_audio_basics juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# TOP SUMMING: sumador analogico estilo consola britanica (8816)
# ---------------------------------------------------------------------------
juce_add_plugin(TopSumming
    COMPANY_NAME                "TopVocal"
    BUNDLE_ID                   "com.topvocal.topsumming"
    PRODUCT_NAME                "Top Summing"
    PLUGIN_MANUFACTURER_CODE    Tvoc
    PLUGIN_CODE                 Tsm1
    FORMATS                     ${TVC_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Distortion
    AU_MAIN_TYPE                kAudioUnitType_Effect
)
target_sources(TopSumming PRIVATE
    TopSumming/Source/TopSummingProcessor.cpp
    TopSumming/Source/TopSummingEditor.cpp
    Source/UI/RackWidgets.cpp
)
target_include_directories(TopSumming PRIVATE Source TopSumming/Source)
target_compile_definitions(TopSumming PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(TopSumming
    PRIVATE juce::juce_audio_processors juce::juce_audio_basics juce::juce_dsp juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# TOP STRIP 737: channel strip de valvula (preamp + compresor optico + EQ 4 bandas)
# ---------------------------------------------------------------------------
juce_add_plugin(TopStrip
    COMPANY_NAME                "TopVocal"
    BUNDLE_ID                   "com.topvocal.topstrip737"
    PRODUCT_NAME                "Top Strip 737"
    PLUGIN_MANUFACTURER_CODE    Tvoc
    PLUGIN_CODE                 Tst7
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
target_sources(TopStrip PRIVATE
    TopStrip/Source/TopStripProcessor.cpp
    TopStrip/Source/TopStripEditor.cpp
    Source/UI/RackWidgets.cpp
)
target_include_directories(TopStrip PRIVATE Source TopStrip/Source)
target_compile_definitions(TopStrip PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(TopStrip
    PRIVATE juce::juce_audio_processors juce::juce_audio_basics juce::juce_dsp juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# TOP DRUMS: transient shaper + color + glue + asistente de mezcla con IA
# ---------------------------------------------------------------------------
juce_add_plugin(TopDrums
    COMPANY_NAME                "TopVocal"
    BUNDLE_ID                   "com.topvocal.topdrums"
    PRODUCT_NAME                "Top Drums"
    PLUGIN_MANUFACTURER_CODE    Tvoc
    PLUGIN_CODE                 Tdr1
    FORMATS                     ${TVC_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Dynamics
    AU_MAIN_TYPE                kAudioUnitType_Effect
)
target_sources(TopDrums PRIVATE
    TopDrums/Source/TopDrumsProcessor.cpp
    TopDrums/Source/TopDrumsEditor.cpp
    Source/UI/RackWidgets.cpp
)
target_include_directories(TopDrums PRIVATE Source TopDrums/Source)
target_compile_definitions(TopDrums PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(TopDrums
    PRIVATE juce::juce_audio_processors juce::juce_audio_basics juce::juce_dsp juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# TOP VOCAL CHAIN V2: mismo motor, interfaz oscura neon con macros de un clic
# ---------------------------------------------------------------------------
juce_add_plugin(TopVocalChainV2
    COMPANY_NAME                "TopVocal"
    BUNDLE_ID                   "com.topvocal.topvocalchainv2"
    PRODUCT_NAME                "Top Vocal Chain V2"
    PLUGIN_MANUFACTURER_CODE    Tvoc
    PLUGIN_CODE                 Tvc2
    FORMATS                     ${TVC_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Dynamics EQ
    AU_MAIN_TYPE                kAudioUnitType_Effect
)
target_sources(TopVocalChainV2 PRIVATE Source/PluginProcessor.cpp Source/V2/EditorV2.cpp)
target_include_directories(TopVocalChainV2 PRIVATE Source)
target_compile_definitions(TopVocalChainV2 PUBLIC
    TVC_V2=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(TopVocalChainV2
    PRIVATE juce::juce_audio_processors juce::juce_audio_basics juce::juce_dsp juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Prueba offline opcional:  cmake -DTVC_BUILD_TESTS=ON
# ---------------------------------------------------------------------------
option(TVC_BUILD_TESTS "Compilar prueba offline del DSP" OFF)
if(TVC_BUILD_TESTS)
    juce_add_console_app(TVCOfflineTest PRODUCT_NAME "TVCOfflineTest")
    target_sources(TVCOfflineTest PRIVATE tests/OfflineTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/UI/ConsoleWidgets.cpp)
    target_include_directories(TVCOfflineTest PRIVATE Source)
    target_compile_definitions(TVCOfflineTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Top Vocal Chain")
    target_link_libraries(TVCOfflineTest PRIVATE juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics
                          juce::juce_recommended_config_flags)

    juce_add_console_app(TopTuneTest PRODUCT_NAME "TopTuneTest")
    target_sources(TopTuneTest PRIVATE tests/TopTuneTest.cpp TopTune/Source/TopTuneProcessor.cpp TopTune/Source/TopTuneEditor.cpp)
    target_include_directories(TopTuneTest PRIVATE Source TopTune/Source)
    target_compile_definitions(TopTuneTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Top Tune")
    target_link_libraries(TopTuneTest PRIVATE juce::juce_audio_processors juce::juce_gui_basics juce::juce_recommended_config_flags)

    juce_add_console_app(TopSummingTest PRODUCT_NAME "TopSummingTest")
    target_sources(TopSummingTest PRIVATE tests/TopSummingTest.cpp TopSumming/Source/TopSummingProcessor.cpp
                   TopSumming/Source/TopSummingEditor.cpp Source/UI/RackWidgets.cpp)
    target_include_directories(TopSummingTest PRIVATE Source TopSumming/Source)
    target_compile_definitions(TopSummingTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Top Summing")
    target_link_libraries(TopSummingTest PRIVATE juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics juce::juce_recommended_config_flags)

    juce_add_console_app(TopStripTest PRODUCT_NAME "TopStripTest")
    target_sources(TopStripTest PRIVATE tests/TopStripTest.cpp TopStrip/Source/TopStripProcessor.cpp
                   TopStrip/Source/TopStripEditor.cpp Source/UI/RackWidgets.cpp)
    target_include_directories(TopStripTest PRIVATE Source TopStrip/Source)
    target_compile_definitions(TopStripTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Top Strip 737")
    target_link_libraries(TopStripTest PRIVATE juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics juce::juce_recommended_config_flags)

    juce_add_console_app(TopDrumsTest PRODUCT_NAME "TopDrumsTest")
    target_sources(TopDrumsTest PRIVATE tests/TopDrumsTest.cpp TopDrums/Source/TopDrumsProcessor.cpp
                   TopDrums/Source/TopDrumsEditor.cpp Source/UI/RackWidgets.cpp)
    target_include_directories(TopDrumsTest PRIVATE Source TopDrums/Source)
    target_compile_definitions(TopDrumsTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Top Drums")
    target_link_libraries(TopDrumsTest PRIVATE juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics juce::juce_recommended_config_flags)

    juce_add_console_app(TVCV2Test PRODUCT_NAME "TVCV2Test")
    target_sources(TVCV2Test PRIVATE tests/V2Test.cpp Source/PluginProcessor.cpp Source/V2/EditorV2.cpp)
    target_include_directories(TVCV2Test PRIVATE Source)
    target_compile_definitions(TVCV2Test PRIVATE TVC_V2=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Top Vocal Chain V2")
    target_link_libraries(TVCV2Test PRIVATE juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics juce::juce_recommended_config_flags)
endif()
