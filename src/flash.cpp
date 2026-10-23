#include "xtrans/flash.hpp"

namespace xtrans {

TargetError assess_target(const BlockDevice& device, std::uint64_t image_bytes) noexcept {
  if (!device.removable) return TargetError::not_removable;
  if (device.mounted) return TargetError::mounted;
  if (device.bytes < image_bytes) return TargetError::too_small;
  return TargetError::ok;
}

std::string_view target_error_text(TargetError error) noexcept {
  switch (error) {
    case TargetError::ok: return "ok";
    case TargetError::not_removable: return "target is not removable";
    case TargetError::mounted: return "target is mounted";
    case TargetError::too_small: return "image does not fit";
    case TargetError::not_found: return "device not found";
    case TargetError::ambiguous: return "device name is ambiguous";
    case TargetError::open_failed: return "cannot open the device";
  }
  return "unknown error";
}

int progress_percent(std::uint64_t written, std::uint64_t total) noexcept {
  if (total == 0) return 100;
  if (written >= total) return 100;
  return static_cast<int>((written * 100U) / total);
}

}  // namespace xtrans
