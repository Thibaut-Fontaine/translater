#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace xtrans {

// A candidate block device, as enumerated from /sys/class/block.
struct BlockDevice {
  std::string name;   // "sdb"
  std::uint64_t bytes = 0;
  bool removable = false;
  bool mounted = false;
};

enum class TargetError : std::uint8_t {
  ok,
  not_removable,
  mounted,
  too_small,
  not_found,
  ambiguous,
  open_failed,
};

// The whole safety policy for choosing a flash target: removable, not mounted,
// and large enough. Anything else is refused before a single byte is written.
TargetError assess_target(const BlockDevice& device, std::uint64_t image_bytes) noexcept;

std::string_view target_error_text(TargetError error) noexcept;

// 0..100, clamped; an empty image counts as complete.
int progress_percent(std::uint64_t written, std::uint64_t total) noexcept;

}  // namespace xtrans
