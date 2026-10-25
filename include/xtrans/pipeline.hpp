#pragma once

#include <cstdint>

namespace xtrans {

// One turn: press -> record -> STT -> MT -> TTS -> idle. Single-threaded and
// deterministic; the daemon owns the threads, the pipeline owns the state.
enum class State : std::uint8_t { idle, recording, transcribing, translating, speaking };

enum class Stage : std::uint8_t { stt, mt, tts };

class Pipeline {
 public:
  // Starts a turn. Rejected unless idle: input is dropped, not queued.
  bool press() noexcept;

  // Ends the recording and dispatches the turn. Rejected unless recording.
  bool release() noexcept;

  // Starts a typed turn: idle -> translating, skipping capture and recognition.
  // Rejected unless idle.
  bool begin_text() noexcept;

  // Any state -> idle. Returns false when already idle.
  bool abort() noexcept;

  // Reports the end of a stage. `false` marks the turn failed and drops to idle.
  // Out-of-order reports are rejected and leave the state untouched.
  bool complete(Stage stage, bool ok) noexcept;

  State state() const noexcept { return state_; }
  bool busy() const noexcept { return state_ != State::idle; }

  // Turns dispatched since start, and of those, how many failed.
  std::uint32_t turns() const noexcept { return turns_; }
  std::uint32_t failures() const noexcept { return failures_; }

 private:
  State state_ = State::idle;
  std::uint32_t turns_ = 0;
  std::uint32_t failures_ = 0;
};

}  // namespace xtrans
