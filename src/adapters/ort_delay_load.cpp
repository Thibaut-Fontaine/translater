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
    // Debian's arm64 runtime package installs libonnxruntime.so.1.21 without
    // the .so.1 / .so names; the -dev package adds .so. Try every spelling we
    // have seen rather than depending on a packaging detail.
    const char* candidates[] = {"libonnxruntime.so.1", "libonnxruntime.so.1.21",
                                "libonnxruntime.so"};
    void* handle = nullptr;
    for (const char* name : candidates) {
      handle = dlopen(name, RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
      if (handle != nullptr) break;
    }
    if (handle == nullptr) return nullptr;

    using Fn = const OrtApiBase* (*)();
    auto fn = reinterpret_cast<Fn>(dlsym(handle, "OrtGetApiBase"));
    return fn == nullptr ? nullptr : fn();
  }();
  return base;
}
