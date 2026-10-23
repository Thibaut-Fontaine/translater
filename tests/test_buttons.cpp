#include "harness.hpp"
#include "xtrans/buttons.hpp"

using namespace xtrans;

namespace {

ButtonState only_ptt(bool down) { return ButtonState{false, false, down}; }

}  // namespace

TEST_CASE(test_buttons_need_three_stable_samples_to_press) {
  Debouncer debouncer;
  CHECK(debouncer.feed(only_ptt(true)).empty());
  CHECK(debouncer.feed(only_ptt(true)).empty());

  const Presses presses = debouncer.feed(only_ptt(true));
  CHECK_EQ(presses.count, 1);
  CHECK_EQ(presses.items[0].button, Button::ptt);
  CHECK_EQ(presses.items[0].edge, Edge::down);
  CHECK(debouncer.stable().ptt);
}

TEST_CASE(test_buttons_a_bounce_resets_the_counter) {
  Debouncer debouncer;
  debouncer.feed(only_ptt(true));
  debouncer.feed(only_ptt(false));
  debouncer.feed(only_ptt(true));
  CHECK(debouncer.feed(only_ptt(true)).empty());
  CHECK(!debouncer.stable().ptt);
  CHECK_EQ(debouncer.feed(only_ptt(true)).count, 1);
}

TEST_CASE(test_buttons_release_is_an_edge_too) {
  Debouncer debouncer;
  for (int i = 0; i < Debouncer::kStableSamples; ++i) debouncer.feed(only_ptt(true));

  Presses presses;
  for (int i = 0; i < Debouncer::kStableSamples; ++i) presses = debouncer.feed(only_ptt(false));
  CHECK_EQ(presses.count, 1);
  CHECK_EQ(presses.items[0].edge, Edge::up);
  CHECK(!debouncer.stable().ptt);
}

TEST_CASE(test_buttons_report_every_button_in_one_feed) {
  Debouncer debouncer;
  ButtonState all{true, true, true};
  Presses presses;
  for (int i = 0; i < Debouncer::kStableSamples; ++i) presses = debouncer.feed(all);

  CHECK_EQ(presses.count, 3);
  CHECK_EQ(presses.items[0].button, Button::lang_a);
  CHECK_EQ(presses.items[1].button, Button::lang_b);
  CHECK_EQ(presses.items[2].button, Button::ptt);
  CHECK(debouncer.stable().lang_a);
  CHECK(debouncer.stable().lang_b);
  CHECK(debouncer.stable().ptt);
}

TEST_CASE(test_buttons_steady_state_produces_no_events) {
  Debouncer debouncer;
  ButtonState idle{};
  for (int i = 0; i < 10; ++i) CHECK(debouncer.feed(idle).empty());
}

TEST_CASE(test_buttons_short_glitch_is_ignored) {
  Debouncer debouncer;
  debouncer.feed(only_ptt(true));
  debouncer.feed(only_ptt(true));
  CHECK(debouncer.feed(only_ptt(false)).empty());
  CHECK(!debouncer.stable().ptt);
  CHECK(debouncer.feed(only_ptt(false)).empty());
}

TEST_CASE(test_buttons_configurable_stability) {
  Debouncer debouncer{1};
  const Presses presses = debouncer.feed(only_ptt(true));
  CHECK_EQ(presses.count, 1);
}

TEST_CASE(test_buttons_active_low_levels_are_inverted) {
  const ButtonState idle = decode_levels(1, 1, 1, true);
  CHECK(!idle.lang_a);
  CHECK(!idle.lang_b);
  CHECK(!idle.ptt);

  const ButtonState pressed = decode_levels(0, 0, 0, true);
  CHECK(pressed.lang_a);
  CHECK(pressed.lang_b);
  CHECK(pressed.ptt);
}

TEST_CASE(test_buttons_active_high_levels_are_not_inverted) {
  const ButtonState pressed = decode_levels(1, 0, 1, false);
  CHECK(pressed.lang_a);
  CHECK(!pressed.lang_b);
  CHECK(pressed.ptt);
}

TEST_CASE(test_buttons_decode_treats_errors_as_released) {
  // gpiod returns -1 on a failed read; a stuck-low line must never look pressed
  // forever, which would wedge a turn in recording.
  const ButtonState state = decode_levels(-1, 1, 1, true);
  CHECK(!state.lang_a);
  CHECK(!state.lang_b);
  CHECK(!state.ptt);
}
