// Delay-loads ONNX Runtime for the whole binary.
//
// ORT statically embeds protobuf 3.14; libsentencepiece links the system
// protobuf 3.21. Loading both normally aborts before main() on a version check,
// or at the first encode. Defining OrtGetApiBase() here (the only symbol the
// ORT C++ header needs) and opening the library with RTLD_DEEPBIND keeps ORT's
// copy private. See docs/nllb-onnx.md.
#include <dlfcn.h>

#include <onnxruntime/onnxruntime_cxx_api.h>

extern "C" const OrtApiBase* OrtGetApiBase(void) {
  static const OrtApiBase* base = []() -> const OrtApiBase* {
    // Debian ships the unversioned soname in libonnxruntime-dev; the runtime
    // package may only install the versioned one.
    void* handle = dlopen("libonnxruntime.so.1", RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
    if (handle == nullptr) {
      handle = dlopen("libonnxruntime.so", RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
    }
    if (handle == nullptr) return nullptr;

    using Fn = const OrtApiBase* (*)();
    auto fn = reinterpret_cast<Fn>(dlsym(handle, "OrtGetApiBase"));
    return fn == nullptr ? nullptr : fn();
  }();
  return base;
}
