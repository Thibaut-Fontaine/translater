#pragma once

#if !defined(XTRANS_WITH_GPIOD) || !XTRANS_WITH_GPIOD
#error "xtrans/adapters/gpiod.hpp requires a build with XTRANS_WITH_GPIOD=1"
#endif

#include <memory>
#include <string>

#include "xtrans/buttons.hpp"

namespace xtrans {

// Physical wiring. Defaults are BCM 17/27/22 (header pins 11/13/15), each button
// between the line and GND, relying on the internal pull-up.
struct GpioLines {
  int lang_a = 17;
  int lang_b = 27;
  int ptt = 22;
  bool active_low = true;
  bool pull_up = true;
};

// libgpiod v1 line reader. Polling three lines costs three ioctls per tick;
// at 200 ticks/s that is noise on a Pi 5, and it keeps the adapter stateless.
class GpiodButtons final : public ButtonSource {
 public:
  static std::unique_ptr<GpiodButtons> open(const char* chip_path, const GpioLines& lines,
                                            std::string& error) noexcept;

  ~GpiodButtons() override;

  GpiodButtons(const GpiodButtons&) = delete;
  GpiodButtons& operator=(const GpiodButtons&) = delete;

  bool poll(ButtonState& out) noexcept override;

 private:
  GpiodButtons() noexcept;
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace xtrans
