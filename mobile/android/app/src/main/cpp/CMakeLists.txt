cmake_minimum_required(VERSION 3.22)
project(xtrans_mobile CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# mobile/android/app/src/main/cpp -> repository root
get_filename_component(XTRANS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../.." ABSOLUTE)
message(STATUS "xtrans root: ${XTRANS_ROOT}")

# whisper.cpp is the same pinned tree the Pi uses.
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory("${XTRANS_ROOT}/third_party/whisper.cpp" whisper EXCLUDE_FROM_ALL)

# The hardware-free core plus the two real adapters. ALSA, GPIO, Piper and the
# ORT delay loader are excluded: there is no system protobuf to collide with on
# Android, and the phone never synthesises with Piper.
set(XTRANS_SOURCES
  "${XTRANS_ROOT}/src/audio.cpp"
  "${XTRANS_ROOT}/src/audio_convert.cpp"
  "${XTRANS_ROOT}/src/bpe.cpp"
  "${XTRANS_ROOT}/src/language.cpp"
  "${XTRANS_ROOT}/src/nllb_ids.cpp"
  "${XTRANS_ROOT}/src/pipeline.cpp"
  "${XTRANS_ROOT}/src/recorder.cpp"
  "${XTRANS_ROOT}/src/session.cpp"
  "${XTRANS_ROOT}/src/text.cpp"
  "${XTRANS_ROOT}/src/adapters/whisper.cpp"
  "${XTRANS_ROOT}/src/adapters/nllb.cpp"
)

add_library(xtrans_core STATIC ${XTRANS_SOURCES})
target_include_directories(xtrans_core PUBLIC
  "${XTRANS_ROOT}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/ort/include"
)
target_compile_definitions(xtrans_core PUBLIC
  XTRANS_WITH_NLLB=1
  XTRANS_NLLB_TOKENIZER_BPE=1
  XTRANS_WITH_WHISPER=1
  XTRANS_WITH_ALSA=0
  XTRANS_WITH_GPIOD=0
  XTRANS_WITH_PIPER=0
)
target_compile_options(xtrans_core PRIVATE -Wall -Wextra -fno-exceptions -fno-rtti)
# Both adapters use libraries that throw; catch at the boundary, as on the Pi.
set_source_files_properties(
  "${XTRANS_ROOT}/src/adapters/nllb.cpp"
  "${XTRANS_ROOT}/src/adapters/whisper.cpp"
  PROPERTIES COMPILE_OPTIONS "-fexceptions"
)
target_link_libraries(xtrans_core PUBLIC whisper)

# ONNX Runtime from the pinned AAR (scripts/fetch_ort_android.sh).
add_library(onnxruntime SHARED IMPORTED)
set_target_properties(onnxruntime PROPERTIES
  IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}/libonnxruntime.so")

find_library(log-lib log)

add_library(xtrans SHARED jni_bridge.cpp)
target_compile_options(xtrans PRIVATE -Wall -Wextra -fexceptions -frtti)
target_link_libraries(xtrans
  xtrans_core
  onnxruntime
  ${log-lib}
)
