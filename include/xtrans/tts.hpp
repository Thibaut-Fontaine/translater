#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "xtrans/language.hpp"

namespace xtrans {

enum class TtsStatus : std::uint8_t {
  ok,
  unsupported,  // no voice for this language
  too_long,     // text exceeds what the voice can take
  failed,
};

// Text-to-speech port. The adapter owns the voice model; the core owns the
// output buffer, already resampled to the pipeline rate.
class Tts {
 public:
  virtual ~Tts() = default;

  // Synthesizes `text` into 16 kHz mono samples. `written` is always set.
  virtual TtsStatus speak(std::string_view text, Lang lang, std::span<std::int16_t> out,
                          std::size_t& written) noexcept = 0;
};

}  // namespace xtrans
