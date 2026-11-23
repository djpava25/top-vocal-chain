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
# TOP BLUE COMP: compresor de color con armonicos de transformador
# ---------------------------------------------------------------------------
juce_add_plugin(TopComp
    COMPANY_NAME                "TopVocal"
    BUNDLE_ID                   "com.topvocal.topbluecomp"
    PRODUCT_NAME                "Top Blue Comp"
    PLUGIN_MANUFACTURER_CODE    Tvoc
    PLUGIN_CODE                 Tbc1
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
target_sources(TopComp PRIVATE
    TopComp/Source/TopCompProcessor.cpp
    TopComp/Source/TopCompEditor.cpp
    Source/UI/RackWidgets.cpp
)
target_include_directories(TopComp PRIVATE Source TopComp/Source)
target_compile_definitions(TopComp PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(TopComp
    PRIVATE juce::juce_audio_processors juce::juce_audio_basics juce::juce_dsp juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# TOP CHOIR: genera coros (octavas y armonias) a partir de una voz
# ---------------------------------------------------------------------------
juce_add_plugin(TopChoir
    COMPANY_NAME                "TopVocal"
    BUNDLE_ID                   "com.topvocal.topchoir"
    PRODUCT_NAME                "Top Choir"
    PLUGIN_MANUFACTURER_CODE    Tvoc
    PLUGIN_CODE                 Tch1
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
target_sources(TopChoir PRIVATE
    TopChoir/Source/TopChoirProcessor.cpp
    TopChoir/Source/ChoirEngine.cpp
    TopChoir/Source/TopChoirEditor.cpp
)
target_include_directories(TopChoir PRIVATE Source TopChoir/Source)
target_compile_definitions(TopChoir PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(TopChoir
    PRIVATE juce::juce_audio_processors juce::juce_audio_formats juce::juce_audio_basics juce::juce_dsp juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# TOP VOICE MIDI: voz / tarareo / beatbox -> MIDI (notas, acordes, bend, CC, bateria)
# ---------------------------------------------------------------------------
juce_add_plugin(TopVoiceMidi
    COMPANY_NAME                "TopVocal"
    BUNDLE_ID                   "com.topvocal.topvoicemidi"
    PRODUCT_NAME                "Top Voice MIDI"
    PLUGIN_MANUFACTURER_CODE    Tvoc
    PLUGIN_CODE                 Tvm1
    FORMATS                     ${TVC_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Analyzer
    AU_MAIN_TYPE                kAudioUnitType_Effect
)
target_sources(TopVoiceMidi PRIVATE
    TopVoiceMidi/Source/TopVoiceMidiProcessor.cpp
    TopVoiceMidi/Source/TopVoiceMidiEditor.cpp
)
target_include_directories(TopVoiceMidi PRIVATE Source TopVoiceMidi/Source)
target_compile_definitions(TopVoiceMidi PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(TopVoiceMidi
    PRIVATE juce::juce_audio_processors juce::juce_audio_devices juce::juce_audio_basics juce::juce_dsp juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# TOP NOTE EDITOR: editor de notas por blobs (tono, tiempo, vibrato, deriva, formantes, volumen)
# ---------------------------------------------------------------------------
juce_add_plugin(TopNoteEditor
    COMPANY_NAME                "TopVocal"
    BUNDLE_ID                   "com.topvocal.topnoteeditor"
    PRODUCT_NAME                "Top Note Editor"
    PLUGIN_MANUFACTURER_CODE    Tvoc
    PLUGIN_CODE                 Tne1
    FORMATS                     ${TVC_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Pitch Shift
    AU_MAIN_TYPE                kAudioUnitType_Effect
)
target_sources(TopNoteEditor PRIVATE
    TopNoteEditor/Source/NoteEngine.cpp
    TopNoteEditor/Source/TopNoteEditorProcessor.cpp
    TopNoteEditor/Source/TopNoteEditorEditor.cpp
)
target_include_directories(TopNoteEditor PRIVATE Source TopNoteEditor/Source)
target_compile_definitions(TopNoteEditor PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_USE_MP3AUDIOFORMAT=1)
target_link_libraries(TopNoteEditor
    PRIVATE juce::juce_audio_processors juce::juce_audio_formats juce::juce_audio_basics juce::juce_dsp juce::juce_gui_basics juce::juce_gui_extra
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# TOP SMOOTH: supresor dinamico de resonancias (fase lineal o 0 ms)
# ---------------------------------------------------------------------------
juce_add_plugin(TopSmooth
    COMPANY_NAME                "TopVocal"
    BUNDLE_ID                   "com.topvocal.topsmooth"
    PRODUCT_NAME                "Top Smooth"
    PLUGIN_MANUFACTURER_CODE    Tvoc
    PLUGIN_CODE                 Tsm1
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
target_sources(TopSmooth PRIVATE
    TopSmooth/Source/TopSmoothProcessor.cpp
    TopSmooth/Source/TopSmoothEditor.cpp
)
target_include_directories(TopSmooth PRIVATE Source TopSmooth/Source)
target_compile_definitions(TopSmooth PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(TopSmooth
    PRIVATE juce::juce_audio_processors juce::juce_audio_basics juce::juce_dsp juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# TOP MASTER: suite de mastering (13 modulos, asistente, loudness, referencia)
# ---------------------------------------------------------------------------
juce_add_plugin(TopMaster
    COMPANY_NAME                "TopVocal"
    BUNDLE_ID                   "com.topvocal.topmaster"
    PRODUCT_NAME                "Top Master"
    PLUGIN_MANUFACTURER_CODE    Tvoc
    PLUGIN_CODE                 Tms1
    FORMATS                     ${TVC_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Mastering
    AU_MAIN_TYPE                kAudioUnitType_Effect
)
target_sources(TopMaster PRIVATE
    TopMaster/Source/TopMasterProcessor.cpp
    TopMaster/Source/TopMasterEditor.cpp
)
target_include_directories(TopMaster PRIVATE Source TopMaster/Source TopSmooth/Source)
target_compile_definitions(TopMaster PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_USE_MP3AUDIOFORMAT=1)
target_link_libraries(TopMaster
    PRIVATE juce::juce_audio_processors juce::juce_audio_formats juce::juce_audio_basics juce::juce_dsp juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# TOP RACK: rack de 8 ranuras con 15 modulos, macros, dream strips y presets
# ---------------------------------------------------------------------------
juce_add_plugin(TopRack
    COMPANY_NAME                "TopVocal"
    BUNDLE_ID                   "com.topvocal.toprack"
    PRODUCT_NAME                "Top Rack"
    PLUGIN_MANUFACTURER_CODE    Tvoc
    PLUGIN_CODE                 Trk1
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
target_sources(TopRack PRIVATE
    TopRack/Source/TopRackProcessor.cpp
    TopRack/Source/TopRackEditor.cpp
)
target_include_directories(TopRack PRIVATE Source TopRack/Source)
target_compile_definitions(TopRack PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(TopRack
    PRIVATE juce::juce_audio_processors juce::juce_audio_basics juce::juce_dsp juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# TOP CONSOLE B: canal de consola estilo serie 4000 B
# ---------------------------------------------------------------------------
juce_add_plugin(TopConsoleB
    COMPANY_NAME                "TopVocal"
    BUNDLE_ID                   "com.topvocal.topconsoleb"
    PRODUCT_NAME                "Top Console B"
    PLUGIN_MANUFACTURER_CODE    Tvoc
    PLUGIN_CODE                 Tcb1
    FORMATS                     ${TVC_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx EQ Dynamics
    AU_MAIN_TYPE                kAudioUnitType_Effect
)
target_sources(TopConsoleB PRIVATE
    TopConsoleB/Source/TopConsoleBProcessor.cpp
    TopConsoleB/Source/TopConsoleBEditor.cpp
)
target_include_directories(TopConsoleB PRIVATE Source TopConsoleB/Source)
target_compile_definitions(TopConsoleB PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(TopConsoleB
    PRIVATE juce::juce_audio_processors juce::juce_audio_basics juce::juce_dsp juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# TOP METER: medidor de sonoridad LUFS, True Peak, VU, goniometro y espectro
# ---------------------------------------------------------------------------
juce_add_plugin(TopMeter
    COMPANY_NAME                "TopVocal"
    BUNDLE_ID                   "com.topvocal.topmeter"
    PRODUCT_NAME                "Top Meter"
    PLUGIN_MANUFACTURER_CODE    Tvoc
    PLUGIN_CODE                 Tmt1
    FORMATS                     ${TVC_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Analyzer
    AU_MAIN_TYPE                kAudioUnitType_Effect
)
target_sources(TopMeter PRIVATE
    TopMeter/Source/TopMeterProcessor.cpp
    TopMeter/Source/TopMeterEditor.cpp
)
target_include_directories(TopMeter PRIVATE Source TopMeter/Source)
target_compile_definitions(TopMeter PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(TopMeter
    PRIVATE juce::juce_audio_processors juce::juce_audio_basics juce::juce_dsp juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# TOP MINI CH: canal compacto (preamp, filtro 75 Hz, EQ 2 bandas, compresor de una perilla)
# ---------------------------------------------------------------------------
juce_add_plugin(TopMiniCH
    COMPANY_NAME                "TopVocal"
    BUNDLE_ID                   "com.topvocal.topminich"
    PRODUCT_NAME                "Top Mini CH"
    PLUGIN_MANUFACTURER_CODE    Tvoc
    PLUGIN_CODE                 Tmc1
    FORMATS                     ${TVC_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx EQ Dynamics
    AU_MAIN_TYPE                kAudioUnitType_Effect
)
target_sources(TopMiniCH PRIVATE
    TopMiniCH/Source/TopMiniCHProcessor.cpp
    TopMiniCH/Source/TopMiniCHEditor.cpp
)
target_include_directories(TopMiniCH PRIVATE Source TopMiniCH/Source)
target_compile_definitions(TopMiniCH PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(TopMiniCH
    PRIVATE juce::juce_audio_processors juce::juce_audio_basics juce::juce_dsp juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# TOP MIC COLLECTION: emulador de microfonos clasicos
# ---------------------------------------------------------------------------
juce_add_plugin(TopMic
    COMPANY_NAME                "TopVocal"
    BUNDLE_ID                   "com.topvocal.topmic"
    PRODUCT_NAME                "Top Mic Collection"
    PLUGIN_MANUFACTURER_CODE    Tvoc
    PLUGIN_CODE                 Tmi1
    FORMATS                     ${TVC_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx EQ
    AU_MAIN_TYPE                kAudioUnitType_Effect
)
target_sources(TopMic PRIVATE
    TopMic/Source/TopMicProcessor.cpp
    TopMic/Source/TopMicEditor.cpp
)
target_include_directories(TopMic PRIVATE Source TopMic/Source)
target_compile_definitions(TopMic PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(TopMic
    PRIVATE juce::juce_audio_processors juce::juce_audio_basics juce::juce_dsp juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# TOP LOCKER: coleccion virtual de microfonos con preamp y sala
# ---------------------------------------------------------------------------
juce_add_plugin(TopLocker
    COMPANY_NAME                "TopVocal"
    BUNDLE_ID                   "com.topvocal.toplocker"
    PRODUCT_NAME                "Top Locker"
    PLUGIN_MANUFACTURER_CODE    Tvoc
    PLUGIN_CODE                 Tlk1
    FORMATS                     ${TVC_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx EQ
    AU_MAIN_TYPE                kAudioUnitType_Effect
)
target_sources(TopLocker PRIVATE
    TopLocker/Source/TopLockerProcessor.cpp
    TopLocker/Source/TopLockerEditor.cpp
    TopLocker/Source/MicArt.cpp
)
target_include_directories(TopLocker PRIVATE Source TopMic/Source TopLocker/Source)
target_compile_definitions(TopLocker PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(TopLocker
    PRIVATE juce::juce_audio_processors juce::juce_audio_basics juce::juce_dsp juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# TOP EQ-1A: ecualizador pasivo a valvulas
# ---------------------------------------------------------------------------
juce_add_plugin(TopEQ
    COMPANY_NAME                "TopVocal"
    BUNDLE_ID                   "com.topvocal.topeq1a"
    PRODUCT_NAME                "Top EQ-1A"
    PLUGIN_MANUFACTURER_CODE    Tvoc
    PLUGIN_CODE                 Teq1
    FORMATS                     ${TVC_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx EQ
    AU_MAIN_TYPE                kAudioUnitType_Effect
)
target_sources(TopEQ PRIVATE
    TopEQ/Source/TopEqProcessor.cpp
    TopEQ/Source/TopEqEditor.cpp
)
target_include_directories(TopEQ PRIVATE Source TopEQ/Source)
target_compile_definitions(TopEQ PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(TopEQ
    PRIVATE juce::juce_audio_processors juce::juce_audio_basics juce::juce_dsp juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# TOP 73: preamp + EQ clase A con transformadores (curvas medidas por impulso)
# ---------------------------------------------------------------------------
juce_add_plugin(Top73
    COMPANY_NAME                "TopVocal"
    BUNDLE_ID                   "com.topvocal.top73"
    PRODUCT_NAME                "Top 73"
    PLUGIN_MANUFACTURER_CODE    Tvoc
    PLUGIN_CODE                 T73p
    FORMATS                     ${TVC_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx EQ
    AU_MAIN_TYPE                kAudioUnitType_Effect
)
target_sources(Top73 PRIVATE
    Top73/Source/Top73Processor.cpp
    Top73/Source/Top73Editor.cpp
)
target_include_directories(Top73 PRIVATE Source Top73/Source)
target_compile_definitions(Top73 PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(Top73
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
# TOP VOCAL CHAIN V3: modulos de color (Dynamics, Tone, Space, SFX) con paneles de detalle
# ---------------------------------------------------------------------------
juce_add_plugin(TopVocalChainV3
    COMPANY_NAME                "TopVocal"
    BUNDLE_ID                   "com.topvocal.topvocalchainv3"
    PRODUCT_NAME                "Top Vocal Chain V3"
    PLUGIN_MANUFACTURER_CODE    Tvoc
    PLUGIN_CODE                 Tvc3
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
target_sources(TopVocalChainV3 PRIVATE Source/PluginProcessor.cpp Source/V3/EditorV3.cpp)
target_include_directories(TopVocalChainV3 PRIVATE Source)
target_compile_definitions(TopVocalChainV3 PUBLIC
    TVC_V3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(TopVocalChainV3
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

    juce_add_console_app(TVCV3Test PRODUCT_NAME "TVCV3Test")
    target_sources(TVCV3Test PRIVATE tests/V3Test.cpp Source/PluginProcessor.cpp Source/V3/EditorV3.cpp)
    target_include_directories(TVCV3Test PRIVATE Source)
    target_compile_definitions(TVCV3Test PRIVATE TVC_V3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Top Vocal Chain V3")
    target_link_libraries(TVCV3Test PRIVATE juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics juce::juce_recommended_config_flags)
    juce_add_console_app(TopCompTest PRODUCT_NAME "TopCompTest")
    target_sources(TopCompTest PRIVATE tests/TopCompTest.cpp TopComp/Source/TopCompProcessor.cpp
                   TopComp/Source/TopCompEditor.cpp Source/UI/RackWidgets.cpp)
    target_include_directories(TopCompTest PRIVATE Source TopComp/Source)
    target_compile_definitions(TopCompTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Top Blue Comp")
    target_link_libraries(TopCompTest PRIVATE juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics juce::juce_recommended_config_flags)
    juce_add_console_app(TopChoirTest PRODUCT_NAME "TopChoirTest")
    target_sources(TopChoirTest PRIVATE tests/TopChoirTest.cpp TopChoir/Source/TopChoirProcessor.cpp
                   TopChoir/Source/TopChoirEditor.cpp TopChoir/Source/ChoirEngine.cpp)
    target_include_directories(TopChoirTest PRIVATE Source TopChoir/Source)
    target_compile_definitions(TopChoirTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Top Choir")
    target_link_libraries(TopChoirTest PRIVATE juce::juce_audio_processors juce::juce_audio_formats juce::juce_dsp juce::juce_gui_basics juce::juce_recommended_config_flags)
    juce_add_console_app(TopVoiceMidiTest PRODUCT_NAME "TopVoiceMidiTest")
    target_sources(TopVoiceMidiTest PRIVATE tests/TopVoiceMidiTest.cpp TopVoiceMidi/Source/TopVoiceMidiProcessor.cpp
                   TopVoiceMidi/Source/TopVoiceMidiEditor.cpp)
    target_include_directories(TopVoiceMidiTest PRIVATE Source TopVoiceMidi/Source)
    target_compile_definitions(TopVoiceMidiTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_ALSA=0 JUCE_JACK=0 JucePlugin_Name="Top Voice MIDI")
    target_link_libraries(TopVoiceMidiTest PRIVATE juce::juce_audio_processors juce::juce_audio_devices juce::juce_dsp juce::juce_gui_basics juce::juce_recommended_config_flags)
    juce_add_console_app(TopNoteEditorTest PRODUCT_NAME "TopNoteEditorTest")
    target_sources(TopNoteEditorTest PRIVATE tests/TopNoteEditorTest.cpp TopNoteEditor/Source/NoteEngine.cpp
                   TopNoteEditor/Source/TopNoteEditorProcessor.cpp TopNoteEditor/Source/TopNoteEditorEditor.cpp)
    target_include_directories(TopNoteEditorTest PRIVATE Source TopNoteEditor/Source)
    target_compile_definitions(TopNoteEditorTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_MP3AUDIOFORMAT=1 JucePlugin_Name="Top Note Editor")
    target_link_libraries(TopNoteEditorTest PRIVATE juce::juce_audio_processors juce::juce_audio_formats juce::juce_dsp juce::juce_gui_basics juce::juce_gui_extra juce::juce_recommended_config_flags)
    juce_add_console_app(TopSmoothTest PRODUCT_NAME "TopSmoothTest")
    target_sources(TopSmoothTest PRIVATE tests/TopSmoothTest.cpp TopSmooth/Source/TopSmoothProcessor.cpp TopSmooth/Source/TopSmoothEditor.cpp)
    target_include_directories(TopSmoothTest PRIVATE Source TopSmooth/Source)
    target_compile_definitions(TopSmoothTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Top Smooth")
    target_link_libraries(TopSmoothTest PRIVATE juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics juce::juce_recommended_config_flags)
    juce_add_console_app(TopMasterTest PRODUCT_NAME "TopMasterTest")
    target_sources(TopMasterTest PRIVATE tests/TopMasterTest.cpp TopMaster/Source/TopMasterProcessor.cpp TopMaster/Source/TopMasterEditor.cpp)
    target_include_directories(TopMasterTest PRIVATE Source TopMaster/Source TopSmooth/Source)
    target_compile_definitions(TopMasterTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_MP3AUDIOFORMAT=1 JucePlugin_Name="Top Master")
    target_link_libraries(TopMasterTest PRIVATE juce::juce_audio_processors juce::juce_audio_formats juce::juce_dsp juce::juce_gui_basics juce::juce_recommended_config_flags)
    juce_add_console_app(TopRackTest PRODUCT_NAME "TopRackTest")
    target_sources(TopRackTest PRIVATE tests/TopRackTest.cpp TopRack/Source/TopRackProcessor.cpp TopRack/Source/TopRackEditor.cpp)
    target_include_directories(TopRackTest PRIVATE Source TopRack/Source)
    target_compile_definitions(TopRackTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Top Rack")
    target_link_libraries(TopRackTest PRIVATE juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics juce::juce_recommended_config_flags)
    juce_add_console_app(TopConsoleBTest PRODUCT_NAME "TopConsoleBTest")
    target_sources(TopConsoleBTest PRIVATE tests/TopConsoleBTest.cpp TopConsoleB/Source/TopConsoleBProcessor.cpp TopConsoleB/Source/TopConsoleBEditor.cpp)
    target_include_directories(TopConsoleBTest PRIVATE Source TopConsoleB/Source)
    target_compile_definitions(TopConsoleBTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Top Console B")
    target_link_libraries(TopConsoleBTest PRIVATE juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics juce::juce_recommended_config_flags)
    juce_add_console_app(TopMeterTest PRODUCT_NAME "TopMeterTest")
    target_sources(TopMeterTest PRIVATE tests/TopMeterTest.cpp TopMeter/Source/TopMeterProcessor.cpp TopMeter/Source/TopMeterEditor.cpp)
    target_include_directories(TopMeterTest PRIVATE Source TopMeter/Source)
    target_compile_definitions(TopMeterTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Top Meter")
    target_link_libraries(TopMeterTest PRIVATE juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics juce::juce_recommended_config_flags)
    juce_add_console_app(TopMiniCHTest PRODUCT_NAME "TopMiniCHTest")
    target_sources(TopMiniCHTest PRIVATE tests/TopMiniCHTest.cpp TopMiniCH/Source/TopMiniCHProcessor.cpp TopMiniCH/Source/TopMiniCHEditor.cpp)
    target_include_directories(TopMiniCHTest PRIVATE Source TopMiniCH/Source)
    target_compile_definitions(TopMiniCHTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Top Mini CH")
    target_link_libraries(TopMiniCHTest PRIVATE juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics juce::juce_recommended_config_flags)
    juce_add_console_app(TopMicTest PRODUCT_NAME "TopMicTest")
    target_sources(TopMicTest PRIVATE tests/TopMicTest.cpp TopMic/Source/TopMicProcessor.cpp TopMic/Source/TopMicEditor.cpp)
    target_include_directories(TopMicTest PRIVATE Source TopMic/Source)
    target_compile_definitions(TopMicTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Top Mic Collection")
    target_link_libraries(TopMicTest PRIVATE juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics juce::juce_recommended_config_flags)
    juce_add_console_app(TopLockerTest PRODUCT_NAME "TopLockerTest")
    target_sources(TopLockerTest PRIVATE tests/TopLockerTest.cpp TopLocker/Source/TopLockerProcessor.cpp TopLocker/Source/TopLockerEditor.cpp TopLocker/Source/MicArt.cpp)
    target_include_directories(TopLockerTest PRIVATE Source TopMic/Source TopLocker/Source)
    target_compile_definitions(TopLockerTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Top Locker")
    target_link_libraries(TopLockerTest PRIVATE juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics juce::juce_recommended_config_flags)
    juce_add_console_app(TopEqTest PRODUCT_NAME "TopEqTest")
    target_sources(TopEqTest PRIVATE tests/TopEqTest.cpp TopEQ/Source/TopEqProcessor.cpp TopEQ/Source/TopEqEditor.cpp)
    target_include_directories(TopEqTest PRIVATE Source TopEQ/Source)
    target_compile_definitions(TopEqTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Top EQ-1A")
    target_link_libraries(TopEqTest PRIVATE juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics juce::juce_recommended_config_flags)
    juce_add_console_app(Top73Test PRODUCT_NAME "Top73Test")
    target_sources(Top73Test PRIVATE tests/Top73Test.cpp Top73/Source/Top73Processor.cpp Top73/Source/Top73Editor.cpp)
    target_include_directories(Top73Test PRIVATE Source Top73/Source)
    target_compile_definitions(Top73Test PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Top 73")
    target_link_libraries(Top73Test PRIVATE juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics juce::juce_recommended_config_flags)
endif()
