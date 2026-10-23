#include "harness.hpp"
#include "xtrans/audio_convert.hpp"

#include <array>
#include <cstdint>
#include <vector>

using namespace xtrans;

TEST_CASE(test_convert_float_to_pcm16_clamps_and_rounds) {
  const std::array<float, 5> in{0.0F, 1.0F, -1.0F, 0.5F, 2.0F};
  std::array<std::int16_t, 5> out{};
  float_to_pcm16(in, out);
  CHECK_EQ(out[0], 0);
  CHECK_EQ(out[1], 32767);
  CHECK_EQ(out[2], -32768);
  CHECK_EQ(out[3], 16384);
  CHECK_EQ(out[4], 32767);  // clamped
}

TEST_CASE(test_convert_float_to_pcm16_stops_at_shorter_output) {
  const std::array<float, 4> in{0.1F, 0.2F, 0.3F, 0.4F};
  std::array<std::int16_t, 2> out{};
  float_to_pcm16(in, out);
  CHECK_EQ(out[0], 3277);
  CHECK_EQ(out[1], 6554);
}

TEST_CASE(test_convert_resample_identity_keeps_samples) {
  const std::array<std::int16_t, 4> in{1, -2, 3, -4};
  std::array<std::int16_t, 4> out{};
  CHECK_EQ(resample_linear(in, 16000, out, 16000), 4U);
  for (std::size_t i = 0; i < in.size(); ++i) CHECK_EQ(out[i], in[i]);
}

TEST_CASE(test_convert_resample_down_by_two_halves_length) {
  const std::array<std::int16_t, 8> in{0, 100, 200, 300, 400, 500, 600, 700};
  std::array<std::int16_t, 8> out{};
  CHECK_EQ(resample_linear(in, 32000, out, 16000), 4U);
  CHECK_EQ(out[0], 0);
  CHECK_EQ(out[2], 400);
}

TEST_CASE(test_convert_resample_22050_to_16000_ratio) {
  std::vector<std::int16_t> in(22050, 1000);
  std::vector<std::int16_t> out(16000, 0);
  const std::size_t n = resample_linear(in, 22050, out, 16000);
  CHECK_EQ(n, 16000U);
  // A constant signal must stay constant through linear interpolation.
  for (std::size_t i = 0; i < n; ++i) CHECK_EQ(out[i], 1000);
}

TEST_CASE(test_convert_resample_handles_empty_and_tiny_input) {
  std::array<std::int16_t, 4> out{};
  CHECK_EQ(resample_linear(std::span<const std::int16_t>{}, 22050, out, 16000), 0U);

  // One input sample at a downsampling ratio does not fill a whole output
  // sample: the length is floor(n * ratio) by design.
  const std::array<std::int16_t, 2> two{42, 43};
  const std::size_t n = resample_linear(two, 22050, out, 16000);
  CHECK_EQ(n, 1U);
  CHECK_EQ(out[0], 42);
}
