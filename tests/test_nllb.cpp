#include "harness.hpp"
#include "xtrans/nllb_ids.hpp"

#include <array>
#include <cstdint>

using namespace xtrans;

TEST_CASE(test_nllb_ids_shift_round_trips) {
  CHECK_EQ(nllb_to_model_id(0), 1);
  CHECK_EQ(nllb_to_model_id(94123), 94124);
  CHECK_EQ(nllb_to_sp_id(nllb_to_model_id(94123)), 94123);
}

TEST_CASE(test_nllb_ids_special_range_starts_at_vocab_size) {
  CHECK(!nllb_is_special(255999));
  CHECK(nllb_is_special(256000));
  CHECK(nllb_is_special(256057));  // fra_Latn
}

TEST_CASE(test_nllb_source_ids_prefix_shift_and_eos) {
  const std::array<std::int32_t, 2> pieces{94123, 4};
  std::array<std::int64_t, 8> out{};
  const std::size_t n = nllb_build_source_ids(pieces, 256047, out);
  CHECK_EQ(n, 4U);
  CHECK_EQ(out[0], 256047);  // source language
  CHECK_EQ(out[1], 94124);   // shifted piece
  CHECK_EQ(out[2], 5);
  CHECK_EQ(out[3], kNllbEos);
}

TEST_CASE(test_nllb_source_ids_handle_empty_text) {
  std::array<std::int64_t, 4> out{};
  const std::size_t n =
      nllb_build_source_ids(std::span<const std::int32_t>{}, 256057, out);
  CHECK_EQ(n, 2U);
  CHECK_EQ(out[0], 256057);
  CHECK_EQ(out[1], kNllbEos);
}

TEST_CASE(test_nllb_source_ids_report_overflow_without_writing) {
  const std::array<std::int32_t, 3> pieces{1, 2, 3};
  std::array<std::int64_t, 4> out{};
  out.fill(-1);
  CHECK_EQ(nllb_build_source_ids(pieces, 256047, out), 0U);
  for (const std::int64_t v : out) CHECK_EQ(v, -1);
}

TEST_CASE(test_nllb_filter_drops_specials_and_unshifts) {
  const std::array<std::int64_t, 5> generated{256057, 94124, 3, 0, 256175};
  std::array<std::int32_t, 5> out{};
  const std::size_t n = nllb_filter_output(generated, out);
  CHECK_EQ(n, 2U);
  CHECK_EQ(out[0], 94123);
  CHECK_EQ(out[1], 2);
}

TEST_CASE(test_nllb_filter_reports_overflow_without_writing) {
  const std::array<std::int64_t, 3> generated{94124, 94125, 94126};
  std::array<std::int32_t, 2> out{};
  out.fill(-7);
  CHECK_EQ(nllb_filter_output(generated, out), 0U);
  CHECK_EQ(out[0], -7);
  CHECK_EQ(out[1], -7);
}
