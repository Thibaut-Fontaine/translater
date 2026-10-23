#include "xtrans/adapters/whisper.hpp"

#include <whisper.h>

#include <algorithm>
#include <cstdio>
#include <vector>

#include "xtrans/audio.hpp"
#include "xtrans/text.hpp"

namespace xtrans {
namespace {

// whisper.cpp logs model loading to stderr; the device logs to the journal and
// stays quiet unless something is wrong.
void silent_log(enum ggml_log_level, const char*, void*) {}

}  // namespace

struct WhisperStt::Impl {
  whisper_context* ctx = nullptr;
  // 15 s at 16 kHz: allocated once at open, never per utterance.
  std::vector<float> floats;
  std::uint64_t inferences = 0;
};

WhisperStt::WhisperStt() noexcept : impl_(std::make_unique<Impl>()) {}

WhisperStt::~WhisperStt() {
  if (impl_->ctx != nullptr) {
    whisper_free(impl_->ctx);
    impl_->ctx = nullptr;
  }
}

std::unique_ptr<WhisperStt> WhisperStt::open(const char* model_path, std::string& error) noexcept {
  std::unique_ptr<WhisperStt> self(new (std::nothrow) WhisperStt());
  if (!self) {
    error = "out of memory";
    return nullptr;
  }

  whisper_log_set(silent_log, nullptr);
  self->impl_->ctx = whisper_init_from_file_with_params(model_path, whisper_context_default_params());
  if (self->impl_->ctx == nullptr) {
    error = "cannot load whisper model";
    return nullptr;
  }
  self->impl_->floats.resize(kSampleRate * 15U);
  return self;
}

SttStatus WhisperStt::transcribe(std::span<const std::int16_t> pcm, Lang lang,
                                 std::span<char> out, std::size_t& written) noexcept {
  written = 0;
  const std::string_view code = lang_info(lang).whisper;
  if (code.empty()) return SttStatus::unsupported;
  if (pcm.empty() || out.empty()) return SttStatus::no_speech;

  whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
  params.print_progress = false;
  params.print_special = false;
  params.print_realtime = false;
  params.print_timestamps = false;
  params.translate = false;  // translation is NLLB's job, not the recognizer's
  params.no_context = true;
  params.single_segment = false;
  params.n_threads = 4;
  // The code points into the language table's static storage, which outlives
  // this call.
  params.language = code.data();

  if (pcm.size() > impl_->floats.size()) return SttStatus::failed;
  for (std::size_t i = 0; i < pcm.size(); ++i) {
    impl_->floats[i] = static_cast<float>(pcm[i]) / 32768.0F;
  }

  if (whisper_full(impl_->ctx, params, impl_->floats.data(), static_cast<int>(pcm.size())) != 0) {
    return SttStatus::failed;
  }

  TextBuffer text{out};
  const int segments = whisper_full_n_segments(impl_->ctx);
  for (int i = 0; i < segments; ++i) {
    const char* piece = whisper_full_get_segment_text(impl_->ctx, i);
    if (piece != nullptr) text.append(piece);
  }

  written = text.text().size();
  ++impl_->inferences;
  return written == 0 ? SttStatus::no_speech : SttStatus::ok;
}

}  // namespace xtrans
