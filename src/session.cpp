#include "xtrans/session.hpp"

namespace xtrans {
namespace {

// Stable literals: the native bridge forwards them to the UI verbatim and the
// app maps them to localized strings.
constexpr const char* kNoSpeechError = "no speech";
constexpr const char* kRecognitionError = "recognition failed";
constexpr const char* kTranslationError = "translation failed";

std::size_t copy_bounded(std::string_view from, std::span<char> to) noexcept {
  const std::size_t n = from.size() < to.size() ? from.size() : to.size();
  for (std::size_t i = 0; i < n; ++i) to[i] = from[i];
  return n;
}

}  // namespace

Session::Session(Stt& stt, Mt& mt, Callbacks callbacks) noexcept
    : stt_(stt), mt_(mt), callbacks_(callbacks) {}

bool Session::configure(Lang from, Lang to, Mode mode) noexcept {
  if (pipeline_.busy()) return false;
  if (!can_translate(from, to)) return false;
  if (!supports(from, mode)) return false;

  from_ = from;
  to_ = to;
  mode_ = mode;
  configured_ = true;
  transcript_len_ = 0;
  translation_len_ = 0;
  return true;
}

bool Session::begin_voice() noexcept {
  if (!configured_) return false;
  if (!supports(from_, mode_)) return false;
  if (!pipeline_.press()) return false;
  emit_state();
  return true;
}

SessionStatus Session::finish_voice(std::span<const std::int16_t> pcm) noexcept {
  if (pipeline_.state() != State::recording) return SessionStatus::busy;
  pipeline_.release();
  emit_state();

  transcript_len_ = 0;
  translation_len_ = 0;

  std::size_t written = 0;
  const SttStatus recognized = stt_.transcribe(pcm, from_, transcript_, written);
  if (recognized != SttStatus::ok) {
    pipeline_.complete(Stage::stt, false);
    emit_state();
    if (recognized == SttStatus::no_speech) {
      emit_error(kNoSpeechError);
      return SessionStatus::no_speech;
    }
    emit_error(kRecognitionError);
    return SessionStatus::stt_failed;
  }

  transcript_len_ = written;
  if (callbacks_.on_transcript != nullptr) {
    callbacks_.on_transcript(callbacks_.ctx, transcript());
  }

  pipeline_.complete(Stage::stt, true);
  emit_state();  // translating
  return run_translation();
}

SessionStatus Session::translate_text(std::string_view text) noexcept {
  if (!configured_) return SessionStatus::unsupported_pair;
  if (pipeline_.busy()) return SessionStatus::busy;
  if (text.empty()) return SessionStatus::empty_text;
  if (!pipeline_.begin_text()) return SessionStatus::busy;

  const std::size_t n = copy_bounded(text, transcript_);
  transcript_len_ = n;
  translation_len_ = 0;
  if (callbacks_.on_transcript != nullptr) {
    callbacks_.on_transcript(callbacks_.ctx, transcript());
  }
  emit_state();  // translating
  return run_translation();
}

SessionStatus Session::run_translation() noexcept {
  if (transcript_len_ == 0) {
    pipeline_.abort();
    emit_state();
    emit_error(kNoSpeechError);
    return SessionStatus::no_speech;
  }

  std::size_t written = 0;
  const MtStatus translated =
      mt_.translate(transcript(), from_, to_, translation_, written);

  if (translated != MtStatus::ok) {
    pipeline_.complete(Stage::mt, false);
    emit_state();
    if (translated == MtStatus::input_empty) {
      emit_error(kNoSpeechError);
      return SessionStatus::no_speech;
    }
    emit_error(kTranslationError);
    return SessionStatus::mt_failed;
  }

  translation_len_ = written;
  if (callbacks_.on_translation != nullptr) {
    callbacks_.on_translation(callbacks_.ctx, translation());
  }

  // Speech (Piper) or the OS engine is outside the session; the state machine
  // still has to walk through `speaking` to reach idle.
  pipeline_.complete(Stage::mt, true);
  pipeline_.complete(Stage::tts, true);
  emit_state();
  return SessionStatus::ok;
}

void Session::cancel() noexcept {
  if (pipeline_.abort()) emit_state();
}

void Session::emit_state() noexcept {
  if (callbacks_.on_state != nullptr) callbacks_.on_state(callbacks_.ctx, pipeline_.state());
}

void Session::emit_error(const char* message) noexcept {
  if (callbacks_.on_error != nullptr) callbacks_.on_error(callbacks_.ctx, message);
}

}  // namespace xtrans
