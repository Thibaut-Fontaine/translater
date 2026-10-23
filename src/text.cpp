#include "xtrans/text.hpp"

#include <cstdint>

namespace xtrans {
namespace {

constexpr bool is_blank(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

std::string_view trim(std::string_view s) noexcept {
  while (!s.empty() && is_blank(s.front())) s.remove_prefix(1);
  while (!s.empty() && is_blank(s.back())) s.remove_suffix(1);
  return s;
}

// Length of the longest prefix that ends on a UTF-8 code point boundary. An
// incomplete trailing sequence is dropped entirely rather than left dangling.
std::size_t complete_prefix(std::string_view s) noexcept {
  std::size_t lead_index = s.size();
  std::size_t trailing = 0;
  while (lead_index > 0 && (static_cast<unsigned char>(s[lead_index - 1]) & 0xC0U) == 0x80U) {
    --lead_index;
    ++trailing;
  }
  if (lead_index == 0) return s.size();  // no lead byte: keep the bytes as-is

  const auto lead = static_cast<unsigned char>(s[lead_index - 1]);
  std::size_t need = 1;
  if (lead >= 0xF0U) {
    need = 4;
  } else if (lead >= 0xE0U) {
    need = 3;
  } else if (lead >= 0xC0U) {
    need = 2;
  }

  if (trailing + 1 >= need) return s.size();
  return lead_index - 1;
}

}  // namespace

TextBuffer::TextBuffer(std::span<char> storage) noexcept : storage_(storage) {}

bool TextBuffer::append(std::string_view piece) noexcept {
  piece = trim(piece);
  if (piece.empty()) return !truncated_;

  std::size_t pos = used_;
  if (pos != 0) {
    if (pos == storage_.size()) {
      truncated_ = true;
      return false;
    }
    storage_[pos++] = ' ';
  }

  const std::size_t space = storage_.size() - pos;
  if (piece.size() > space) {
    const std::size_t n = complete_prefix(piece.substr(0, space));
    for (std::size_t i = 0; i < n; ++i) storage_[pos + i] = piece[i];
    used_ = pos + n;
    truncated_ = true;
    return false;
  }

  for (std::size_t i = 0; i < piece.size(); ++i) storage_[pos + i] = piece[i];
  used_ = pos + piece.size();
  return true;
}

void TextBuffer::clear() noexcept {
  used_ = 0;
  truncated_ = false;
}

}  // namespace xtrans
