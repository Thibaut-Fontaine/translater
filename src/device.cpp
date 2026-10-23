#include "xtrans/device.hpp"

namespace xtrans {

Device::Device(ButtonSource& buttons, AudioIn& audio, std::span<std::int16_t> capture) noexcept
    : buttons_(buttons), audio_(audio), recorder_(capture) {}

bool Device::tick() noexcept {
  ButtonState raw;
  if (buttons_.poll(raw)) {
    const Presses presses = debouncer_.feed(raw);
    for (std::uint8_t i = 0; i < presses.count; ++i) {
      const Press& press = presses.items[i];
      if (press.button != Button::ptt) {
        controller_.apply(press.button == Button::lang_a ? Command::lang_a : Command::lang_b);
        continue;
      }
      if (press.edge == Edge::down) {
        start_turn();
      } else if (recorder_.active()) {
        finish_turn(recorder_.stop());
        return true;
      }
    }
  }

  if (!recorder_.active()) return false;

  if (audio_.failed()) {
    drop_turn();
    return true;
  }

  const std::size_t got = audio_.read(frame_);
  if (got == 0) return false;

  const RecordResult result = recorder_.feed(std::span<const std::int16_t>(frame_).first(got));
  if (result == RecordResult::recording) return false;

  finish_turn(result);
  return true;
}

void Device::start_turn() noexcept {
  if (recorder_.active()) return;
  recorder_.begin();
  last_turn_ = {};
  controller_.apply(Command::press);
}

void Device::finish_turn(RecordResult result) noexcept {
  last_result_ = result;
  if (!recorder_.has_speech()) {
    drop_turn();
    return;
  }
  last_turn_ = recorder_.samples();
  ++turns_;
  controller_.apply(Command::release);
}

void Device::drop_turn() noexcept {
  recorder_.stop();
  last_result_ = RecordResult::stopped_button;
  last_turn_ = {};
  controller_.apply(Command::abort);
  ++discarded_;
}

}  // namespace xtrans
