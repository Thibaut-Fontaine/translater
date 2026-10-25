#include "xtrans/pipeline.hpp"

namespace xtrans {
namespace {

State expected_state(Stage stage) noexcept {
  switch (stage) {
    case Stage::stt: return State::transcribing;
    case Stage::mt: return State::translating;
    case Stage::tts: return State::speaking;
  }
  return State::idle;
}

State next_state(Stage stage) noexcept {
  switch (stage) {
    case Stage::stt: return State::translating;
    case Stage::mt: return State::speaking;
    case Stage::tts: return State::idle;
  }
  return State::idle;
}

}  // namespace

bool Pipeline::press() noexcept {
  if (state_ != State::idle) return false;
  state_ = State::recording;
  return true;
}

bool Pipeline::release() noexcept {
  if (state_ != State::recording) return false;
  state_ = State::transcribing;
  ++turns_;
  return true;
}

bool Pipeline::begin_text() noexcept {
  if (state_ != State::idle) return false;
  state_ = State::translating;
  ++turns_;
  return true;
}

bool Pipeline::abort() noexcept {
  if (state_ == State::idle) return false;
  state_ = State::idle;
  return true;
}

bool Pipeline::complete(Stage stage, bool ok) noexcept {
  if (state_ != expected_state(stage)) return false;
  if (!ok) {
    ++failures_;
    state_ = State::idle;
    return false;
  }
  state_ = next_state(stage);
  return true;
}

}  // namespace xtrans
