# Minimal CMake config for Debian's libonnxruntime-dev, which installs headers
# and a library but no package config. Piper and our NLLB adapter both need the
# onnxruntime::onnxruntime target; without this they download a vendor build.
if(NOT TARGET onnxruntime::onnxruntime)
  find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h PATH_SUFFIXES onnxruntime)
  find_library(ONNXRUNTIME_LIBRARY onnxruntime)
  if(NOT ONNXRUNTIME_INCLUDE_DIR OR NOT ONNXRUNTIME_LIBRARY)
    message(FATAL_ERROR "onnxruntimeConfig: libonnxruntime-dev not found")
  endif()
  add_library(onnxruntime::onnxruntime SHARED IMPORTED)
  set_target_properties(onnxruntime::onnxruntime PROPERTIES
    IMPORTED_LOCATION "${ONNXRUNTIME_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_INCLUDE_DIR}")
endif()
