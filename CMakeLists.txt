cmake_minimum_required(VERSION 3.22)
project(xtrans VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
# Debian ships libonnxruntime-dev without a CMake package config; ours defines
# onnxruntime::onnxruntime so third-party projects use the distro library.
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(XTRANS_WERROR "Treat compiler warnings as errors" ON)
option(XTRANS_NATIVE "Tune for the build host (never for release artifacts)" OFF)
set(XTRANS_SANITIZE "" CACHE STRING "Comma separated sanitizers, e.g. address,undefined")

# Coverage build type: instrumented, unoptimised, used for the 100% rule.
set(CMAKE_CXX_FLAGS_COVERAGE "-O0 -g --coverage" CACHE STRING "Flags for Coverage builds")
set(CMAKE_EXE_LINKER_FLAGS_COVERAGE "--coverage")
mark_as_advanced(CMAKE_CXX_FLAGS_COVERAGE CMAKE_EXE_LINKER_FLAGS_COVERAGE)

# No exceptions, no RTTI: errors are returned. Applied to every target.
set(XTRANS_CXX_WARNINGS -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
if(XTRANS_WERROR)
  list(APPEND XTRANS_CXX_WARNINGS -Werror)
endif()

# Optional OS adapters. Found -> built by default; missing headers keep the host
# test build dependency-free.
find_path(XTRANS_ALSA_INCLUDE_DIR alsa/asoundlib.h)
find_library(XTRANS_ALSA_LIBRARY asound)
find_path(XTRANS_GPIOD_INCLUDE_DIR gpiod.h)
find_library(XTRANS_GPIOD_LIBRARY gpiod)
find_path(XTRANS_ORT_INCLUDE_DIR onnxruntime/onnxruntime_cxx_api.h)
find_library(XTRANS_SENTENCEPIECE_LIBRARY sentencepiece)
find_package(Threads REQUIRED)

if(XTRANS_ALSA_INCLUDE_DIR AND XTRANS_ALSA_LIBRARY)
  set(XTRANS_ALSA_DEFAULT ON)
else()
  set(XTRANS_ALSA_DEFAULT OFF)
endif()
if(XTRANS_GPIOD_INCLUDE_DIR AND XTRANS_GPIOD_LIBRARY)
  set(XTRANS_GPIOD_DEFAULT ON)
else()
  set(XTRANS_GPIOD_DEFAULT OFF)
endif()

if(XTRANS_ORT_INCLUDE_DIR AND XTRANS_SENTENCEPIECE_LIBRARY)
  set(XTRANS_NLLB_DEFAULT ON)
else()
  set(XTRANS_NLLB_DEFAULT OFF)
endif()

option(XTRANS_WITH_ALSA "Build the ALSA capture/playback adapters" ${XTRANS_ALSA_DEFAULT})
option(XTRANS_WITH_GPIOD "Build the libgpiod button adapter" ${XTRANS_GPIOD_DEFAULT})
option(XTRANS_WITH_NLLB "Build the NLLB translation adapter" ${XTRANS_NLLB_DEFAULT})


add_library(xtrans STATIC
  src/audio.cpp
  src/audio_convert.cpp
  src/buttons.cpp
  src/control.cpp
  src/device.cpp
  src/language.cpp
  src/nllb_ids.cpp
  src/pipeline.cpp
  src/recorder.cpp
  src/text.cpp
  src/wav.cpp
)

target_include_directories(xtrans PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_options(xtrans PUBLIC
  ${XTRANS_CXX_WARNINGS}
  -fno-exceptions
  -fno-rtti
  "$<$<CONFIG:Release>:-O3>"
)

if(XTRANS_WITH_ALSA)
  target_sources(xtrans PRIVATE src/adapters/alsa.cpp)
  target_include_directories(xtrans PRIVATE "${XTRANS_ALSA_INCLUDE_DIR}")
  target_link_libraries(xtrans PUBLIC "${XTRANS_ALSA_LIBRARY}" Threads::Threads)
  target_compile_definitions(xtrans PUBLIC XTRANS_WITH_ALSA=1)
  message(STATUS "ALSA adapters: ${XTRANS_ALSA_LIBRARY}")
else()
  target_compile_definitions(xtrans PUBLIC XTRANS_WITH_ALSA=0)
  message(STATUS "ALSA adapters: disabled (libasound2-dev not found)")
endif()

if(XTRANS_WITH_GPIOD)
  target_sources(xtrans PRIVATE src/adapters/gpiod.cpp)
  target_include_directories(xtrans PRIVATE "${XTRANS_GPIOD_INCLUDE_DIR}")
  target_link_libraries(xtrans PUBLIC "${XTRANS_GPIOD_LIBRARY}")
  target_compile_definitions(xtrans PUBLIC XTRANS_WITH_GPIOD=1)
  message(STATUS "GPIO adapter: ${XTRANS_GPIOD_LIBRARY}")
else()
  target_compile_definitions(xtrans PUBLIC XTRANS_WITH_GPIOD=0)
  message(STATUS "GPIO adapter: disabled (libgpiod-dev not found)")
endif()

if(XTRANS_WITH_NLLB)
  target_sources(xtrans PRIVATE src/adapters/nllb.cpp)
  target_include_directories(xtrans PRIVATE "${XTRANS_ORT_INCLUDE_DIR}")
  target_link_libraries(xtrans PUBLIC "${XTRANS_SENTENCEPIECE_LIBRARY}" ${CMAKE_DL_LIBS})
  target_compile_definitions(xtrans PUBLIC XTRANS_WITH_NLLB=1)
  # ONNX Runtime's C++ API throws. This is the only place exceptions are
  # enabled; the adapter catches at its boundary and returns MtStatus.
  set_source_files_properties(src/adapters/nllb.cpp PROPERTIES COMPILE_OPTIONS "-fexceptions")
  message(STATUS "NLLB adapter: enabled (ONNX Runtime from ${XTRANS_ORT_INCLUDE_DIR})")
else()
  target_compile_definitions(xtrans PUBLIC XTRANS_WITH_NLLB=0)
  message(STATUS "NLLB adapter: disabled (libonnxruntime-dev / libsentencepiece-dev not found)")
endif()

# Anything that uses ONNX Runtime headers must go through our delay loader.
# The ORT loader must be an object file in every executable: a static archive
# is scanned before libpiper.so asks for OrtGetApiBase, so the symbol would
# stay undefined at link time.
if(XTRANS_WITH_NLLB OR XTRANS_WITH_PIPER)
  add_library(xtrans_ort_loader OBJECT src/adapters/ort_delay_load.cpp)
  target_include_directories(xtrans_ort_loader PRIVATE "${XTRANS_ORT_INCLUDE_DIR}")
  set_target_properties(xtrans_ort_loader PROPERTIES
    COMPILE_OPTIONS "-fexceptions")
  add_library(xtrans::ort_loader ALIAS xtrans_ort_loader)
endif()

set(XTRANS_PIPER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/piper")
if(EXISTS "${XTRANS_PIPER_DIR}/libpiper/CMakeLists.txt" AND XTRANS_ORT_INCLUDE_DIR)
  set(XTRANS_PIPER_DEFAULT ON)
else()
  set(XTRANS_PIPER_DEFAULT OFF)
endif()
option(XTRANS_WITH_PIPER "Build the Piper TTS adapter" ${XTRANS_PIPER_DEFAULT})

if(XTRANS_WITH_PIPER)
  set(PIPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  # libpiper builds espeak-ng and open_jtalk from pinned git commits at build
  # time; that is a build-machine concern, never the device.
  add_subdirectory("${XTRANS_PIPER_DIR}/libpiper" "${CMAKE_BINARY_DIR}/piper" EXCLUDE_FROM_ALL)
  target_sources(xtrans PRIVATE src/adapters/piper.cpp)
  target_link_libraries(xtrans PRIVATE piper)
  # Piper's C++ can throw (nlohmann/json, ONNX Runtime); catch at the boundary.
  set_source_files_properties(src/adapters/piper.cpp PROPERTIES COMPILE_OPTIONS "-fexceptions")
  target_compile_definitions(xtrans PUBLIC XTRANS_WITH_PIPER=1)
  message(STATUS "Piper adapter: enabled")
else()
  target_compile_definitions(xtrans PUBLIC XTRANS_WITH_PIPER=0)
  message(STATUS "Piper adapter: disabled (run scripts/fetch_third_party.sh)")
endif()

# whisper.cpp STT adapter. The tree is vendored by scripts/fetch_third_party.sh
# and gitignored; the pin file is the source of truth.
set(XTRANS_WHISPER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/whisper.cpp")
if(EXISTS "${XTRANS_WHISPER_DIR}/CMakeLists.txt")
  set(XTRANS_WHISPER_DEFAULT ON)
else()
  set(XTRANS_WHISPER_DEFAULT OFF)
endif()
option(XTRANS_WITH_WHISPER "Build the whisper.cpp STT adapter" ${XTRANS_WHISPER_DEFAULT})

if(XTRANS_WITH_WHISPER)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
  set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
  add_subdirectory("${XTRANS_WHISPER_DIR}" EXCLUDE_FROM_ALL)

  target_sources(xtrans PRIVATE src/adapters/whisper.cpp)
  target_link_libraries(xtrans PRIVATE whisper)
  target_compile_definitions(xtrans PUBLIC XTRANS_WITH_WHISPER=1)
  message(STATUS "whisper.cpp adapter: enabled")
else()
  target_compile_definitions(xtrans PUBLIC XTRANS_WITH_WHISPER=0)
  message(STATUS "whisper.cpp adapter: disabled (run scripts/fetch_third_party.sh)")
endif()

# LTO only for optimised builds; debugging must stay fast to link.
set_property(TARGET xtrans PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
set_property(TARGET xtrans PROPERTY INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE)

if(XTRANS_NATIVE)
  target_compile_options(xtrans PUBLIC -march=native)
endif()

if(XTRANS_SANITIZE)
  target_compile_options(xtrans PUBLIC -fsanitize=${XTRANS_SANITIZE} -fno-omit-frame-pointer)
  target_link_options(xtrans PUBLIC -fsanitize=${XTRANS_SANITIZE})
endif()

enable_testing()

add_executable(xtransd apps/xtransd.cpp)
target_link_libraries(xtransd PRIVATE xtrans)
if(TARGET xtrans_ort_loader)
  target_sources(xtransd PRIVATE $<TARGET_OBJECTS:xtrans_ort_loader>)
endif()
install(TARGETS xtransd RUNTIME DESTINATION bin)

add_executable(xtrans-mt apps/xtrans-mt.cpp)
target_link_libraries(xtrans-mt PRIVATE xtrans)
if(TARGET xtrans_ort_loader)
  target_sources(xtrans-mt PRIVATE $<TARGET_OBJECTS:xtrans_ort_loader>)
endif()
install(TARGETS xtrans-mt RUNTIME DESTINATION bin)

add_executable(xtrans-tts apps/xtrans-tts.cpp)
target_link_libraries(xtrans-tts PRIVATE xtrans)
if(TARGET xtrans_ort_loader)
  target_sources(xtrans-tts PRIVATE $<TARGET_OBJECTS:xtrans_ort_loader>)
endif()
install(TARGETS xtrans-tts RUNTIME DESTINATION bin)

option(XTRANS_BUILD_TESTS "Build the unit tests (host only)" ON)

if(XTRANS_BUILD_TESTS)
  add_executable(xtrans_tests
    tests/main.cpp
    tests/test_audio.cpp
    tests/test_convert.cpp
    tests/test_buttons.cpp
    tests/test_control.cpp
    tests/test_device.cpp
    tests/test_language.cpp
    tests/test_nllb.cpp
    tests/test_pipeline.cpp
    tests/test_recorder.cpp
    tests/test_text.cpp
    tests/test_wav.cpp
    tests/test_watchdog.cpp
  )
  target_link_libraries(xtrans_tests PRIVATE xtrans)

  add_test(NAME unit COMMAND xtrans_tests)
endif()
