#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "xtrans/language.hpp"
#include "xtrans/mt.hpp"
#include "xtrans/pipeline.hpp"
#include "xtrans/stt.hpp"

namespace xtrans {

enum class SessionStatus : std::uint8_t {
  ok,
  busy,              // a turn is already open
  unsupported_pair,  // not configured, or no NLLB path for the pair
  empty_text,        // nothing to translate
  no_speech,         // recognizer produced no usable text
  stt_failed,
  mt_failed,
};

// Turns the three ports into the four calls a UI needs: configure, speak,
// finish, type, cancel. It owns no thread, no device and no allocation after
// construction: the caller (the Pi daemon on one side, a native module on the
// other) decides when to run it.
//
// The Pi keeps using Device + Recorder; the phone does not capture through the
// core, so it hands over a complete utterance and gets text back.
// Namespace scope so a default argument can use the initializers (a nested
// aggregate is still incomplete at that point).
struct SessionCallbacks {
  void* ctx = nullptr;
  void (*on_state)(void* ctx, State) = nullptr;
  void (*on_transcript)(void* ctx, std::string_view) = nullptr;
  void (*on_translation)(void* ctx, std::string_view) = nullptr;
  void (*on_error)(void* ctx, const char*) = nullptr;
};

class Session {
 public:
  static constexpr std::size_t kTextCapacity = 4096;
  using Callbacks = SessionCallbacks;

  Session(Stt& stt, Mt& mt, Callbacks callbacks = {}) noexcept;

  // Selects the pair and what the turn produces. False when NLLB cannot
  // translate the pair, or when the mode needs a component the pair lacks
  // (e.g. speech input for a language with no whisper code).
  bool configure(Lang from, Lang to, Mode mode) noexcept;

  Lang from() const noexcept { return from_; }
  Lang to() const noexcept { return to_; }
  Mode mode() const noexcept { return mode_; }

  // Press-to-talk down. False when busy or when the mode has no speech input.
  bool begin_voice() noexcept;

  // Press-to-talk up: transcribe, translate, then return to idle.
  SessionStatus finish_voice(std::span<const std::int16_t> pcm) noexcept;

  // Typed input: translate, then return to idle. No recognizer involved.
  SessionStatus translate_text(std::string_view text) noexcept;

  // Any state -> idle. Keeps the last transcript/translation on screen.
  void cancel() noexcept;

  State state() const noexcept { return pipeline_.state(); }
  std::string_view transcript() const noexcept {
    return std::string_view(transcript_.data(), transcript_len_);
  }
  std::string_view translation() const noexcept {
    return std::string_view(translation_.data(), translation_len_);
  }
  const Pipeline& pipeline() const noexcept { return pipeline_; }

 private:
  SessionStatus run_translation() noexcept;
  void emit_state() noexcept;
  void emit_error(const char* message) noexcept;

  Stt& stt_;
  Mt& mt_;
  Callbacks callbacks_;
  Pipeline pipeline_;
  Lang from_ = Lang::fr;
  Lang to_ = Lang::en;
  Mode mode_ = Mode::speech_to_text;
  bool configured_ = false;
  std::array<char, kTextCapacity> transcript_{};
  std::size_t transcript_len_ = 0;
  std::array<char, kTextCapacity> translation_{};
  std::size_t translation_len_ = 0;
};

}  // namespace xtrans
