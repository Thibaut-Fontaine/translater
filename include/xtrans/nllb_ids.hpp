#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace xtrans {

// NLLB-200 reserves id 0, so the SentencePiece ids it was trained with are
// published shifted by one (verified against transformers.js and the model
// itself). Ids at or above the vocabulary size are language/special tokens.
inline constexpr std::int32_t kNllbOffset = 1;
inline constexpr std::int32_t kNllbVocabSize = 256000;
inline constexpr std::int32_t kNllbEos = 2;
inline constexpr std::int32_t kNllbPad = 1;
inline constexpr std::int32_t kNllbBos = 0;

constexpr std::int32_t nllb_to_model_id(std::int32_t sp_id) noexcept {
  return sp_id + kNllbOffset;
}

constexpr std::int32_t nllb_to_sp_id(std::int32_t model_id) noexcept {
  return model_id - kNllbOffset;
}

constexpr bool nllb_is_special(std::int32_t model_id) noexcept {
  return model_id >= kNllbVocabSize;
}

// Builds the encoder input: [source language] + shifted pieces + [eos].
// Returns the number of ids written, or 0 (leaving `out` untouched) when it
// does not fit. Callers allocate for the 1024-token NLLB limit.
std::size_t nllb_build_source_ids(std::span<const std::int32_t> pieces,
                                  std::int32_t source_lang_id,
                                  std::span<std::int64_t> out) noexcept;

// Converts decoder output to SentencePiece ids: drops language tokens and
// control ids (0, 1, 2), then unshifts the rest. Returns 0 on overflow.
std::size_t nllb_filter_output(std::span<const std::int64_t> generated,
                               std::span<std::int32_t> out) noexcept;

}  // namespace xtrans
