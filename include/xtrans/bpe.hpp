#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace xtrans {

// NLLB's tokenizer without SentencePiece. The vocabulary and the merge table
// are exported once by scripts/gen_nllb_bpe.py and loaded at startup; input
// must already be NFKC-normalised (Android: java.text.Normalizer, iOS: NSString,
// Pi: SentencePiece does it itself, so the Pi keeps that path).
//
// Not on the audio path: a few vectors per utterance are fine here.
class Bpe {
 public:
  // Borrows `blob`; it must outlive the encoder.
  bool load(std::span<const std::uint8_t> blob) noexcept;

  bool loaded() const noexcept { return !entries_.empty(); }
  std::int32_t unk_id() const noexcept { return unk_; }

  // Encodes UTF-8 into tokenizer ids (the published NLLB ids, not shifted).
  // Returns the number written; 0 when the output buffer is too small.
  std::size_t encode(std::string_view text, std::span<std::int32_t> out) noexcept;

  // Decodes ids back to UTF-8: drops specials, turns U+2581 into spaces.
  // Returns the number of bytes written.
  std::size_t decode(std::span<const std::int32_t> ids, std::span<char> out) noexcept;

 private:
  struct Entry {
    std::uint32_t offset;
    std::uint16_t len;
    std::uint32_t id;
  };
  struct Merge {
    std::uint32_t left;
    std::uint32_t right;
    std::uint32_t result;
    std::uint32_t rank;  // lower wins when several pairs are mergeable
  };

  std::string_view piece(const Entry& e) const noexcept {
    return std::string_view(reinterpret_cast<const char*>(blob_.data() + e.offset), e.len);
  }
  const Entry* find_piece(std::string_view text) const noexcept;
  const Merge* find_merge(std::int32_t left, std::int32_t right) const noexcept;

  std::span<const std::uint8_t> blob_;
  std::vector<Entry> entries_;          // sorted by bytes
  std::vector<std::uint32_t> by_id_;    // entry index per tokenizer id
  std::vector<Merge> merges_;           // sorted by (left, right)
  std::int32_t unk_ = 3;
};

}  // namespace xtrans
