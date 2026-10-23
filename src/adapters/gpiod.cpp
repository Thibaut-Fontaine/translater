#include "xtrans/adapters/gpiod.hpp"

#include <gpiod.h>

#include <cerrno>
#include <cstring>

namespace xtrans {
namespace {

gpiod_line* request_line(gpiod_chip* chip, int offset, const GpioLines& lines,
                         std::string& error) noexcept {
  gpiod_line* line = gpiod_chip_get_line(chip, static_cast<unsigned int>(offset));
  if (line == nullptr) {
    error = "no such GPIO line";
    return nullptr;
  }

  // GPIOD_LINE_REQUEST_FLAG_* are enum constants, not macros: no #if guard.
  // libgpiod >= 1.6 is required (Debian bookworm ships 1.6.3).
  int flags = 0;
  if (lines.pull_up) flags |= static_cast<int>(GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP);
  if (lines.active_low) flags |= static_cast<int>(GPIOD_LINE_REQUEST_FLAG_ACTIVE_LOW);

  if (gpiod_line_request_input_flags(line, "xtrans", flags) < 0) {
    error = std::strerror(errno);
    return nullptr;
  }
  return line;
}

}  // namespace

struct GpiodButtons::Impl {
  gpiod_chip* chip = nullptr;
  gpiod_line* lang_a = nullptr;
  gpiod_line* lang_b = nullptr;
  gpiod_line* ptt = nullptr;
};

GpiodButtons::GpiodButtons() noexcept : impl_(std::make_unique<Impl>()) {}

GpiodButtons::~GpiodButtons() {
  // Releasing the chip releases every requested line.
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
  self->impl_->lang_a = request_line(self->impl_->chip, lines.lang_a, lines, error);
  self->impl_->lang_b = request_line(self->impl_->chip, lines.lang_b, lines, error);
  self->impl_->ptt = request_line(self->impl_->chip, lines.ptt, lines, error);
  if (self->impl_->lang_a == nullptr || self->impl_->lang_b == nullptr ||
      self->impl_->ptt == nullptr) {
    return nullptr;  // destructor closes the chip and frees the lines
  }

  return self;
}

bool GpiodButtons::poll(ButtonState& out) noexcept {
  // Active-low is requested from libgpiod, so the returned level is already
  // logical (1 = pressed); decode_levels still handles the -1 read error.
  const int a = gpiod_line_get_value(impl_->lang_a);
  const int b = gpiod_line_get_value(impl_->lang_b);
  const int p = gpiod_line_get_value(impl_->ptt);
  out = decode_levels(a, b, p, false);
  return true;
}

}  // namespace xtrans
