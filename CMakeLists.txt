cmake_minimum_required(VERSION 3.22)

project(ExpressiveComposer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(EC_BUILD_PLUGIN "Build the JUCE VST3/Standalone plugin" OFF)
option(EC_BUILD_CLAP "Build the CLAP wrapper through clap-juce-extensions" OFF)
option(EC_BUILD_TESTS "Build the automated test suite" ON)
option(EC_BUILD_BENCH "Build the render benchmark tool" ON)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /Zc:__cplusplus)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

set(EC_CORE_SOURCES
    src/core/Types.cpp
    src/core/Json.cpp
    src/core/TimeMap.cpp
    src/core/Expression.cpp
    src/core/Theory.cpp
    src/core/Motif.cpp
    src/core/ProjectIO.cpp
    src/core/Composer.cpp
    src/core/Humanizer.cpp
)

add_library(ec_core STATIC ${EC_CORE_SOURCES})
target_include_directories(ec_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(ec_core PUBLIC cxx_std_20)

add_library(ec_io STATIC
    src/io/MidiFile.cpp
    src/io/WavWriter.cpp
)
target_link_libraries(ec_io PUBLIC ec_core)
target_include_directories(ec_io PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(ec_dsp STATIC
    src/dsp/Synth.cpp
    src/dsp/Effects.cpp
)
target_link_libraries(ec_dsp PUBLIC ec_core)
target_include_directories(ec_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(ec_render STATIC
    src/render/Renderer.cpp
)
target_link_libraries(ec_render PUBLIC ec_core ec_io ec_dsp)
target_include_directories(ec_render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(ec_cli src/cli/main.cpp)
target_link_libraries(ec_cli PRIVATE ec_render)

if(EC_BUILD_PLUGIN)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE/CMakeLists.txt")
        message(FATAL_ERROR "JUCE not found at third_party/JUCE. See README for dependency setup.")
    endif()
    add_subdirectory(third_party/JUCE)
    if(EC_BUILD_CLAP AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/clap-juce-extensions/CMakeLists.txt")
        add_subdirectory(third_party/clap-juce-extensions EXCLUDE_FROM_ALL)
    endif()
    add_subdirectory(src/plugin)
endif()

if(EC_BUILD_TESTS)
    enable_testing()
    add_executable(ec_tests
        tests/test_main.cpp
        tests/test_core.cpp
        tests/test_project.cpp
        tests/test_midi.cpp
        tests/test_dsp.cpp
    )
    target_link_libraries(ec_tests PRIVATE ec_core ec_io ec_dsp ec_render)
    target_include_directories(ec_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(NAME ec_tests COMMAND ec_tests)
endif()

if(EC_BUILD_BENCH)
    add_executable(ec_bench tests/bench_main.cpp)
    target_link_libraries(ec_bench PRIVATE ec_core ec_io ec_dsp ec_render)
endif()

install(TARGETS ec_core ARCHIVE DESTINATION lib)
