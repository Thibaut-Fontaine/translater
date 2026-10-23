#include "harness.hpp"
#include "xtrans/pipeline.hpp"

using namespace xtrans;

TEST_CASE(test_pipeline_starts_idle) {
  Pipeline p;
  CHECK_EQ(p.state(), State::idle);
  CHECK(!p.busy());
}

TEST_CASE(test_pipeline_press_starts_recording) {
  Pipeline p;
  CHECK(p.press());
  CHECK_EQ(p.state(), State::recording);
  CHECK(p.busy());
}

TEST_CASE(test_pipeline_press_is_ignored_while_busy) {
  Pipeline p;
  CHECK(p.press());
  CHECK(!p.press());
  CHECK_EQ(p.state(), State::recording);
}

TEST_CASE(test_pipeline_release_moves_to_transcription) {
  Pipeline p;
  p.press();
  CHECK(p.release());
  CHECK_EQ(p.state(), State::transcribing);
}

TEST_CASE(test_pipeline_release_without_press_is_rejected) {
  Pipeline p;
  CHECK(!p.release());
  CHECK_EQ(p.state(), State::idle);
}

TEST_CASE(test_pipeline_happy_path_runs_every_stage) {
  Pipeline p;
  CHECK(p.press());
  CHECK(p.release());
  CHECK_EQ(p.state(), State::transcribing);

  CHECK(p.complete(Stage::stt, true));
  CHECK_EQ(p.state(), State::translating);

  CHECK(p.complete(Stage::mt, true));
  CHECK_EQ(p.state(), State::speaking);

  CHECK(p.complete(Stage::tts, true));
  CHECK_EQ(p.state(), State::idle);
}

TEST_CASE(test_pipeline_rejects_out_of_order_stage_completion) {
  Pipeline p;
  p.press();
  p.release();
  CHECK(!p.complete(Stage::mt, true));
  CHECK_EQ(p.state(), State::transcribing);
}

TEST_CASE(test_pipeline_stage_failure_returns_to_idle) {
  Pipeline p;
  p.press();
  p.release();
  CHECK(!p.complete(Stage::stt, false));
  CHECK_EQ(p.state(), State::idle);
}

TEST_CASE(test_pipeline_abort_between_press_and_release_discards_turn) {
  Pipeline p;
  p.press();
  CHECK(p.abort());
  CHECK_EQ(p.state(), State::idle);
  CHECK(!p.abort());
}

TEST_CASE(test_pipeline_abort_while_speaking_stops_playback) {
  Pipeline p;
  p.press();
  p.release();
  p.complete(Stage::stt, true);
  p.complete(Stage::mt, true);
  CHECK_EQ(p.state(), State::speaking);
  CHECK(p.abort());
  CHECK_EQ(p.state(), State::idle);
}

TEST_CASE(test_pipeline_counts_finished_and_failed_turns) {
  Pipeline p{};
  for (int i = 0; i < 3; ++i) {
    p.press();
    p.release();
    p.complete(Stage::stt, true);
    p.complete(Stage::mt, true);
    p.complete(Stage::tts, true);
  }
  p.press();
  p.release();
  p.complete(Stage::stt, false);

  CHECK_EQ(p.turns(), 4U);
  CHECK_EQ(p.failures(), 1U);
}
