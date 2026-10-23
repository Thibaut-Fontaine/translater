#pragma once

#include <cstddef>
#include <span>
#include <string_view>

namespace xtrans {

// Bounded UTF-8 accumulator for recognizer segments. Joins with a single space,
// trims each piece, and never leaves a half-written UTF-8 sequence at the end:
// a truncated transcript must stay printable, not break the console or TTS.
class TextBuffer {
 public:
  explicit TextBuffer(std::span<char> storage) noexcept;

  // Returns false when the piece did not fit completely (text is truncated).
  bool append(std::string_view piece) noexcept;

  std::string_view text() const noexcept { return std::string_view(storage_.data(), used_); }
  bool truncated() const noexcept { return truncated_; }
  void clear() noexcept;

 private:
  std::span<char> storage_;
  std::size_t used_ = 0;
  bool truncated_ = false;
};

}  // namespace xtrans
