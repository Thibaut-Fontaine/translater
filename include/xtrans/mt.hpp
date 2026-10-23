#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "xtrans/language.hpp"

namespace xtrans {

enum class MtStatus : std::uint8_t {
  ok,
  unsupported,  // pair not in the validated table, or no translation model
  input_empty,
  failed,
};

// Machine-translation port. Same contract as Stt: caller owns the buffer, the
// adapter owns the model, failures are returned as enum values.
class Mt {
 public:
  virtual ~Mt() = default;

  // Translates UTF-8 `text` from `from` to `to` into `out`. `written` is always
  // set (0 on failure) and never includes a NUL.
  virtual MtStatus translate(std::string_view text, Lang from, Lang to, std::span<char> out,
                             std::size_t& written) noexcept = 0;
};

}  // namespace xtrans
