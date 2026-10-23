#include "harness.hpp"
#include "xtrans/flash.hpp"

#include <cstdint>

using namespace xtrans;

namespace {

BlockDevice device(bool removable = true, bool mounted = false, std::uint64_t bytes = 64ULL << 30) {
  BlockDevice d;
  d.name = "sdb";
  d.bytes = bytes;
  d.removable = removable;
  d.mounted = mounted;
  return d;
}

}  // namespace

TEST_CASE(test_flash_accepts_a_removable_unmounted_card) {
  CHECK_EQ(assess_target(device(), 4ULL << 30), TargetError::ok);
}

TEST_CASE(test_flash_refuses_a_fixed_disk) {
  CHECK_EQ(assess_target(device(false), 4ULL << 30), TargetError::not_removable);
}

TEST_CASE(test_flash_refuses_a_mounted_target) {
  CHECK_EQ(assess_target(device(true, true), 4ULL << 30), TargetError::mounted);
}

TEST_CASE(test_flash_refuses_an_image_larger_than_the_card) {
  CHECK_EQ(assess_target(device(true, false, 2ULL << 30), 4ULL << 30), TargetError::too_small);
}

TEST_CASE(test_flash_accepts_an_image_that_exactly_fits) {
  CHECK_EQ(assess_target(device(true, false, 4ULL << 30), 4ULL << 30), TargetError::ok);
}

TEST_CASE(test_flash_reports_a_human_reason) {
  CHECK_EQ(target_error_text(TargetError::ok), std::string_view("ok"));
  CHECK_EQ(target_error_text(TargetError::not_removable), std::string_view("target is not removable"));
  CHECK_EQ(target_error_text(TargetError::mounted), std::string_view("target is mounted"));
  CHECK_EQ(target_error_text(TargetError::too_small), std::string_view("image does not fit"));
}

TEST_CASE(test_flash_progress_is_bounded_and_monotonic) {
  CHECK_EQ(progress_percent(0, 1000), 0);
  CHECK_EQ(progress_percent(500, 1000), 50);
  CHECK_EQ(progress_percent(1000, 1000), 100);
  CHECK_EQ(progress_percent(2000, 1000), 100);  // never above 100
  CHECK_EQ(progress_percent(0, 0), 100);        // nothing to write
}
