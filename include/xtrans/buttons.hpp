#pragma once

#include <array>
#include <cstdint>

#include "xtrans/language.hpp"

namespace xtrans {

struct ButtonState {
  bool lang_a = false;
  bool lang_b = false;
  bool ptt = false;
};

enum class Edge : std::uint8_t { down, up };

struct Press {
  Button button = Button::ptt;
  Edge edge = Edge::down;
};

inline constexpr std::uint8_t kMaxPresses = 3;

struct Presses {
  std::array<Press, kMaxPresses> items{};
  std::uint8_t count = 0;

  bool empty() const noexcept { return count == 0; }
};

// Stable-state filter. A button must read the same value for `stable` samples
// before the edge is reported, which kills contact bounce on cheap tactile
// switches without adding a timer.
class Debouncer {
 public:
  static constexpr std::uint8_t kStableSamples = 3;

  explicit Debouncer(std::uint8_t stable = kStableSamples) noexcept;

  Presses feed(const ButtonState& raw) noexcept;

  const ButtonState& stable() const noexcept { return stable_; }

 private:
  static constexpr std::array<Button, 3> kButtons{Button::lang_a, Button::lang_b, Button::ptt};

  std::array<bool, 3> last_{};
  std::array<std::uint8_t, 3> count_{};
  ButtonState stable_{};
  std::uint8_t need_;
};

class ButtonSource {
 public:
  virtual ~ButtonSource() = default;

  // Writes the current raw state and returns true when a new sample exists.
  virtual bool poll(ButtonState& out) noexcept = 0;
};

// Maps raw line levels (0 or 1, negative means a failed read) to button states.
// Negative levels count as released: a broken line must never look pressed,
// otherwise a turn would stay in recording forever.
constexpr ButtonState decode_levels(int lang_a, int lang_b, int ptt,
                                    bool active_low) noexcept {
  const auto pressed = [active_low](int level) constexpr noexcept {
    if (level < 0) return false;
    return active_low ? level == 0 : level != 0;
  };
  return ButtonState{pressed(lang_a), pressed(lang_b), pressed(ptt)};
}

}  // namespace xtrans
