// JNI boundary between the Kotlin TurboModule and the shared C++ core.
//
// Rules encoded here:
//   * audio arrives as a short[] captured by Kotlin, never from JS;
//   * every Session call happens on the module's single worker thread, so the
//     core stays single-threaded;
//   * the polling getters read a snapshot guarded by a mutex, never the live
//     session, so a long translation cannot block the UI thread.

#include <jni.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "xtrans/adapters/nllb.hpp"
#include "xtrans/adapters/whisper.hpp"
#include "xtrans/language.hpp"
#include "xtrans/session.hpp"

namespace {

std::string from_java(JNIEnv* env, jstring value) {
  if (value == nullptr) return {};
  const char* chars = env->GetStringUTFChars(value, nullptr);
  std::string out = chars != nullptr ? chars : "";
  if (chars != nullptr) env->ReleaseStringUTFChars(value, chars);
  return out;
}

// UTF-8 -> UTF-16. NewStringUTF expects modified UTF-8, which differs for
// supplementary characters (emoji survive a translation only through this).
jstring to_java(JNIEnv* env, std::string_view text) {
  std::u16string out;
  out.reserve(text.size());
  for (std::size_t i = 0; i < text.size();) {
    const auto c = static_cast<unsigned char>(text[i]);
    std::uint32_t cp = 0;
    std::size_t n = 1;
    if (c < 0x80) {
      cp = c;
    } else if ((c & 0xE0) == 0xC0 && i + 1 < text.size()) {
      cp = (c & 0x1FU) << 6 | (static_cast<unsigned char>(text[i + 1]) & 0x3FU);
      n = 2;
    } else if ((c & 0xF0) == 0xE0 && i + 2 < text.size()) {
      cp = (c & 0x0FU) << 12 | ((static_cast<unsigned char>(text[i + 1]) & 0x3FU) << 6) |
           (static_cast<unsigned char>(text[i + 2]) & 0x3FU);
      n = 3;
    } else if ((c & 0xF8) == 0xF0 && i + 3 < text.size()) {
      cp = (c & 0x07U) << 18 | ((static_cast<unsigned char>(text[i + 1]) & 0x3FU) << 12) |
           ((static_cast<unsigned char>(text[i + 2]) & 0x3FU) << 6) |
           (static_cast<unsigned char>(text[i + 3]) & 0x3FU);
      n = 4;
    } else {
      cp = 0xFFFD;
    }
    i += n;
    if (cp >= 0x10000) {
      cp -= 0x10000;
      out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
      out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
    } else {
      out.push_back(static_cast<char16_t>(cp));
    }
  }
  return env->NewString(reinterpret_cast<const jchar*>(out.data()),
                        static_cast<jsize>(out.size()));
}

bool file_exists(const std::string& path) {
  std::FILE* file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) return false;
  std::fclose(file);
  return true;
}

std::uintmax_t file_size(const std::string& path) {
  std::FILE* file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) return 0;
  std::fseek(file, 0, SEEK_END);
  const long size = std::ftell(file);
  std::fclose(file);
  return size > 0 ? static_cast<std::uintmax_t>(size) : 0;
}

bool any_whisper_model(const std::string& dir, std::string& path) {
  static const char* kNames[] = {"ggml-base-q5_1.bin", "ggml-base.bin"};
  for (const char* name : kNames) {
    path = dir + "/" + name;
    if (file_exists(path)) return true;
  }
  return false;
}

struct Bridge {
  std::string dir;
  std::unique_ptr<xtrans::WhisperStt> stt;
  std::unique_ptr<xtrans::NllbMt> mt;
  std::unique_ptr<xtrans::Session> session;
  bool ready = false;

  // Everything below is a snapshot for the polling getters.
  std::mutex mutex;
  int state = 0;
  std::string transcript;
  std::string translation;
  std::string last_error;

  void snapshot_locked() {
    if (!session) return;
    state = static_cast<int>(session->state());
    transcript.assign(session->transcript());
    translation.assign(session->translation());
  }

  void update() {
    std::lock_guard<std::mutex> lock(mutex);
    snapshot_locked();
  }

  static void on_error(void* ctx, const char* message) {
    auto* bridge = static_cast<Bridge*>(ctx);
    std::lock_guard<std::mutex> lock(bridge->mutex);
    bridge->last_error = message != nullptr ? message : "";
  }
};

std::mutex g_open_mutex;
std::string g_open_error;

int worker_threads() {
  const unsigned hw = std::thread::hardware_concurrency();
  if (hw == 0) return 4;
  return hw < 4U ? static_cast<int>(hw) : 4;
}

}  // namespace

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_xtrans_app_XtransModule_nativeOpen(JNIEnv* env, jobject, jstring models_dir,
                                             jboolean cache_sessions) {
  auto bridge = std::make_unique<Bridge>();
  bridge->dir = from_java(env, models_dir);

  std::string whisper_path;
  if (!any_whisper_model(bridge->dir, whisper_path)) {
    std::lock_guard<std::mutex> lock(g_open_mutex);
    g_open_error = "whisper model missing in " + bridge->dir;
    return 0;
  }

  std::string error;
  bridge->stt = xtrans::WhisperStt::open(whisper_path.c_str(), error);
  if (!bridge->stt) {
    std::lock_guard<std::mutex> lock(g_open_mutex);
    g_open_error = "whisper: " + error;
    return 0;
  }

  const std::string base = bridge->dir + "/";
  xtrans::NllbConfig config;
  const std::string encoder = base + "encoder_model_int8.onnx";
  const std::string decoder = base + "decoder_model_quantized.onnx";
  const std::string with_past = base + "decoder_with_past_model_quantized.onnx";
  const std::string tokenizer = base + "nllb_bpe.bin";
  config.encoder = encoder.c_str();
  config.decoder = decoder.c_str();
  config.decoder_with_past = with_past.c_str();
  config.tokenizer = tokenizer.c_str();
  config.threads = worker_threads();
  config.cache_sessions = cache_sessions == JNI_TRUE ? true : false;

  bridge->mt = xtrans::NllbMt::open(config, error);
  if (!bridge->mt) {
    std::lock_guard<std::mutex> lock(g_open_mutex);
    g_open_error = "nllb: " + error;
    return 0;
  }

  xtrans::SessionCallbacks callbacks;
  callbacks.ctx = bridge.get();
  callbacks.on_error = &Bridge::on_error;
  bridge->session =
      std::make_unique<xtrans::Session>(*bridge->stt, *bridge->mt, callbacks);
  bridge->ready = true;
  bridge->update();
  return reinterpret_cast<jlong>(bridge.release());
}

JNIEXPORT jstring JNICALL
Java_com_xtrans_app_XtransModule_nativeOpenError(JNIEnv* env, jobject) {
  std::lock_guard<std::mutex> lock(g_open_mutex);
  return to_java(env, g_open_error);
}

JNIEXPORT void JNICALL
Java_com_xtrans_app_XtransModule_nativeClose(JNIEnv*, jobject, jlong handle) {
  delete reinterpret_cast<Bridge*>(handle);
}

JNIEXPORT jboolean JNICALL
Java_com_xtrans_app_XtransModule_nativeReady(JNIEnv*, jobject, jlong handle) {
  const auto* bridge = reinterpret_cast<const Bridge*>(handle);
  return bridge != nullptr && bridge->ready ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_xtrans_app_XtransModule_nativeConfigure(JNIEnv*, jobject, jlong handle, jint from,
                                                 jint to, jint mode) {
  auto* bridge = reinterpret_cast<Bridge*>(handle);
  if (bridge == nullptr || !bridge->session) return JNI_FALSE;
  if (from < 0 || to < 0 || from >= static_cast<jint>(xtrans::kLangCount) ||
      to >= static_cast<jint>(xtrans::kLangCount) || mode < 0 || mode > 3) {
    return JNI_FALSE;
  }
  const bool ok = bridge->session->configure(static_cast<xtrans::Lang>(from),
                                             static_cast<xtrans::Lang>(to),
                                             static_cast<xtrans::Mode>(mode));
  bridge->update();
  return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_xtrans_app_XtransModule_nativeBeginVoice(JNIEnv*, jobject, jlong handle) {
  auto* bridge = reinterpret_cast<Bridge*>(handle);
  if (bridge == nullptr || !bridge->session) return JNI_FALSE;
  const bool ok = bridge->session->begin_voice();
  bridge->update();
  return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_xtrans_app_XtransModule_nativeFinishVoice(JNIEnv* env, jobject, jlong handle,
                                                   jshortArray samples) {
  auto* bridge = reinterpret_cast<Bridge*>(handle);
  if (bridge == nullptr || !bridge->session || samples == nullptr) return 1;

  const jsize count = env->GetArrayLength(samples);
  std::vector<std::int16_t> pcm(static_cast<std::size_t>(count));
  if (count > 0) {
    env->GetShortArrayRegion(samples, 0, count, reinterpret_cast<jshort*>(pcm.data()));
  }

  const auto status = bridge->session->finish_voice(pcm);
  bridge->update();
  return static_cast<jint>(status);
}

JNIEXPORT jint JNICALL
Java_com_xtrans_app_XtransModule_nativeTranslate(JNIEnv* env, jobject, jlong handle,
                                                 jstring text) {
  auto* bridge = reinterpret_cast<Bridge*>(handle);
  if (bridge == nullptr || !bridge->session) return 1;
  const std::string input = from_java(env, text);
  const auto status = bridge->session->translate_text(input);
  bridge->update();
  return static_cast<jint>(status);
}

JNIEXPORT void JNICALL
Java_com_xtrans_app_XtransModule_nativeCancel(JNIEnv*, jobject, jlong handle) {
  auto* bridge = reinterpret_cast<Bridge*>(handle);
  if (bridge == nullptr || !bridge->session) return;
  bridge->session->cancel();
  bridge->update();
}

JNIEXPORT jint JNICALL
Java_com_xtrans_app_XtransModule_nativeState(JNIEnv*, jobject, jlong handle) {
  auto* bridge = reinterpret_cast<Bridge*>(handle);
  if (bridge == nullptr) return 0;
  std::lock_guard<std::mutex> lock(bridge->mutex);
  return bridge->state;
}

JNIEXPORT jstring JNICALL
Java_com_xtrans_app_XtransModule_nativeTranscript(JNIEnv* env, jobject, jlong handle) {
  auto* bridge = reinterpret_cast<Bridge*>(handle);
  if (bridge == nullptr) return to_java(env, {});
  std::lock_guard<std::mutex> lock(bridge->mutex);
  return to_java(env, bridge->transcript);
}

JNIEXPORT jstring JNICALL
Java_com_xtrans_app_XtransModule_nativeTranslation(JNIEnv* env, jobject, jlong handle) {
  auto* bridge = reinterpret_cast<Bridge*>(handle);
  if (bridge == nullptr) return to_java(env, {});
  std::lock_guard<std::mutex> lock(bridge->mutex);
  return to_java(env, bridge->translation);
}

JNIEXPORT jstring JNICALL
Java_com_xtrans_app_XtransModule_nativeLastError(JNIEnv* env, jobject, jlong handle) {
  auto* bridge = reinterpret_cast<Bridge*>(handle);
  if (bridge == nullptr) return to_java(env, {});
  std::lock_guard<std::mutex> lock(bridge->mutex);
  return to_java(env, bridge->last_error);
}

// --- model inspection, used by the Models screen ---------------------------

namespace {
const char* kModelNames[] = {
    "encoder_model_int8.onnx",
    "decoder_model_quantized.onnx",
    "decoder_with_past_model_quantized.onnx",
    "nllb_bpe.bin",
    "ggml-base-q5_1.bin",
};
}  // namespace

JNIEXPORT jstring JNICALL
Java_com_xtrans_app_XtransModule_nativeModelsStatus(JNIEnv* env, jobject, jstring models_dir) {
  const std::string dir = from_java(env, models_dir);
  std::string json = "[";
  bool first = true;
  for (const char* name : kModelNames) {
    std::string path = dir + "/" + name;
    bool present = file_exists(path);
    // The unquantised whisper model is an accepted alternative.
    if (!present && std::string_view(name) == "ggml-base-q5_1.bin") {
      path = dir + "/ggml-base.bin";
      present = file_exists(path);
    }
    const auto size = present ? file_size(path) : 0;
    if (!first) json += ",";
    first = false;
    json += "{\"name\":\"";
    json += name;
    json += "\",\"present\":";
    json += present ? "true" : "false";
    json += ",\"bytes\":";
    json += std::to_string(size);
    json += "}";
  }
  json += "]";
  return to_java(env, json);
}

}  // extern "C"
