#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "xtrans/language.hpp"

namespace xtrans {

enum class SttStatus : std::uint8_t {
  ok,
  no_speech,    // recognizer ran and produced nothing usable
  unsupported,  // no recognizer for this language
  failed,       // model or inference error
};

// Speech-to-text port. The recognizer writes UTF-8 into a caller buffer: the
// core owns memory, the adapter owns the model.
class Stt {
 public:
  virtual ~Stt() = default;

  // `written` is always set (0 on failure). Text is NUL-free.
  virtual SttStatus transcribe(std::span<const std::int16_t> pcm, Lang lang,
                               std::span<char> out, std::size_t& written) noexcept = 0;
};

}  // namespace xtrans
