#include "harness.hpp"
#include "xtrans/audio.hpp"

#include <array>
#include <cmath>
#include <cstdint>

using namespace xtrans;

namespace {

constexpr std::array<std::int16_t, 16> kRamp = {0,  1000, 2000, 3000, 4000, 5000, 6000,  7000,
                                                8000, 9000, 10000, 11000, 12000, 13000,
                                                14000, 15000};

}  // namespace

TEST_CASE(test_audio_rms_of_silence_is_zero) {
  std::array<std::int16_t, 8> silence{};
  CHECK_EQ(rms(silence), 0);
}

TEST_CASE(test_audio_rms_of_full_scale_square_is_full_scale) {
  std::array<std::int16_t, 8> full{};
  full.fill(32767);
  CHECK_EQ(rms(full), 32767);
  full.fill(-32768);
  CHECK_EQ(rms(full), 32767);  // clamped: the scale is 0..32767
}

TEST_CASE(test_audio_rms_of_empty_input_is_zero) {
  CHECK_EQ(rms(std::span<const std::int16_t>{}), 0);
}

TEST_CASE(test_audio_dbfs_of_silence_is_floored) {
  std::array<std::int16_t, 8> silence{};
  CHECK_EQ(dbfs(silence), kSilenceDbfs);
}

TEST_CASE(test_audio_dbfs_of_full_scale_is_zero) {
  std::array<std::int16_t, 4> full{};
  full.fill(32767);
  const float db = dbfs(full);
  CHECK(std::fabs(db) < 0.01f);
}

TEST_CASE(test_audio_dbfs_halving_amplitude_costs_six_db) {
  std::array<std::int16_t, 4> loud{};
  std::array<std::int16_t, 4> half{};
  loud.fill(32000);
  half.fill(16000);
  CHECK(std::fabs((dbfs(loud) - dbfs(half)) - 6.02f) < 0.05f);
}

TEST_CASE(test_audio_ring_push_pop_preserves_order) {
  std::array<std::int16_t, 8> storage{};
  Ring ring{storage};
  CHECK_EQ(ring.capacity(), 8U);
  CHECK_EQ(ring.size(), 0U);

  CHECK_EQ(ring.push(kRamp.data(), 4), 0U);
  CHECK_EQ(ring.size(), 4U);

  std::array<std::int16_t, 4> out{};
  CHECK_EQ(ring.pop(out.data(), 4), 4U);
  for (std::size_t i = 0; i < out.size(); ++i) CHECK_EQ(out[i], kRamp[i]);
  CHECK_EQ(ring.size(), 0U);
}

TEST_CASE(test_audio_ring_drops_new_samples_when_full) {
  std::array<std::int16_t, 8> storage{};
  Ring ring{storage};
  CHECK_EQ(ring.push(kRamp.data(), 8), 0U);
  CHECK_EQ(ring.push(kRamp.data(), 4), 4U);
  CHECK_EQ(ring.size(), 8U);
}

TEST_CASE(test_audio_ring_wraps_without_losing_order) {
  std::array<std::int16_t, 8> storage{};
  Ring ring{storage};
  CHECK_EQ(ring.push(kRamp.data(), 6), 0U);

  std::array<std::int16_t, 6> out{};
  CHECK_EQ(ring.pop(out.data(), 6), 6U);
  CHECK_EQ(out[5], 5000);

  CHECK_EQ(ring.push(kRamp.data() + 6, 6), 0U);
  std::array<std::int16_t, 6> wrapped{};
  CHECK_EQ(ring.pop(wrapped.data(), 6), 6U);
  for (std::size_t i = 0; i < wrapped.size(); ++i) CHECK_EQ(wrapped[i], kRamp[6 + i]);
}

TEST_CASE(test_audio_ring_pop_is_bounded_by_available_samples) {
  std::array<std::int16_t, 8> storage{};
  Ring ring{storage};
  CHECK_EQ(ring.push(kRamp.data(), 3), 0U);
  std::array<std::int16_t, 8> out{};
  CHECK_EQ(ring.pop(out.data(), out.size()), 3U);
}

TEST_CASE(test_audio_ring_rejects_non_power_of_two_storage) {
  std::array<std::int16_t, 7> storage{};
  Ring ring{storage};
  CHECK_EQ(ring.capacity(), 0U);
  CHECK_EQ(ring.push(kRamp.data(), 4), 4U);
}

TEST_CASE(test_audio_gate_opens_above_threshold) {
  Gate gate;
  CHECK(!gate.open());
  CHECK_EQ(gate.feed(-20.0f), GateEvent::opened);
  CHECK(gate.open());
}

TEST_CASE(test_audio_gate_ignores_loudness_below_threshold) {
  Gate gate;
  CHECK_EQ(gate.feed(-60.0f), GateEvent::none);
  CHECK(!gate.open());
}

TEST_CASE(test_audio_gate_holds_open_during_short_pauses) {
  Gate gate;
  gate.feed(-20.0f);
  for (std::uint32_t i = 0; i + 1 < Gate::kHangoverFrames; ++i) {
    CHECK_EQ(gate.feed(-60.0f), GateEvent::none);
    CHECK(gate.open());
  }
  CHECK_EQ(gate.feed(-60.0f), GateEvent::closed);
  CHECK(!gate.open());
}

TEST_CASE(test_audio_gate_hysteresis_keeps_open_in_the_band) {
  Gate gate;
  gate.feed(-20.0f);
  for (std::uint32_t i = 0; i < Gate::kHangoverFrames * 2; ++i) {
    CHECK_EQ(gate.feed(-42.0f), GateEvent::none);
    CHECK(gate.open());
  }
}

TEST_CASE(test_audio_gate_reset_closes_immediately) {
  Gate gate;
  gate.feed(-20.0f);
  gate.reset();
  CHECK(!gate.open());
  CHECK_EQ(gate.feed(-20.0f), GateEvent::opened);
}
