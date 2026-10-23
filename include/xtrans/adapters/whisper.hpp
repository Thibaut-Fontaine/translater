#pragma once

#if !defined(XTRANS_WITH_WHISPER) || !XTRANS_WITH_WHISPER
#error "xtrans/adapters/whisper.hpp requires a build with XTRANS_WITH_WHISPER=1"
#endif

#include <cstddef>
#include <memory>
#include <span>
#include <string>

#include "xtrans/stt.hpp"

namespace xtrans {

// whisper.cpp recognizer. The model is large and mmapped: load it once at
// startup, never per utterance. Language comes from the pinned language table,
// so an unsupported language is rejected before any inference happens.
class WhisperStt final : public Stt {
 public:
  static std::unique_ptr<WhisperStt> open(const char* model_path, std::string& error) noexcept;

  ~WhisperStt() override;

  WhisperStt(const WhisperStt&) = delete;
  WhisperStt& operator=(const WhisperStt&) = delete;

  SttStatus transcribe(std::span<const std::int16_t> pcm, Lang lang, std::span<char> out,
                       std::size_t& written) noexcept override;

 private:
  WhisperStt() noexcept;
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace xtrans
