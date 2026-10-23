#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "xtrans/audio.hpp"

namespace xtrans {

enum class RecordResult : std::uint8_t {
  recording,
  stopped_silence,  // gate closed after speech
  stopped_full,     // capture buffer exhausted
  stopped_button,   // push-to-talk released (or capture failed)
};

// Accumulates one utterance into caller-owned storage. No allocation, no clock:
// duration is implied by the sample count, endpointing by the gate.
class Recorder {
 public:
  explicit Recorder(std::span<std::int16_t> storage) noexcept;

  void begin() noexcept;

  // Appends one frame and reports whether the turn continues.
  RecordResult feed(std::span<const std::int16_t> frame) noexcept;

  // Explicit end (button released). Idempotent.
  RecordResult stop() noexcept;

  bool active() const noexcept { return active_; }
  bool has_speech() const noexcept { return speech_; }

  // True when the last frame did not fit and was cut.
  bool truncated() const noexcept { return truncated_; }

  std::span<const std::int16_t> samples() const noexcept {
    return storage_.first(written_);
  }

 private:
  std::span<std::int16_t> storage_;
  Gate gate_{};
  std::size_t written_ = 0;
  bool active_ = false;
  bool speech_ = false;
  bool truncated_ = false;
};

}  // namespace xtrans
