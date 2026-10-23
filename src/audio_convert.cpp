#include "xtrans/audio_convert.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace xtrans {

void float_to_pcm16(std::span<const float> in, std::span<std::int16_t> out) noexcept {
  const std::size_t n = std::min(in.size(), out.size());
  for (std::size_t i = 0; i < n; ++i) {
    const float clamped = std::clamp(in[i], -1.0F, 1.0F);
    const float scaled = clamped * 32768.0F;
    // Round away from zero; the cast truncates toward zero.
    const int value = static_cast<int>(scaled >= 0.0F ? scaled + 0.5F : scaled - 0.5F);
    out[i] = static_cast<std::int16_t>(
        std::clamp(value, static_cast<int>(std::numeric_limits<std::int16_t>::min()),
                   static_cast<int>(std::numeric_limits<std::int16_t>::max())));
  }
}

std::size_t resample_linear(std::span<const std::int16_t> in, std::uint32_t in_rate,
                            std::span<std::int16_t> out, std::uint32_t out_rate) noexcept {
  if (in.empty() || out.empty() || in_rate == 0 || out_rate == 0) return 0;

  const std::size_t wanted =
      std::min(out.size(), static_cast<std::size_t>(
                               (static_cast<std::uint64_t>(in.size()) * out_rate) / in_rate));
  if (wanted == 0) return 0;

  if (in_rate == out_rate) {
    std::copy_n(in.begin(), wanted, out.begin());
    return wanted;
  }

  const double step = static_cast<double>(in_rate) / static_cast<double>(out_rate);
  const auto last = static_cast<double>(in.size() - 1);
  for (std::size_t i = 0; i < wanted; ++i) {
    const double position = std::min(static_cast<double>(i) * step, last);
    const auto index = static_cast<std::size_t>(position);
    const double fraction = position - static_cast<double>(index);
    const double a = static_cast<double>(in[index]);
    const double b = index + 1 < in.size() ? static_cast<double>(in[index + 1]) : a;
    const double sample = a + (b - a) * fraction;
    out[i] = static_cast<std::int16_t>(std::lround(sample));
  }
  return wanted;
}

}  // namespace xtrans
