cmake_minimum_required(VERSION 3.22)
project(Aeryther VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(AERYTHER_BUILD_TESTS "Build portable DSP/state regression tests" ON)
option(AERYTHER_BUILD_PLUGIN "Build JUCE Aeryther plugin" ON)
option(AERYTHER_FETCH_JUCE "Download pinned JUCE via FetchContent if not installed" ON)

if (AERYTHER_BUILD_TESTS)
  enable_testing()
  add_executable(AerytherTests tests/CoreTests.cpp)
  target_include_directories(AerytherTests PRIVATE Source)
  add_test(NAME AerytherCore COMMAND AerytherTests)
endif()

if (AERYTHER_BUILD_PLUGIN)
  find_package(JUCE 8 CONFIG QUIET)
  if (NOT JUCE_FOUND)
    if (AERYTHER_FETCH_JUCE)
      include(FetchContent)
      FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.8 GIT_SHALLOW TRUE)
      FetchContent_MakeAvailable(JUCE)
    else()
      message(FATAL_ERROR "JUCE not found. Set JUCE_DIR or turn on AERYTHER_FETCH_JUCE.")
    endif()
  endif()

  if(APPLE)
    set(AERYTHER_FORMATS VST3 AU Standalone)
  else()
    set(AERYTHER_FORMATS VST3 Standalone)
  endif()

  juce_add_plugin(Aeryther
    COMPANY_NAME "Volodymyr Gnatenko"
    BUNDLE_ID "com.volodymyrgnatenko.aeryther"
    PLUGIN_MANUFACTURER_CODE Vgnt
    PLUGIN_CODE Aery
    PRODUCT_NAME "Aeryther"
    PLUGIN_NAME "Aeryther"
    DESCRIPTION "Aeryther - 3 Layer Ambient Instrument by Volodymyr Gnatenko"
    FORMATS ${AERYTHER_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
  )
  juce_generate_juce_header(Aeryther)
  target_sources(Aeryther PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
  target_compile_definitions(Aeryther PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
  target_link_libraries(Aeryther PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

  juce_add_plugin(AerytherFX
    COMPANY_NAME "Volodymyr Gnatenko"
    BUNDLE_ID "com.volodymyrgnatenko.aerytherfx"
    PLUGIN_MANUFACTURER_CODE Vgnt
    PLUGIN_CODE Aefx
    PRODUCT_NAME "Aeryther FX"
    PLUGIN_NAME "Aeryther FX"
    DESCRIPTION "Aeryther FX - atmospheric processing by Volodymyr Gnatenko"
    FORMATS ${AERYTHER_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
  )
  juce_generate_juce_header(AerytherFX)
  target_sources(AerytherFX PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
  target_compile_definitions(AerytherFX PRIVATE AERYTHER_FX=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
  target_link_libraries(AerytherFX PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
endif()
