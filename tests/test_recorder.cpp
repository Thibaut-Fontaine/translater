#include "harness.hpp"
#include "xtrans/recorder.hpp"

#include <array>
#include <cstdint>
#include <vector>

using namespace xtrans;

namespace {

constexpr std::int16_t kLoud = 12000;
constexpr std::int16_t kQuiet = 0;

std::vector<std::int16_t> frame(std::int16_t value, std::size_t n = kFrameSamples) {
  return std::vector<std::int16_t>(n, value);
}

}  // namespace

TEST_CASE(test_recorder_ignores_frames_before_begin) {
  std::array<std::int16_t, 1024> storage{};
  Recorder recorder{storage};
  const auto loud = frame(kLoud);
  CHECK_EQ(recorder.feed(loud), RecordResult::recording);
  CHECK(!recorder.active());
  CHECK_EQ(recorder.samples().size(), 0U);
}

TEST_CASE(test_recorder_accumulates_samples_and_detects_speech) {
  std::array<std::int16_t, 4096> storage{};
  Recorder recorder{storage};
  recorder.begin();
  CHECK(recorder.active());
  CHECK(!recorder.has_speech());

  const auto loud = frame(kLoud);
  CHECK_EQ(recorder.feed(loud), RecordResult::recording);
  CHECK(recorder.has_speech());
  CHECK_EQ(recorder.samples().size(), kFrameSamples);
  CHECK_EQ(recorder.samples()[0], kLoud);
}

TEST_CASE(test_recorder_does_not_call_silence_speech) {
  std::array<std::int16_t, 4096> storage{};
  Recorder recorder{storage};
  recorder.begin();
  const auto quiet = frame(kQuiet);
  for (int i = 0; i < 10; ++i) CHECK_EQ(recorder.feed(quiet), RecordResult::recording);
  CHECK(!recorder.has_speech());
  CHECK_EQ(recorder.samples().size(), kFrameSamples * 10);
}

TEST_CASE(test_recorder_endpoints_on_hangover_expiry) {
  std::array<std::int16_t, 8192> storage{};
  Recorder recorder{storage};
  recorder.begin();

  const auto loud = frame(kLoud);
  const auto quiet = frame(kQuiet);
  CHECK_EQ(recorder.feed(loud), RecordResult::recording);

  RecordResult result = RecordResult::recording;
  for (std::uint32_t i = 0; i < Gate::kHangoverFrames; ++i) result = recorder.feed(quiet);
  CHECK_EQ(result, RecordResult::stopped_silence);
  CHECK(!recorder.active());
  CHECK(recorder.has_speech());
}

TEST_CASE(test_recorder_raw_silence_never_endpoints) {
  std::array<std::int16_t, 65536> storage{};
  Recorder recorder{storage};
  recorder.begin();
  const auto quiet = frame(kQuiet);
  for (int i = 0; i < 100; ++i) CHECK_EQ(recorder.feed(quiet), RecordResult::recording);
  CHECK(!recorder.has_speech());
}

TEST_CASE(test_recorder_stops_when_the_buffer_is_exactly_full) {
  std::array<std::int16_t, kFrameSamples * 2> storage{};
  Recorder recorder{storage};
  recorder.begin();
  const auto loud = frame(kLoud);
  CHECK_EQ(recorder.feed(loud), RecordResult::recording);
  CHECK_EQ(recorder.feed(loud), RecordResult::stopped_full);
  CHECK(!recorder.active());
  CHECK(!recorder.truncated());
  CHECK_EQ(recorder.samples().size(), storage.size());
}

TEST_CASE(test_recorder_flags_truncation_when_a_frame_does_not_fit) {
  std::array<std::int16_t, kFrameSamples + 100> storage{};
  Recorder recorder{storage};
  recorder.begin();
  const auto loud = frame(kLoud);
  CHECK_EQ(recorder.feed(loud), RecordResult::recording);
  CHECK_EQ(recorder.feed(loud), RecordResult::stopped_full);
  CHECK(recorder.truncated());
  CHECK_EQ(recorder.samples().size(), storage.size());
}

TEST_CASE(test_recorder_stop_is_explicit_and_idempotent) {
  std::array<std::int16_t, 1024> storage{};
  Recorder recorder{storage};
  recorder.begin();
  recorder.feed(frame(kLoud));
  CHECK_EQ(recorder.stop(), RecordResult::stopped_button);
  CHECK(!recorder.active());
  CHECK_EQ(recorder.stop(), RecordResult::stopped_button);
}

TEST_CASE(test_recorder_begin_resets_state) {
  std::array<std::int16_t, 4096> storage{};
  Recorder recorder{storage};
  recorder.begin();
  recorder.feed(frame(kLoud));
  const auto quiet = frame(kQuiet);
  while (recorder.active()) recorder.feed(quiet);
  CHECK(recorder.has_speech());

  recorder.begin();
  CHECK(!recorder.has_speech());
  CHECK(!recorder.truncated());
  CHECK_EQ(recorder.samples().size(), 0U);
}
