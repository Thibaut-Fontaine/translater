cmake_minimum_required(VERSION 3.22)
project(xtrans VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

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

add_library(xtrans STATIC
  src/audio.cpp
  src/buttons.cpp
  src/control.cpp
  src/device.cpp
  src/language.cpp
  src/pipeline.cpp
  src/recorder.cpp
)

target_include_directories(xtrans PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_options(xtrans PUBLIC
  ${XTRANS_CXX_WARNINGS}
  -fno-exceptions
  -fno-rtti
  "$<$<CONFIG:Release>:-O3>"
)

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
install(TARGETS xtransd RUNTIME DESTINATION bin)

option(XTRANS_BUILD_TESTS "Build the unit tests (host only)" ON)

if(XTRANS_BUILD_TESTS)
  add_executable(xtrans_tests
    tests/main.cpp
    tests/test_audio.cpp
    tests/test_buttons.cpp
    tests/test_control.cpp
    tests/test_device.cpp
    tests/test_language.cpp
    tests/test_pipeline.cpp
    tests/test_recorder.cpp
  )
  target_link_libraries(xtrans_tests PRIVATE xtrans)

  add_test(NAME unit COMMAND xtrans_tests)
endif()
