#include "xtrans/adapters/gpiod.hpp"

#include <gpiod.h>

#include <cerrno>
#include <cstring>

namespace xtrans {
namespace {

constexpr gpiod_line_direction kDirection = GPIOD_LINE_DIRECTION_INPUT;

}  // namespace

struct GpiodButtons::Impl {
  gpiod_chip* chip = nullptr;
  gpiod_line_request* request = nullptr;
  unsigned int offsets[3] = {0, 1, 2};
};

GpiodButtons::GpiodButtons() noexcept : impl_(std::make_unique<Impl>()) {}

GpiodButtons::~GpiodButtons() {
  if (impl_->request != nullptr) gpiod_line_request_release(impl_->request);
  if (impl_->chip != nullptr) gpiod_chip_close(impl_->chip);
}

std::unique_ptr<GpiodButtons> GpiodButtons::open(const char* chip_path, const GpioLines& lines,
                                                 std::string& error) noexcept {
  std::unique_ptr<GpiodButtons> self(new (std::nothrow) GpiodButtons());
  if (!self) {
    error = "out of memory";
    return nullptr;
  }

  self->impl_->chip = gpiod_chip_open(chip_path);
  if (self->impl_->chip == nullptr) {
    error = std::strerror(errno);
    return nullptr;
  }

  gpiod_line_settings* settings = gpiod_line_settings_new();
  gpiod_line_config* line_config = gpiod_line_config_new();
  gpiod_request_config* request_config = gpiod_request_config_new();
  if (settings == nullptr || line_config == nullptr || request_config == nullptr) {
    error = "out of memory";
    gpiod_line_settings_free(settings);
    gpiod_line_config_free(line_config);
    gpiod_request_config_free(request_config);
    return nullptr;
  }

  gpiod_line_settings_set_direction(settings, kDirection);
  gpiod_line_settings_set_active_low(settings, lines.active_low);
  // Without the bias the line floats and a button can read pressed forever,
  // which would wedge the device in recording.
  gpiod_line_settings_set_bias(
      settings, lines.pull_up ? GPIOD_LINE_BIAS_PULL_UP : GPIOD_LINE_BIAS_AS_IS);

  self->impl_->offsets[0] = static_cast<unsigned int>(lines.lang_a);
  self->impl_->offsets[1] = static_cast<unsigned int>(lines.lang_b);
  self->impl_->offsets[2] = static_cast<unsigned int>(lines.ptt);

  int rc = gpiod_line_config_add_line_settings(line_config, self->impl_->offsets, 3, settings);
  if (rc == 0) {
    gpiod_request_config_set_consumer(request_config, "xtrans");
    self->impl_->request = gpiod_chip_request_lines(self->impl_->chip, request_config, line_config);
    if (self->impl_->request == nullptr) rc = -1;
  }

  if (rc != 0) {
    error = std::strerror(errno);
    gpiod_line_settings_free(settings);
    gpiod_line_config_free(line_config);
    gpiod_request_config_free(request_config);
    return nullptr;
  }

  gpiod_line_settings_free(settings);
  gpiod_line_config_free(line_config);
  gpiod_request_config_free(request_config);
  return self;
}

bool GpiodButtons::poll(ButtonState& out) noexcept {
  // Active-low is requested at the kernel level, so the values are already
  // logical (1 = pressed). A failed read returns -1 and decode_levels treats it
  // as released.
  const int a = gpiod_line_request_get_value(impl_->request, impl_->offsets[0]);
  const int b = gpiod_line_request_get_value(impl_->request, impl_->offsets[1]);
  const int p = gpiod_line_request_get_value(impl_->request, impl_->offsets[2]);
  out = decode_levels(a, b, p, false);
  return true;
}

}  // namespace xtrans
