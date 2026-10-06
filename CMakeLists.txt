cmake_minimum_required(VERSION 3.22)
# A versão pode ser sobrescrita pelo CI: cmake -DHUSHRIG_VERSION=0.2.0
set(HUSHRIG_VERSION "0.1.0" CACHE STRING "Versão do HushRig (major.minor.patch)")
project(HushRig VERSION ${HUSHRIG_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW ON)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE Catch2)

# --- Plugin (Standalone + VST3) -------------------------------------------
juce_add_plugin(HushRig
    PRODUCT_NAME "HushRig"
    COMPANY_NAME "HushRig"
    PLUGIN_MANUFACTURER_CODE Hshr
    PLUGIN_CODE Hr01
    FORMATS Standalone VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(HushRig PRIVATE
    src/PluginProcessor.cpp
    src/PluginProcessor.h
    src/PluginEditor.cpp
    src/PluginEditor.h
    src/DeviceInfo.cpp
    src/DeviceInfo.h
    src/ui/HushLookAndFeel.h
    src/ui/LevelMeter.h
    src/update/Updater.cpp
    src/update/Updater.h
    src/update/Sha256.h
    src/update/VersionCompare.h)

# A raiz dos módulos dá acesso a juce_audio_plugin_client (StandalonePluginHolder, usado em DeviceInfo.cpp)
target_include_directories(HushRig PRIVATE src ${juce_SOURCE_DIR}/modules)

# Fontes em UTF-8 (textos em português com acento)
target_compile_options(HushRig PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_compile_definitions(HushRig PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(HushRig
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Ferramenta de screenshots (usada pelo CI para o README) ---------------
juce_add_console_app(HushRigShots PRODUCT_NAME "HushRigShots")

target_sources(HushRigShots PRIVATE
    tools/Screenshots.cpp
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/DeviceInfo.cpp
    src/update/Updater.cpp)

target_include_directories(HushRigShots PRIVATE src)
target_compile_options(HushRigShots PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_compile_definitions(HushRigShots PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    HUSHRIG_NO_STANDALONE_HOLDER=1
    JucePlugin_VersionString="${PROJECT_VERSION}")

target_link_libraries(HushRigShots
    PRIVATE
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# --- Testes do DSP (sem dependência do JUCE) -------------------------------
enable_testing()
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

add_executable(hushrig_tests tests/NoiseGateTests.cpp tests/VersionCompareTests.cpp tests/Sha256Tests.cpp)
target_compile_options(hushrig_tests PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_include_directories(hushrig_tests PRIVATE src)
target_link_libraries(hushrig_tests PRIVATE Catch2::Catch2WithMain)
catch_discover_tests(hushrig_tests)
