#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "xtrans/audio_io.hpp"
#include "xtrans/buttons.hpp"
#include "xtrans/control.hpp"
#include "xtrans/recorder.hpp"

namespace xtrans {

// One non-blocking iteration of the device: buttons -> debounce -> turn
// start/stop, plus one capture frame while a turn is open. The daemon supplies
// the sleeping between ticks; tests call tick() directly.
class Device {
 public:
  Device(ButtonSource& buttons, AudioIn& audio, std::span<std::int16_t> capture) noexcept;

  // Returns true when a turn just ended (dispatched or discarded).
  bool tick() noexcept;

  const Controller& controller() const noexcept { return controller_; }
  bool recording() const noexcept { return recorder_.active(); }

  // Samples of the last finished turn; empty when it was discarded.
  std::span<const std::int16_t> last_turn() const noexcept { return last_turn_; }
  RecordResult last_result() const noexcept { return last_result_; }
  std::uint32_t turns() const noexcept { return turns_; }
  std::uint32_t discarded() const noexcept { return discarded_; }

 private:
  void start_turn() noexcept;
  void finish_turn(RecordResult result) noexcept;
  void drop_turn() noexcept;

  ButtonSource& buttons_;
  AudioIn& audio_;
  Debouncer debouncer_;
  Recorder recorder_;
  Controller controller_;
  std::array<std::int16_t, kFrameSamples> frame_{};
  std::span<const std::int16_t> last_turn_{};
  RecordResult last_result_ = RecordResult::recording;
  std::uint32_t turns_ = 0;
  std::uint32_t discarded_ = 0;
};

}  // namespace xtrans
