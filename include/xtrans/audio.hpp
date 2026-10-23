#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>

namespace xtrans {

// Mono, 16-bit little-endian PCM. The capture path is locked to this rate so
// Whisper and the gate share one buffer format.
inline constexpr std::uint32_t kSampleRate = 16000;
inline constexpr std::uint32_t kFrameSamples = 320;  // 20 ms at 16 kHz
inline constexpr float kSilenceDbfs = -120.0f;

using Pcm = std::span<const std::int16_t>;

// Root-mean-square level, 0..32767. Empty input yields 0.
int rms(Pcm pcm) noexcept;

// Level in dBFS, floored at kSilenceDbfs. 32767 maps to ~0 dBFS.
float dbfs(Pcm pcm) noexcept;

// Lock-free single-producer/single-consumer ring of samples.
// Storage is owned by the caller and must be a power-of-two length; anything
// else yields a zero-capacity ring that drops every sample.
class Ring {
 public:
  explicit Ring(std::span<std::int16_t> storage) noexcept;

  std::size_t capacity() const noexcept { return cap_; }
  std::size_t size() const noexcept;

  // Copies as much as fits and returns how many samples were refused. The
  // producer never overwrites unread samples: a slow consumer loses new audio
  // instead of corrupting the stream.
  std::size_t push(const std::int16_t* data, std::size_t n) noexcept;

  // Copies up to `n` samples and returns how many were read.
  std::size_t pop(std::int16_t* out, std::size_t n) noexcept;

 private:
  std::span<std::int16_t> buf_;
  std::size_t mask_ = 0;
  std::size_t cap_ = 0;
  std::atomic<std::uint32_t> write_{0};
  std::atomic<std::uint32_t> read_{0};
};

enum class GateEvent : std::uint8_t { none, opened, closed };

struct GateConfig {
  float open_dbfs = -40.0f;
  float close_dbfs = -45.0f;
};

// Voice activity gate with hysteresis and hangover. Opening is immediate;
// closing waits kHangoverFrames quiet frames so short pauses inside a sentence
// do not cut the utterance in two.
class Gate {
 public:
  static constexpr std::uint32_t kHangoverFrames = 25;  // 500 ms at 20 ms frames

  explicit Gate(GateConfig cfg = {}) noexcept : cfg_(cfg) {}

  GateEvent feed(float frame_dbfs) noexcept;
  bool open() const noexcept { return open_; }
  void reset() noexcept;

 private:
  GateConfig cfg_;
  std::uint32_t hold_ = 0;
  bool open_ = false;
};

}  // namespace xtrans
