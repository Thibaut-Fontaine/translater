#include "harness.hpp"
#include "xtrans/device.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

using namespace xtrans;

namespace {

constexpr std::int16_t kLoud = 12000;
constexpr std::int16_t kQuiet = 0;

struct FakeButtons final : ButtonSource {
  std::vector<ButtonState> samples;
  std::size_t next = 0;

  bool poll(ButtonState& out) noexcept override {
    if (next >= samples.size()) return false;
    out = samples[next++];
    return true;
  }

  void add(ButtonState state, int count) {
    for (int i = 0; i < count; ++i) samples.push_back(state);
  }
};

struct FakeAudio final : AudioIn {
  std::vector<std::vector<std::int16_t>> frames;
  std::size_t next = 0;
  bool bad = false;

  std::size_t read(std::span<std::int16_t> out) noexcept override {
    if (next >= frames.size()) return 0;
    const auto& f = frames[next++];
    const std::size_t n = std::min(out.size(), f.size());
    std::copy_n(f.begin(), n, out.begin());
    return n;
  }

  bool failed() const noexcept override { return bad; }

  void add_level(std::int16_t value, int count) {
    for (int i = 0; i < count; ++i) frames.emplace_back(kFrameSamples, value);
  }
};

struct Fixture {
  FakeButtons buttons;
  FakeAudio audio;
  std::vector<std::int16_t> capture;
  Device device;

  explicit Fixture(std::size_t capacity = kSampleRate * 5)
      : capture(capacity), device(buttons, audio, capture) {}

  void ticks(int n) {
    for (int i = 0; i < n; ++i) device.tick();
  }
};

}  // namespace

TEST_CASE(test_device_idle_without_input_does_nothing) {
  Fixture f;
  CHECK(!f.device.tick());
  CHECK_EQ(f.device.controller().pipeline().state(), State::idle);
  CHECK_EQ(f.device.turns(), 0U);
  CHECK_EQ(f.device.discarded(), 0U);
}

TEST_CASE(test_device_push_to_talk_records_and_dispatches_a_turn) {
  Fixture f;
  f.buttons.add(ButtonState{false, false, true}, 3);
  f.audio.add_level(kLoud, 15);
  f.ticks(10);

  CHECK(f.device.recording());
  CHECK_EQ(f.device.controller().pipeline().state(), State::recording);

  f.buttons.add(ButtonState{false, false, false}, 3);
  f.ticks(5);

  CHECK(!f.device.recording());
  CHECK_EQ(f.device.last_result(), RecordResult::stopped_button);
  CHECK_EQ(f.device.turns(), 1U);
  CHECK_EQ(f.device.controller().pipeline().state(), State::transcribing);
  CHECK(f.device.last_turn().size() >= kFrameSamples);
  CHECK_EQ(f.device.last_turn()[0], kLoud);
}

TEST_CASE(test_device_discards_a_silence_only_turn) {
  Fixture f;
  f.buttons.add(ButtonState{false, false, true}, 3);
  f.audio.add_level(kQuiet, 15);
  f.ticks(10);
  f.buttons.add(ButtonState{false, false, false}, 3);
  f.ticks(5);

  CHECK_EQ(f.device.turns(), 0U);
  CHECK_EQ(f.device.discarded(), 1U);
  CHECK(f.device.last_turn().empty());
  CHECK_EQ(f.device.controller().pipeline().state(), State::idle);
}

TEST_CASE(test_device_endpoints_a_turn_on_silence_without_release) {
  Fixture f;
  f.buttons.add(ButtonState{false, false, true}, 3);
  f.audio.add_level(kLoud, 4);
  f.audio.add_level(kQuiet, static_cast<int>(Gate::kHangoverFrames) + 2);
  f.ticks(static_cast<int>(Gate::kHangoverFrames) + 20);

  CHECK(!f.device.recording());
  CHECK_EQ(f.device.last_result(), RecordResult::stopped_silence);
  CHECK_EQ(f.device.turns(), 1U);
  CHECK_EQ(f.device.controller().pipeline().state(), State::transcribing);
}

TEST_CASE(test_device_cuts_the_turn_at_max_duration) {
  Fixture f{kFrameSamples * 3};
  f.buttons.add(ButtonState{false, false, true}, 3);
  f.audio.add_level(kLoud, 10);
  f.ticks(15);

  CHECK(!f.device.recording());
  CHECK_EQ(f.device.last_result(), RecordResult::stopped_full);
  CHECK_EQ(f.device.turns(), 1U);
  CHECK_EQ(f.device.last_turn().size(), static_cast<std::size_t>(kFrameSamples) * 3);
}

TEST_CASE(test_device_discards_the_turn_when_capture_fails) {
  Fixture f;
  f.buttons.add(ButtonState{false, false, true}, 3);
  f.audio.add_level(kLoud, 4);
  f.ticks(2);
  f.audio.bad = true;
  f.ticks(1);

  CHECK(!f.device.recording());
  CHECK_EQ(f.device.turns(), 0U);
  CHECK_EQ(f.device.discarded(), 1U);
  CHECK_EQ(f.device.controller().pipeline().state(), State::idle);
}

TEST_CASE(test_device_language_button_cycles_while_idle) {
  Fixture f;
  f.buttons.add(ButtonState{true, false, false}, 3);
  f.ticks(4);

  CHECK_EQ(f.device.controller().selector().pair().a, Lang::en);
  CHECK_EQ(f.device.controller().selector().pair().b, Lang::fr);
  CHECK_EQ(f.device.controller().pipeline().state(), State::idle);
}

TEST_CASE(test_device_ignores_a_release_without_a_press) {
  Fixture f;
  f.buttons.add(ButtonState{false, false, false}, 3);
  f.ticks(4);
  CHECK_EQ(f.device.turns(), 0U);
  CHECK_EQ(f.device.controller().pipeline().state(), State::idle);
}
