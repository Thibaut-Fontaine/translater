#pragma once

#if !defined(XTRANS_WITH_ALSA) || !XTRANS_WITH_ALSA
#error "xtrans/adapters/alsa.hpp requires a build with XTRANS_WITH_ALSA=1"
#endif

#include <cstdint>
#include <memory>
#include <span>
#include <string>

#include "xtrans/audio_io.hpp"

namespace xtrans {

// ALSA capture, 16 kHz mono S16_LE through the "default" plug, so a device that
// only runs at 48 kHz is resampled by ALSA instead of by us. A dedicated thread
// does the blocking readi() and pushes into a one-second lock-free ring; the
// consumer only pops samples, so a slow pipeline never stalls the microphone.
class AlsaIn final : public AudioIn {
 public:
  // Returns nullptr and fills `error` when the device cannot be opened.
  static std::unique_ptr<AlsaIn> open(const char* device, std::string& error) noexcept;

  ~AlsaIn() override;

  AlsaIn(const AlsaIn&) = delete;
  AlsaIn& operator=(const AlsaIn&) = delete;

  std::size_t read(std::span<std::int16_t> out) noexcept override;
  bool failed() const noexcept override;

  std::uint32_t overruns() const noexcept;

 private:
  AlsaIn() noexcept;
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// ALSA playback. write() blocks for the duration of the buffer, which is what
// speech playback wants; underruns are counted and recovered, never fatal.
class AlsaOut final : public AudioOut {
 public:
  static std::unique_ptr<AlsaOut> open(const char* device, std::string& error) noexcept;

  ~AlsaOut() override;

  AlsaOut(const AlsaOut&) = delete;
  AlsaOut& operator=(const AlsaOut&) = delete;

  bool write(std::span<const std::int16_t> pcm) noexcept override;
  void drain() noexcept override;

  std::uint32_t underruns() const noexcept;

 private:
  AlsaOut() noexcept;
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace xtrans
