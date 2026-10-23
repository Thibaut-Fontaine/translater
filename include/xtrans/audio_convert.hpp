#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace xtrans {

// Piper returns float samples in [-1, 1]; the pipeline is int16 everywhere.
void float_to_pcm16(std::span<const float> in, std::span<std::int16_t> out) noexcept;

// Linear resampling to the pipeline rate (16 kHz). Voices are 22.05 kHz
// typically; doing it here keeps AudioOut at one fixed format. Returns the
// number of output samples written.
std::size_t resample_linear(std::span<const std::int16_t> in, std::uint32_t in_rate,
                            std::span<std::int16_t> out, std::uint32_t out_rate) noexcept;

}  // namespace xtrans
