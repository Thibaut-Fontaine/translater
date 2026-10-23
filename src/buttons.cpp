#include "xtrans/buttons.hpp"

namespace xtrans {

Debouncer::Debouncer(std::uint8_t stable) noexcept : need_(stable == 0 ? 1 : stable) {}

Presses Debouncer::feed(const ButtonState& raw) noexcept {
  const std::array<bool, 3> now{raw.lang_a, raw.lang_b, raw.ptt};
  Presses out;

  for (std::size_t i = 0; i < now.size(); ++i) {
    if (now[i] == last_[i]) {
      count_[i] = 0;
      continue;
    }
    if (++count_[i] < need_) continue;

    count_[i] = 0;
    last_[i] = now[i];
    if (out.count < kMaxPresses) {
      out.items[out.count] = Press{kButtons[i], now[i] ? Edge::down : Edge::up};
      ++out.count;
    }
  }

  stable_ = ButtonState{last_[0], last_[1], last_[2]};
  return out;
}

}  // namespace xtrans
