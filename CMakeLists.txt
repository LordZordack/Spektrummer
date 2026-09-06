cmake_minimum_required(VERSION 4.2)

project(Spektrummer VERSION 0.1.0 LANGUAGES C CXX)

if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE is not available. Initialise the pinned submodule with:\n"
        "  git submodule update --init --recursive")
endif()

add_subdirectory(external/JUCE)

juce_add_plugin(Spektrummer
    COMPANY_NAME "LordZordack"
    BUNDLE_ID "com.lordzordack.spektrummer"
    PLUGIN_MANUFACTURER_CODE LZrd
    PLUGIN_CODE Spkt
    PRODUCT_NAME "Spektrummer"
    VERSION "0.1.0"
    FORMATS VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_COPY_DIR "$ENV{LOCALAPPDATA}/Programs/Common/VST3")

target_sources(Spektrummer
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginProcessor.h
        src/PluginEditor.cpp
        src/PluginEditor.h
        src/model/ExpressionProgram.cpp
        src/model/ExpressionProgram.h
        src/analyzer/AnalyzerSampleFifo.h
        src/dsp/SpectralSynthEngine.cpp
        src/dsp/SpectralSynthEngine.h
        src/dsp/LockFreeFft.h
        src/dsp/SynthMidiEvent.h
        src/ui/SpectrumComponent.cpp
        src/ui/SpectrumComponent.h
        src/ui/EnvelopeComponent.cpp
        src/ui/EnvelopeComponent.h)

target_compile_features(Spektrummer PRIVATE cxx_std_20)

target_include_directories(Spektrummer
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_compile_definitions(Spektrummer
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Spektrummer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

juce_generate_juce_header(Spektrummer)

include(CTest)

if (BUILD_TESTING)
    juce_add_console_app(SpektrummerTests
        PRODUCT_NAME "Spektrummer Tests")

    target_sources(SpektrummerTests
        PRIVATE
            tests/ExpressionProgramTests.cpp
            tests/SpectralPresetModelTests.cpp
            tests/TestAllocationCounter.cpp
            tests/TestAllocationCounter.h
            tests/PluginShellTests.cpp
            tests/SpectralSynthEngineTests.cpp
            tests/AnalyzerSampleFifoTests.cpp
            tests/SpectrumComponentTests.cpp
            tests/EnvelopeComponentTests.cpp
            tests/LockFreeFftTests.cpp
            tests/SynthMidiEventTests.cpp
            src/PluginProcessor.cpp
            src/PluginProcessor.h
            src/PluginEditor.cpp
            src/PluginEditor.h
            src/model/ExpressionProgram.cpp
            src/model/ExpressionProgram.h
            src/analyzer/AnalyzerSampleFifo.h
            src/dsp/SpectralSynthEngine.cpp
            src/dsp/SpectralSynthEngine.h
            src/dsp/LockFreeFft.h
            src/dsp/SynthMidiEvent.h
            src/ui/SpectrumComponent.cpp
            src/ui/SpectrumComponent.h
            src/ui/EnvelopeComponent.cpp
            src/ui/EnvelopeComponent.h)

    target_compile_features(SpektrummerTests PRIVATE cxx_std_20)

    target_include_directories(SpektrummerTests
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src")

    target_compile_definitions(SpektrummerTests
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(SpektrummerTests
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    juce_generate_juce_header(SpektrummerTests)

    add_test(NAME SpektrummerTests COMMAND SpektrummerTests)
endif()
