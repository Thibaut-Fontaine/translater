#include "harness.hpp"
#include "xtrans/session.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

using namespace xtrans;

namespace {

// Fakes implement the ports, not the models: the session must be testable with
// no audio device, no model file and no NN runtime.
struct FakeStt final : Stt {
  SttStatus status = SttStatus::ok;
  std::string_view text = "bonjour";
  int calls = 0;

  SttStatus transcribe(std::span<const std::int16_t>, Lang, std::span<char> out,
                       std::size_t& written) noexcept override {
    ++calls;
    written = 0;
    if (status != SttStatus::ok) return status;
    std::size_t n = text.size() < out.size() ? text.size() : out.size();
    for (std::size_t i = 0; i < n; ++i) out[i] = text[i];
    written = n;
    return SttStatus::ok;
  }
};

struct FakeMt final : Mt {
  MtStatus status = MtStatus::ok;
  std::string_view text = "hello";
  int calls = 0;
  Lang seen_from = Lang::fr;
  Lang seen_to = Lang::en;
  std::string seen_input;

  MtStatus translate(std::string_view input, Lang from, Lang to, std::span<char> out,
                     std::size_t& written) noexcept override {
    ++calls;
    seen_from = from;
    seen_to = to;
    seen_input.assign(input);
    written = 0;
    if (status != MtStatus::ok) return status;
    std::size_t n = text.size() < out.size() ? text.size() : out.size();
    for (std::size_t i = 0; i < n; ++i) out[i] = text[i];
    written = n;
    return MtStatus::ok;
  }
};

struct Recorder {
  std::vector<State> states;
  std::string transcript;
  std::string translation;
  std::string error;

  static void state(void* ctx, State s) noexcept {
    static_cast<Recorder*>(ctx)->states.push_back(s);
  }
  static void on_transcript(void* ctx, std::string_view text) noexcept {
    static_cast<Recorder*>(ctx)->transcript.assign(text);
  }
  static void on_translation(void* ctx, std::string_view text) noexcept {
    static_cast<Recorder*>(ctx)->translation.assign(text);
  }
  static void on_error(void* ctx, const char* text) noexcept {
    static_cast<Recorder*>(ctx)->error.assign(text);
  }

  Session::Callbacks callbacks() noexcept {
    Session::Callbacks c;
    c.ctx = this;
    c.on_state = &Recorder::state;
    c.on_transcript = &Recorder::on_transcript;
    c.on_translation = &Recorder::on_translation;
    c.on_error = &Recorder::on_error;
    return c;
  }
};

constexpr std::array<std::int16_t, 4> kPcm{1, 2, 3, 4};

}  // namespace

TEST_CASE(test_session_text_turn_translates_without_a_recognizer) {
  FakeStt stt;
  FakeMt mt;
  Recorder rec;
  Session session{stt, mt, rec.callbacks()};

  CHECK(session.configure(Lang::fr, Lang::en, Mode::text_to_text));
  CHECK_EQ(session.translate_text("bonjour"), SessionStatus::ok);
  CHECK_EQ(stt.calls, 0);
  CHECK_EQ(mt.calls, 1);
  CHECK_EQ(rec.translation, std::string("hello"));
  CHECK_EQ(rec.transcript, std::string("bonjour"));  // typed text is shown as the source
  CHECK_EQ(session.translation(), std::string_view("hello"));
  CHECK_EQ(session.state(), State::idle);
  CHECK_EQ(mt.seen_from, Lang::fr);
  CHECK_EQ(mt.seen_to, Lang::en);
}

TEST_CASE(test_session_text_turn_accepts_a_pair_with_no_voice_or_recognizer) {
  FakeStt stt;
  FakeMt mt;
  Recorder rec;
  Session session{stt, mt, rec.callbacks()};

  // Acehnese has an NLLB token but neither a whisper code nor a Piper voice.
  CHECK(session.configure(Lang::ace_Arab, Lang::fr, Mode::text_to_text));
  CHECK_EQ(session.translate_text("assalamualaikum"), SessionStatus::ok);
  CHECK_EQ(mt.calls, 1);
}

TEST_CASE(test_session_rejects_a_pair_nllb_cannot_translate) {
  FakeStt stt;
  FakeMt mt;
  Recorder rec;
  Session session{stt, mt, rec.callbacks()};

  CHECK(!session.configure(Lang::hmn, Lang::fr, Mode::text_to_text));
  CHECK_EQ(session.translate_text("hello"), SessionStatus::unsupported_pair);
  CHECK_EQ(mt.calls, 0);
}

TEST_CASE(test_session_rejects_speech_mode_without_a_recognizer) {
  FakeStt stt;
  FakeMt mt;
  Recorder rec;
  Session session{stt, mt, rec.callbacks()};

  CHECK(!session.configure(Lang::ace_Arab, Lang::fr, Mode::speech_to_text));
  CHECK(session.configure(Lang::ace_Arab, Lang::fr, Mode::text_to_text));
}

TEST_CASE(test_session_empty_text_never_reaches_the_translator) {
  FakeStt stt;
  FakeMt mt;
  Recorder rec;
  Session session{stt, mt, rec.callbacks()};

  CHECK(session.configure(Lang::fr, Lang::en, Mode::text_to_text));
  CHECK_EQ(session.translate_text(""), SessionStatus::empty_text);
  CHECK_EQ(mt.calls, 0);
  CHECK_EQ(session.state(), State::idle);
}

TEST_CASE(test_session_voice_turn_runs_recognizer_then_translator) {
  FakeStt stt;
  stt.text = "bonjour tout le monde";
  FakeMt mt;
  mt.text = "hello everyone";
  Recorder rec;
  Session session{stt, mt, rec.callbacks()};

  CHECK(session.configure(Lang::fr, Lang::en, Mode::speech_to_text));
  CHECK(session.begin_voice());
  CHECK_EQ(session.state(), State::recording);
  CHECK_EQ(session.finish_voice(kPcm), SessionStatus::ok);
  CHECK_EQ(stt.calls, 1);
  CHECK_EQ(mt.calls, 1);
  CHECK_EQ(mt.seen_input, std::string("bonjour tout le monde"));
  CHECK_EQ(rec.transcript, std::string("bonjour tout le monde"));
  CHECK_EQ(rec.translation, std::string("hello everyone"));
  CHECK_EQ(session.state(), State::idle);

  // The UI sees the transcript before the translation.
  CHECK(rec.states.size() >= 4U);
  if (!rec.states.empty()) {
    CHECK_EQ(rec.states.front(), State::recording);
    CHECK(std::find(rec.states.begin(), rec.states.end(), State::transcribing) !=
          rec.states.end());
    CHECK(std::find(rec.states.begin(), rec.states.end(), State::translating) !=
          rec.states.end());
    CHECK_EQ(rec.states.back(), State::idle);
  }
}

TEST_CASE(test_session_voice_turn_without_speech_skips_translation) {
  FakeStt stt;
  stt.status = SttStatus::no_speech;
  FakeMt mt;
  Recorder rec;
  Session session{stt, mt, rec.callbacks()};

  CHECK(session.configure(Lang::fr, Lang::en, Mode::speech_to_text));
  CHECK(session.begin_voice());
  CHECK_EQ(session.finish_voice(kPcm), SessionStatus::no_speech);
  CHECK_EQ(stt.calls, 1);
  CHECK_EQ(mt.calls, 0);
  CHECK_EQ(rec.error, std::string("no speech"));
  CHECK_EQ(session.state(), State::idle);
}

TEST_CASE(test_session_recognition_failure_is_reported_and_clears_the_turn) {
  FakeStt stt;
  stt.status = SttStatus::failed;
  FakeMt mt;
  Recorder rec;
  Session session{stt, mt, rec.callbacks()};

  CHECK(session.configure(Lang::fr, Lang::en, Mode::speech_to_text));
  CHECK(session.begin_voice());
  CHECK_EQ(session.finish_voice(kPcm), SessionStatus::stt_failed);
  CHECK_EQ(rec.error, std::string("recognition failed"));
  CHECK_EQ(session.state(), State::idle);
}

TEST_CASE(test_session_translation_failure_is_reported) {
  FakeStt stt;
  FakeMt mt;
  mt.status = MtStatus::failed;
  Recorder rec;
  Session session{stt, mt, rec.callbacks()};

  CHECK(session.configure(Lang::fr, Lang::en, Mode::text_to_text));
  CHECK_EQ(session.translate_text("bonjour"), SessionStatus::mt_failed);
  CHECK_EQ(rec.error, std::string("translation failed"));
  CHECK_EQ(session.state(), State::idle);
}

TEST_CASE(test_session_second_voice_turn_is_rejected_while_recording) {
  FakeStt stt;
  FakeMt mt;
  Session session{stt, mt};
  CHECK(session.configure(Lang::fr, Lang::en, Mode::speech_to_text));
  CHECK(session.begin_voice());
  CHECK(!session.begin_voice());
  session.cancel();
  CHECK_EQ(session.state(), State::idle);
}

TEST_CASE(test_session_cancel_mid_turn_returns_to_idle_and_keeps_callbacks_optional) {
  FakeStt stt;
  FakeMt mt;
  Session session{stt, mt};  // no callbacks: must not crash
  CHECK(session.configure(Lang::fr, Lang::en, Mode::speech_to_text));
  CHECK(session.begin_voice());
  session.cancel();
  CHECK_EQ(session.state(), State::idle);
  CHECK_EQ(session.finish_voice(kPcm), SessionStatus::busy);
}

TEST_CASE(test_session_language_change_clears_the_last_result) {
  FakeStt stt;
  FakeMt mt;
  Session session{stt, mt};
  CHECK(session.configure(Lang::fr, Lang::en, Mode::text_to_text));
  CHECK_EQ(session.translate_text("bonjour"), SessionStatus::ok);
  CHECK(!session.translation().empty());

  CHECK(session.configure(Lang::en, Lang::fr, Mode::text_to_text));
  CHECK(session.translation().empty());
  CHECK(session.transcript().empty());
  CHECK_EQ(session.from(), Lang::en);
  CHECK_EQ(session.to(), Lang::fr);
}
