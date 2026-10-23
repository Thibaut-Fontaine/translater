#pragma once

#include <cstdint>

namespace xtrans {

// Monotonic deadline for the pipeline's stages. It is clock-free: callers pass
// the current time in milliseconds, so tests stay deterministic.
//
// Note: a native call that blocks forever cannot be interrupted by this; it
// guards against a stage that never reports, and the process supervisor covers
// a wedged adapter.
class Watchdog {
 public:
  explicit Watchdog(std::uint32_t timeout_ms) noexcept : timeout_(timeout_ms) {}

  void arm(std::uint64_t now_ms) noexcept {
    deadline_ = now_ms + timeout_;
    armed_ = true;
  }

  void disarm() noexcept { armed_ = false; }

  bool armed() const noexcept { return armed_; }

  bool expired(std::uint64_t now_ms) const noexcept {
    return armed_ && now_ms >= deadline_;
  }

  std::uint32_t timeout_ms() const noexcept { return timeout_; }

 private:
  std::uint32_t timeout_;
  std::uint64_t deadline_ = 0;
  bool armed_ = false;
};

}  // namespace xtrans
