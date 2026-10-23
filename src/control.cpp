#include "xtrans/control.hpp"

#include <array>
#include <charconv>
#include <cstring>

namespace xtrans {
namespace {

constexpr std::string_view kBlanks = " \t\r\n";

std::size_t put(std::string_view s, char* out, std::size_t pos) noexcept {
  std::memcpy(out + pos, s.data(), s.size());
  return pos + s.size();
}

// Two-digit-safe unsigned formatting; counts never exceed uint32_t.
std::size_t put_uint(std::uint32_t value, char* out, std::size_t pos) noexcept {
  const auto res = std::to_chars(out + pos, out + pos + 10, value);
  return static_cast<std::size_t>(res.ptr - out);
}

}  // namespace

Command parse_command(std::string_view line) noexcept {
  const std::size_t first = line.find_first_not_of(kBlanks);
  if (first == std::string_view::npos) return Command::none;
  const std::size_t last = line.find_last_not_of(kBlanks);
  const std::string_view token = line.substr(first, last - first + 1);

  if (token == "press" || token == "p") return Command::press;
  if (token == "release" || token == "r") return Command::release;
  if (token == "abort" || token == "a") return Command::abort;
  if (token == "lang-a" || token == "la") return Command::lang_a;
  if (token == "lang-b" || token == "lb") return Command::lang_b;
  if (token == "swap" || token == "s") return Command::swap;
  if (token == "quit" || token == "q") return Command::quit;
  return Command::none;
}

bool Controller::apply(Command command) noexcept {
  switch (command) {
    case Command::press: return pipeline_.press();
    case Command::release: return pipeline_.release();
    case Command::abort: return pipeline_.abort();
    case Command::lang_a:
      selector_.cycle(Button::lang_a);
      return true;
    case Command::lang_b:
      selector_.cycle(Button::lang_b);
      return true;
    case Command::swap:
      selector_.swap_direction();
      return true;
    case Command::quit: quit_ = true; return true;
    case Command::none: break;
  }
  return false;
}

std::string_view state_name(State state) noexcept {
  switch (state) {
    case State::idle: return "idle";
    case State::recording: return "recording";
    case State::transcribing: return "transcribing";
    case State::translating: return "translating";
    case State::speaking: return "speaking";
  }
  return "?";
}

std::size_t format_status(char* out, std::size_t cap, const Controller& controller) noexcept {
  const Pair& pair = controller.selector().pair();
  const Pipeline& pipeline = controller.pipeline();

  std::array<char, kStatusMax> line{};
  std::size_t pos = 0;
  pos = put(state_name(pipeline.state()), line.data(), pos);
  pos = put(" ", line.data(), pos);
  pos = put(lang_info(pair.from()).iso639_3, line.data(), pos);
  pos = put("->", line.data(), pos);
  pos = put(lang_info(pair.to()).iso639_3, line.data(), pos);
  pos = put(" turns=", line.data(), pos);
  pos = put_uint(pipeline.turns(), line.data(), pos);
  pos = put(" fails=", line.data(), pos);
  pos = put_uint(pipeline.failures(), line.data(), pos);

  if (pos > cap) return 0;
  std::memcpy(out, line.data(), pos);
  return pos;
}

}  // namespace xtrans
