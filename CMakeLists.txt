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
# NeuralAmpModelerCore (MIT): baixado so os fontes (o CMake deles e feito para app proprio,
# entao criamos a biblioteca `nam_core` aqui).
# O Eigen e o mesmo commit que o NAM fixa como submodulo (bc3b398), mas vem do espelho oficial no
# GitHub: o gitlab.com recusa clones sob carga e derrubou o CI ("unable to handle this request due to load").
FetchContent_Declare(NAMCore
    GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
    GIT_TAG 0b3d3c97b0859a3a8c92a8628c4dd89a25eb5842
    GIT_SUBMODULES "" # nenhum submodulo (Eigen abaixo; nlohmann/json ja vem versionado no repositorio)
    SOURCE_SUBDIR nao-existe)
FetchContent_Declare(EigenSrc
    GIT_REPOSITORY https://github.com/eigen-mirror/eigen.git
    GIT_TAG bc3b39870ecb690a623a3f49149a358b95c5781d
    SOURCE_SUBDIR nao-existe)
FetchContent_MakeAvailable(JUCE Catch2 NAMCore EigenSrc)

file(GLOB NAM_CORE_SOURCES CONFIGURE_DEPENDS
    "${namcore_SOURCE_DIR}/NAM/*.cpp"
    "${namcore_SOURCE_DIR}/NAM/*/*.cpp")
# OBJECT (nao STATIC): o NAM registra WaveNet/LSTM/etc. por inicializadores estaticos, e o linker
# descartaria esses objetos de uma biblioteca estatica ("No config parser registered for architecture").
add_library(nam_core OBJECT ${NAM_CORE_SOURCES})
set_target_properties(nam_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(nam_core PUBLIC "${namcore_SOURCE_DIR}")
target_include_directories(nam_core SYSTEM PUBLIC
    "${eigensrc_SOURCE_DIR}"
    "${namcore_SOURCE_DIR}/Dependencies/nlohmann")
target_compile_features(nam_core PUBLIC cxx_std_20)
target_compile_definitions(nam_core PUBLIC NAM_ENABLE_A2_FAST)
# No Windows os headers do NAM precisam disto; fica fora do PUBLIC para nao colidir com as definicoes do JUCE.
target_compile_definitions(nam_core PRIVATE $<$<PLATFORM_ID:Windows>:NOMINMAX WIN32_LEAN_AND_MEAN>)
set_source_files_properties(src/amp/NamAmp.cpp PROPERTIES
    COMPILE_DEFINITIONS "$<$<PLATFORM_ID:Windows>:NOMINMAX;WIN32_LEAN_AND_MEAN>")
target_compile_options(nam_core PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w>)

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
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png"
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(HushRig PRIVATE
    src/PluginProcessor.cpp
    src/PluginProcessor.h
    src/PluginEditor.cpp
    src/PluginEditor.h
    src/DeviceInfo.cpp
    src/DeviceInfo.h
    src/amp/NamAmp.cpp
    src/amp/NamAmp.h
    src/PresetManager.cpp
    src/PresetManager.h
    src/ui/AmpPanel.h
    src/ui/HushLookAndFeel.h
    src/ui/LevelMeter.h
    src/ui/PedalBoard.h
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
        nam_core
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
    src/PresetManager.cpp
    src/amp/NamAmp.cpp
    src/update/Updater.cpp)

target_include_directories(HushRigShots PRIVATE src)
target_compile_options(HushRigShots PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_compile_definitions(HushRigShots PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    HUSHRIG_NO_STANDALONE_HOLDER=1
    HUSHRIG_SAMPLE_MODEL="${CMAKE_CURRENT_SOURCE_DIR}/tests/data/wavenet.nam"
    JucePlugin_VersionString="${PROJECT_VERSION}")

target_link_libraries(HushRigShots
    PRIVATE
        juce::juce_audio_utils
        nam_core
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# --- Benchmark de modelos NAM (sem JUCE): hushrig_ampbench <modelo.nam> -------
add_executable(hushrig_ampbench tools/AmpBench.cpp src/amp/NamAmp.cpp)
target_include_directories(hushrig_ampbench PRIVATE src)
target_link_libraries(hushrig_ampbench PRIVATE nam_core)

# --- Testes do DSP (sem dependência do JUCE) -------------------------------
enable_testing()
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

add_executable(hushrig_tests tests/NoiseGateTests.cpp tests/VersionCompareTests.cpp tests/Sha256Tests.cpp tests/PedalTests.cpp tests/ChainOrderTests.cpp
    tests/NamAmpTests.cpp src/amp/NamAmp.cpp)
target_compile_options(hushrig_tests PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_include_directories(hushrig_tests PRIVATE src)
target_compile_definitions(hushrig_tests PRIVATE HUSHRIG_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
target_link_libraries(hushrig_tests PRIVATE Catch2::Catch2WithMain nam_core)
catch_discover_tests(hushrig_tests)
