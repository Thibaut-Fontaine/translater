#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "xtrans/language.hpp"
#include "xtrans/pipeline.hpp"

namespace xtrans {

// One line of the control interface: "press", "release", "abort", "lang-a",
// "lang-b", "swap", "quit", or their two-letter aliases. Case-sensitive so a
// corrupted line never triggers a turn.
enum class Command : std::uint8_t { none, press, release, abort, lang_a, lang_b, swap, quit };

Command parse_command(std::string_view line) noexcept;

// Wires the buttons to the pipeline and the language pair. This is the whole
// device behaviour; the adapters (GPIO, ALSA, models) stay outside.
class Controller {
 public:
  // Applies a parsed command. Returns whether it was accepted.
  bool apply(Command command) noexcept;

  bool quit() const noexcept { return quit_; }
  const Pipeline& pipeline() const noexcept { return pipeline_; }
  Pipeline& pipeline() noexcept { return pipeline_; }
  const Selector& selector() const noexcept { return selector_; }

 private:
  Pipeline pipeline_;
  Selector selector_;
  bool quit_ = false;
};

inline constexpr std::size_t kStatusMax = 64;

// Writes "idle fr->en turns=0 fails=0" plus a terminating NUL is not included.
// Returns the length written, or 0 without touching `out` when it does not fit.
// Allocation-free: safe to call from the daemon loop after every event.
std::size_t format_status(char* out, std::size_t cap, const Controller& controller) noexcept;

std::string_view state_name(State state) noexcept;

}  // namespace xtrans
