#include "xtrans/audio.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace xtrans {

int rms(Pcm pcm) noexcept {
  if (pcm.empty()) return 0;
  std::int64_t sum = 0;
  for (const std::int16_t s : pcm) sum += static_cast<std::int64_t>(s) * s;
  const auto mean = static_cast<double>(sum) / static_cast<double>(pcm.size());
  constexpr int kMaxRms = 32767;  // -32768 alone would round to 32768
  return std::min(static_cast<int>(std::sqrt(mean)), kMaxRms);
}

float dbfs(Pcm pcm) noexcept {
  if (pcm.empty()) return kSilenceDbfs;
  constexpr double kFullScale = 32768.0;  // -32768 maps to 0 dBFS
  const double level = std::log10(static_cast<double>(rms(pcm)) / kFullScale) * 20.0;
  if (!(level > static_cast<double>(kSilenceDbfs))) return kSilenceDbfs;
  return static_cast<float>(level);
}

Ring::Ring(std::span<std::int16_t> storage) noexcept : buf_(storage) {
  const std::size_t n = storage.size();
  if (n != 0 && (n & (n - 1)) == 0 && n <= std::numeric_limits<std::uint32_t>::max()) {
    cap_ = n;
    mask_ = n - 1;
  }
}

std::size_t Ring::size() const noexcept {
  const auto w = write_.load(std::memory_order_relaxed);
  const auto r = read_.load(std::memory_order_acquire);
  return static_cast<std::size_t>(w - r);
}

std::size_t Ring::push(const std::int16_t* data, std::size_t n) noexcept {
  if (cap_ == 0) return n;
  const auto w = write_.load(std::memory_order_relaxed);
  const auto r = read_.load(std::memory_order_acquire);
  const std::size_t used = static_cast<std::size_t>(w - r);
  const std::size_t put = std::min(n, cap_ - used);
  for (std::size_t i = 0; i < put; ++i) {
    buf_[(static_cast<std::size_t>(w) + i) & mask_] = data[i];
  }
  write_.store(w + static_cast<std::uint32_t>(put), std::memory_order_release);
  return n - put;
}

std::size_t Ring::pop(std::int16_t* out, std::size_t n) noexcept {
  if (cap_ == 0) return 0;
  const auto r = read_.load(std::memory_order_relaxed);
  const auto w = write_.load(std::memory_order_acquire);
  const std::size_t avail = static_cast<std::size_t>(w - r);
  const std::size_t got = std::min(n, avail);
  for (std::size_t i = 0; i < got; ++i) {
    out[i] = buf_[(static_cast<std::size_t>(r) + i) & mask_];
  }
  read_.store(r + static_cast<std::uint32_t>(got), std::memory_order_release);
  return got;
}

GateEvent Gate::feed(float frame_dbfs) noexcept {
  if (!open_) {
    if (frame_dbfs >= cfg_.open_dbfs) {
      open_ = true;
      hold_ = 0;
      return GateEvent::opened;
    }
    return GateEvent::none;
  }

  if (frame_dbfs > cfg_.close_dbfs) {
    hold_ = 0;
    return GateEvent::none;
  }

  if (++hold_ >= kHangoverFrames) {
    open_ = false;
    hold_ = 0;
    return GateEvent::closed;
  }
  return GateEvent::none;
}

void Gate::reset() noexcept {
  open_ = false;
  hold_ = 0;
}

}  // namespace xtrans
