# Interface-only config for Debian's libonnxruntime-dev.
#
# We deliberately do NOT create an imported shared library: linking
# libonnxruntime.so puts ORT's bundled protobuf into the global symbol
# namespace at startup, which crashes libsentencepiece's descriptor
# registration (protobuf 3.14 vs 3.21). Instead, the executable provides
# OrtGetApiBase() and dlopen()s ORT with RTLD_DEEPBIND (see
# src/adapters/ort_delay_load.cpp). Targets only need the headers.
if(NOT TARGET onnxruntime::onnxruntime)
  find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h PATH_SUFFIXES onnxruntime)
  if(NOT ONNXRUNTIME_INCLUDE_DIR)
    message(FATAL_ERROR "onnxruntimeConfig: libonnxruntime-dev headers not found")
  endif()
  add_library(onnxruntime::onnxruntime INTERFACE IMPORTED)
  set_target_properties(onnxruntime::onnxruntime PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_INCLUDE_DIR}")
endif()
