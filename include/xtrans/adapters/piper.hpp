#pragma once

#if !defined(XTRANS_WITH_PIPER) || !XTRANS_WITH_PIPER
#error "xtrans/adapters/piper.hpp requires a build with XTRANS_WITH_PIPER=1"
#endif

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "xtrans/tts.hpp"

namespace xtrans {

struct PiperConfig {
  const char* voice_dir = nullptr;   // contains <voice-id>.onnx and .onnx.json
  const char* espeak_data = nullptr; // vendored espeak-ng-data directory
};

// Piper (GPL-3) as a TTS engine. Voices are loaded lazily per language and kept
// resident; synthesis is float PCM from the voice's own rate, converted and
// resampled to the pipeline rate here.
class PiperTts final : public Tts {
 public:
  static std::unique_ptr<PiperTts> open(const PiperConfig& config, std::string& error) noexcept;

  ~PiperTts() override;

  PiperTts(const PiperTts&) = delete;
  PiperTts& operator=(const PiperTts&) = delete;

  TtsStatus speak(std::string_view text, Lang lang, std::span<std::int16_t> out,
                  std::size_t& written) noexcept override;

 private:
  PiperTts() noexcept;
  TtsStatus synthesize(std::string_view text, Lang lang, std::span<std::int16_t> out,
                       std::size_t& written) noexcept;
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace xtrans
