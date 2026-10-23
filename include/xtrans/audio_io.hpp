#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace xtrans {

// Capture side of the device. Adapters (ALSA today) own the OS handle; the
// core only ever sees 16 kHz mono signed samples.
class AudioIn {
 public:
  virtual ~AudioIn() = default;

  // Copies up to out.size() samples. Returns 0 when nothing is ready yet.
  virtual std::size_t read(std::span<std::int16_t> out) noexcept = 0;

  // Sticky I/O error. A turn in progress is discarded, never dispatched.
  virtual bool failed() const noexcept = 0;
};

class AudioOut {
 public:
  virtual ~AudioOut() = default;

  virtual bool write(std::span<const std::int16_t> pcm) noexcept = 0;
  virtual void drain() noexcept = 0;
};

}  // namespace xtrans
