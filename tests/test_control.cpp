#include "harness.hpp"
#include "xtrans/control.hpp"

#include <array>
#include <cstring>
#include <string_view>

using namespace xtrans;

TEST_CASE(test_control_parse_accepts_long_and_short_forms) {
  CHECK_EQ(parse_command("press"), Command::press);
  CHECK_EQ(parse_command("p"), Command::press);
  CHECK_EQ(parse_command("release"), Command::release);
  CHECK_EQ(parse_command("r"), Command::release);
  CHECK_EQ(parse_command("abort"), Command::abort);
  CHECK_EQ(parse_command("lang-a"), Command::lang_a);
  CHECK_EQ(parse_command("la"), Command::lang_a);
  CHECK_EQ(parse_command("lang-b"), Command::lang_b);
  CHECK_EQ(parse_command("lb"), Command::lang_b);
  CHECK_EQ(parse_command("swap"), Command::swap);
  CHECK_EQ(parse_command("s"), Command::swap);
  CHECK_EQ(parse_command("quit"), Command::quit);
  CHECK_EQ(parse_command("q"), Command::quit);
}

TEST_CASE(test_control_parse_trims_line_endings_and_spaces) {
  CHECK_EQ(parse_command("press\r\n"), Command::press);
  CHECK_EQ(parse_command("  release  "), Command::release);
  CHECK_EQ(parse_command("q\n"), Command::quit);
}

TEST_CASE(test_control_parse_rejects_unknown_empty_and_wrong_case) {
  CHECK_EQ(parse_command(""), Command::none);
  CHECK_EQ(parse_command("  "), Command::none);
  CHECK_EQ(parse_command("PRESS"), Command::none);
  CHECK_EQ(parse_command("jump"), Command::none);
}

TEST_CASE(test_control_press_and_release_drive_the_pipeline) {
  Controller c;
  CHECK(c.apply(Command::press));
  CHECK_EQ(c.pipeline().state(), State::recording);
  CHECK(c.apply(Command::release));
  CHECK_EQ(c.pipeline().state(), State::transcribing);
  CHECK(!c.apply(Command::release));
  CHECK_EQ(c.pipeline().state(), State::transcribing);
}

TEST_CASE(test_control_language_button_cycles_selection) {
  Controller c;
  CHECK(c.apply(Command::lang_a));
  CHECK_EQ(c.selector().pair().a, Lang::en);
  CHECK_EQ(c.selector().pair().b, Lang::fr);
}

TEST_CASE(test_control_swap_toggles_direction) {
  Controller c;
  CHECK_EQ(c.selector().pair().from(), Lang::fr);
  CHECK(c.apply(Command::swap));
  CHECK_EQ(c.selector().pair().from(), Lang::en);
  CHECK(c.apply(Command::swap));
  CHECK_EQ(c.selector().pair().from(), Lang::fr);
}

TEST_CASE(test_control_quit_latches_and_none_is_ignored) {
  Controller c;
  CHECK(!c.quit());
  CHECK(!c.apply(Command::none));
  CHECK(c.apply(Command::quit));
  CHECK(c.quit());
}

TEST_CASE(test_control_status_line_is_exact) {
  Controller c;
  std::array<char, 64> buf{};
  const std::size_t n = format_status(buf.data(), buf.size(), c);
  CHECK_EQ(std::string_view(buf.data(), n), std::string_view("idle fra->eng turns=0 fails=0"));
}

TEST_CASE(test_control_status_reflects_state_and_counters) {
  Controller c;
  c.apply(Command::press);
  c.apply(Command::release);
  c.pipeline().complete(Stage::stt, false);

  std::array<char, 64> buf{};
  const std::size_t n = format_status(buf.data(), buf.size(), c);
  CHECK_EQ(std::string_view(buf.data(), n),
           std::string_view("idle fra->eng turns=1 fails=1"));
}

TEST_CASE(test_control_status_leaves_buffer_untouched_when_it_does_not_fit) {
  Controller c;
  std::array<char, 8> buf{};
  buf.fill('x');
  CHECK_EQ(format_status(buf.data(), buf.size(), c), 0U);
  CHECK_EQ(std::strncmp(buf.data(), "xxxxxxxx", buf.size()), 0);
}
