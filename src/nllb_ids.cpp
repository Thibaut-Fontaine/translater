#include "xtrans/nllb_ids.hpp"

namespace xtrans {
namespace {

bool is_control(std::int64_t id) noexcept {
  return id == kNllbBos || id == kNllbPad || id == kNllbEos;
}

}  // namespace

std::size_t nllb_build_source_ids(std::span<const std::int32_t> pieces,
                                  std::int32_t source_lang_id,
                                  std::span<std::int64_t> out) noexcept {
  const std::size_t needed = pieces.size() + 2;
  if (out.size() < needed) return 0;

  std::size_t pos = 0;
  out[pos++] = source_lang_id;
  for (const std::int32_t piece : pieces) {
    out[pos++] = nllb_to_model_id(piece);
  }
  out[pos++] = kNllbEos;
  return pos;
}

std::size_t nllb_filter_output(std::span<const std::int64_t> generated,
                               std::span<std::int32_t> out) noexcept {
  std::size_t needed = 0;
  for (const std::int64_t id : generated) {
    if (!nllb_is_special(static_cast<std::int32_t>(id)) &&
        !is_control(id)) {
      ++needed;
    }
  }
  if (out.size() < needed) return 0;

  std::size_t pos = 0;
  for (const std::int64_t id : generated) {
    if (nllb_is_special(static_cast<std::int32_t>(id)) || is_control(id)) continue;
    out[pos++] = nllb_to_sp_id(static_cast<std::int32_t>(id));
  }
  return pos;
}

}  // namespace xtrans
