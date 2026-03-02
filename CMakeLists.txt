cmake_minimum_required(VERSION 3.22)
project(MagicTrack VERSION 0.1.0 LANGUAGES C CXX)

if(NOT DEFINED JUCE_DIR)
  message(FATAL_ERROR "Set JUCE_DIR to your JUCE checkout, e.g. -DJUCE_DIR=/path/to/JUCE")
endif()

add_subdirectory(${JUCE_DIR} JUCE)

juce_add_binary_data(MagicTrackData
  SOURCES
    Resources/phrases.v1.json
)

juce_add_plugin(MagicTrack
  COMPANY_NAME "OpenClaw"
  BUNDLE_ID "com.openclaw.magictrack"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE
  PLUGIN_MANUFACTURER_CODE OCLW
  PLUGIN_CODE MGTR
  FORMATS AU VST3
  PRODUCT_NAME "MagicTrack"
)

juce_generate_juce_header(MagicTrack)

target_sources(MagicTrack
  PRIVATE
    Source/MagicTrackAudioProcessor.cpp
    Source/MagicTrackAudioProcessor.h
    Source/MagicTrackEditor.cpp
    Source/MagicTrackEditor.h
    Source/TextCommandParser.cpp
    Source/TextCommandParser.h
    Source/MacroTypes.h
)

target_compile_features(MagicTrack PRIVATE cxx_std_20)

target_compile_definitions(MagicTrack
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(MagicTrack
  PRIVATE
    MagicTrackData
    juce::juce_dsp
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

add_executable(magictrack_parser_cli
  Tests/ParserCli.cpp
  Source/TextCommandParser.cpp
)

target_include_directories(magictrack_parser_cli PRIVATE Source)
target_compile_features(magictrack_parser_cli PRIVATE cxx_std_20)
target_link_libraries(magictrack_parser_cli PRIVATE juce::juce_core juce::juce_data_structures)
