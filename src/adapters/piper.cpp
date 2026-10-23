#include "xtrans/adapters/piper.hpp"

#include <piper.h>

#include <array>
#include <cstring>
#include <exception>
#include <vector>

#include "xtrans/audio.hpp"
#include "xtrans/audio_convert.hpp"

namespace xtrans {
namespace {

constexpr std::size_t kMaxSynthSamples = 22050U * 30U;  // 30 s of speech

}  // namespace

struct PiperTts::Impl {
  struct Voice {
    piper_synthesizer* synth = nullptr;
  };

  std::string voice_dir;
  std::string espeak_data;
  std::array<Voice, kLangCount> voices{};
  std::vector<float> floats;
  std::vector<std::int16_t> pcm;
  std::string text;

  ~Impl() {
    for (auto& voice : voices) {
      if (voice.synth != nullptr) piper_free(voice.synth);
    }
  }

  piper_synthesizer* voice_for(Lang lang) noexcept {
    const auto id = lang_info(lang).piper;
    if (id.empty()) return nullptr;

    auto& slot = voices[static_cast<std::size_t>(lang)];
    if (slot.synth == nullptr) {
      const std::string model = voice_dir + "/" + std::string(id) + ".onnx";
      const std::string config = model + ".json";
      slot.synth = piper_create(model.c_str(), config.c_str(),
                                espeak_data.empty() ? nullptr : espeak_data.c_str());
      if (slot.synth == nullptr) return nullptr;
    }
    return slot.synth;
  }
};

PiperTts::PiperTts() noexcept : impl_(std::make_unique<Impl>()) {}

PiperTts::~PiperTts() = default;

std::unique_ptr<PiperTts> PiperTts::open(const PiperConfig& config, std::string& error) noexcept {
  if (config.voice_dir == nullptr) {
    error = "no voice directory";
    return nullptr;
  }
  auto self = std::unique_ptr<PiperTts>(new (std::nothrow) PiperTts());
  if (!self) {
    error = "out of memory";
    return nullptr;
  }
  self->impl_->voice_dir = config.voice_dir;
  if (config.espeak_data != nullptr) self->impl_->espeak_data = config.espeak_data;
  self->impl_->floats.reserve(kMaxSynthSamples);

  // Warm the voices at startup: the first call pays model load and ONNX
  // session creation (seconds). A voice that fails to load stays unavailable
  // and speak() reports `unsupported` instead of failing mid-turn.
  try {
    for (std::size_t i = 0; i < kLangCount; ++i) {
      const auto lang = static_cast<Lang>(i);
      if (!lang_info(lang).piper.empty()) (void)self->impl_->voice_for(lang);
    }
  } catch (...) {
    // A broken voice must not take the whole engine down.
  }
  return self;
}

TtsStatus PiperTts::speak(std::string_view text, Lang lang, std::span<std::int16_t> out,
                          std::size_t& written) noexcept {
  written = 0;
  if (text.empty() || out.empty()) return TtsStatus::failed;

  try {
    return synthesize(text, lang, out, written);
  } catch (const std::exception&) {
    return TtsStatus::failed;
  } catch (...) {
    return TtsStatus::failed;
  }
}

TtsStatus PiperTts::synthesize(std::string_view text, Lang lang, std::span<std::int16_t> out,
                               std::size_t& written) {
  auto& impl = *impl_;
  piper_synthesizer* synth = impl.voice_for(lang);
  if (synth == nullptr) return TtsStatus::unsupported;

  impl.text.assign(text);
  const piper_synthesize_options options = piper_default_synthesize_options(synth);
  if (piper_synthesize_start(synth, impl.text.c_str(), &options) != PIPER_OK) {
    return TtsStatus::failed;
  }

  impl.floats.clear();
  int sample_rate = 22050;  // overwritten by the first chunk
  while (true) {
    piper_audio_chunk chunk{};
    const int rc = piper_synthesize_next(synth, &chunk);
    if (rc != PIPER_OK && rc != PIPER_DONE) return TtsStatus::failed;

    if (chunk.num_samples != 0) {
      if (impl.floats.size() + chunk.num_samples > kMaxSynthSamples) return TtsStatus::too_long;
      impl.floats.insert(impl.floats.end(), chunk.samples, chunk.samples + chunk.num_samples);
      sample_rate = chunk.sample_rate;
    }
    if (rc == PIPER_DONE || chunk.is_last) break;
  }

  if (impl.floats.empty()) return TtsStatus::failed;

  impl.pcm.resize(impl.floats.size());
  float_to_pcm16(impl.floats, impl.pcm);
  const std::size_t n = resample_linear(impl.pcm, static_cast<std::uint32_t>(sample_rate), out,
                                        kSampleRate);
  written = n;
  return n == 0 ? TtsStatus::failed : TtsStatus::ok;
}

}  // namespace xtrans
