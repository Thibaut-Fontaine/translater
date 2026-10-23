#include "xtrans/recorder.hpp"

#include <algorithm>

namespace xtrans {

Recorder::Recorder(std::span<std::int16_t> storage) noexcept : storage_(storage) {}

void Recorder::begin() noexcept {
  written_ = 0;
  active_ = !storage_.empty();
  speech_ = false;
  truncated_ = false;
  gate_.reset();
}

RecordResult Recorder::feed(std::span<const std::int16_t> frame) noexcept {
  if (!active_) return RecordResult::recording;

  const GateEvent event = gate_.feed(dbfs(frame));
  if (event == GateEvent::opened) speech_ = true;

  const std::size_t space = storage_.size() - written_;
  const std::size_t n = std::min(space, frame.size());
  std::copy_n(frame.begin(), static_cast<std::ptrdiff_t>(n),
              storage_.begin() + static_cast<std::ptrdiff_t>(written_));
  written_ += n;

  if (event == GateEvent::closed) {
    active_ = false;
    return RecordResult::stopped_silence;
  }

  if (n < frame.size()) {
    truncated_ = true;
    active_ = false;
    return RecordResult::stopped_full;
  }

  if (written_ == storage_.size()) {
    active_ = false;
    return RecordResult::stopped_full;
  }

  return RecordResult::recording;
}

RecordResult Recorder::stop() noexcept {
  active_ = false;
  return RecordResult::stopped_button;
}

}  // namespace xtrans
