#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace xtrans {

// Minimal 16-bit mono PCM WAV writer, used to verify synthesis on a machine
// without a speaker and to emit the boot prompt at provisioning time.
// Returns the number of bytes written, or 0 on failure.
std::size_t write_wav(const char* path, std::span<const std::int16_t> pcm,
                      std::uint32_t sample_rate) noexcept;

}  // namespace xtrans
