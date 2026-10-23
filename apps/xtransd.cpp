// xtransd — headless controller for the portable translator.
//
//   --stdin      control lines on stdin (no hardware, used by the container)
//   --device     real buttons, microphone and speaker
//   --selftest   run a sentence through TTS -> STT -> MT -> TTS, no hardware
//
// NLLB-200 is CC-BY-NC-4.0: personal use only.
#include <algorithm>
#include <array>
#include <csignal>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>
#include <span>
#include <string_view>
#include <vector>

#include "xtrans/control.hpp"
#include "xtrans/device.hpp"
#include "xtrans/mt.hpp"
#include "xtrans/stt.hpp"
#include "xtrans/tts.hpp"
#include "xtrans/watchdog.hpp"
#include "xtrans/wav.hpp"

#if XTRANS_WITH_ALSA && XTRANS_WITH_GPIOD
#include "xtrans/adapters/alsa.hpp"
#include "xtrans/adapters/gpiod.hpp"
#endif

#if XTRANS_WITH_WHISPER
#include "xtrans/adapters/whisper.hpp"
#endif

#if XTRANS_WITH_NLLB
#include "xtrans/adapters/nllb.hpp"
#endif

#if XTRANS_WITH_PIPER
#include "xtrans/adapters/piper.hpp"
#endif

namespace {

volatile std::sig_atomic_t g_stop = 0;

extern "C" void handle_signal(int) { g_stop = 1; }

constexpr std::uint32_t kDefaultStageTimeoutMs = 5000;
constexpr std::size_t kTranscriptMax = 1024;
constexpr std::size_t kSpeechMax = xtrans::kSampleRate * 30;

struct Options {
  bool device = false;
  bool selftest = false;
  const char* model = nullptr;
  const char* mt_dir = nullptr;
  const char* voice_dir = nullptr;
  const char* espeak_data = nullptr;
  const char* out_dir = "/tmp";
  std::uint32_t stage_timeout_ms = kDefaultStageTimeoutMs;
};

void usage(const char* argv0) {
  std::printf(
      "usage: %s [--stdin | --device | --selftest] [options]\n"
      "\n"
      "  --stdin               read control lines (no hardware)\n"
      "  --device              buttons, microphone, speaker\n"
      "  --selftest            TTS -> STT -> MT -> TTS, writes WAVs, no hardware\n"
      "  --model FILE          whisper.cpp model\n"
      "  --mt-dir DIR          NLLB ONNX files and tokenizer\n"
      "  --voice-dir DIR       Piper voices\n"
      "  --espeak-data DIR     espeak-ng data directory\n"
      "  --out DIR             selftest output directory (default /tmp)\n"
      "  --stage-timeout MS    per-stage watchdog (default %u)\n"
      "\n"
      "Exits on EOF (--stdin), SIGINT or SIGTERM. Logs stage status and\n"
      "lengths only, never transcript or translation content.\n",
      argv0, kDefaultStageTimeoutMs);
}

#if XTRANS_WITH_ALSA && XTRANS_WITH_GPIOD
std::uint64_t now_ms() {
  timespec ts{};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<std::uint64_t>(ts.tv_sec) * 1000U +
         static_cast<std::uint64_t>(ts.tv_nsec) / 1000000U;
}
#endif

void emit(const xtrans::Controller& controller) {
  char line[xtrans::kStatusMax];
  const std::size_t n = xtrans::format_status(line, sizeof line, controller);
  if (n == 0) return;
  std::fwrite(line, 1, n, stdout);
  std::fputc('\n', stdout);
  std::fflush(stdout);
}

int run_stdin() {
  xtrans::Controller controller;
  emit(controller);

  char line[64];
  while (g_stop == 0) {
    if (std::fgets(line, sizeof line, stdin) == nullptr) {
      if (std::feof(stdin) != 0) break;
      continue;
    }
    const xtrans::Command command = xtrans::parse_command(line);
    if (command == xtrans::Command::none) {
      std::fputs("error\n", stdout);
      std::fflush(stdout);
      continue;
    }
    if (!controller.apply(command)) {
      std::fputs("rejected\n", stdout);
      std::fflush(stdout);
      continue;
    }
    emit(controller);
    if (controller.quit()) break;
  }
  return 0;
}

// --- adapters -------------------------------------------------------------

struct Engines {
  std::unique_ptr<xtrans::Stt> stt;
  std::unique_ptr<xtrans::Mt> mt;
  std::unique_ptr<xtrans::Tts> tts;
};

int open_engines(const Options& options, Engines& engines) {
#if XTRANS_WITH_WHISPER
  if (options.model != nullptr) {
    std::string error;
    auto whisper = xtrans::WhisperStt::open(options.model, error);
    if (!whisper) {
      std::fprintf(stderr, "xtransd: stt: %s\n", error.c_str());
      return 3;
    }
    engines.stt = std::move(whisper);
  }
#else
  if (options.model != nullptr) {
    std::fputs("xtransd: built without whisper.cpp (no --model)\n", stderr);
    return 3;
  }
#endif

#if XTRANS_WITH_NLLB
  if (options.mt_dir != nullptr) {
    std::string error;
    char encoder[512];
    char decoder[512];
    char decoder_with_past[512];
    char tokenizer[512];
    std::snprintf(encoder, sizeof encoder, "%s/encoder_model_int8.onnx", options.mt_dir);
    std::snprintf(decoder, sizeof decoder, "%s/decoder_model_quantized.onnx", options.mt_dir);
    std::snprintf(decoder_with_past, sizeof decoder_with_past,
                  "%s/decoder_with_past_model_quantized.onnx", options.mt_dir);
    std::snprintf(tokenizer, sizeof tokenizer, "%s/sentencepiece.bpe.model", options.mt_dir);

    xtrans::NllbConfig config;
    config.encoder = encoder;
    config.decoder = decoder;
    config.decoder_with_past = decoder_with_past;
    config.tokenizer = tokenizer;
    auto nllb = xtrans::NllbMt::open(config, error);
    if (!nllb) {
      std::fprintf(stderr, "xtransd: mt: %s\n", error.c_str());
      return 3;
    }
    engines.mt = std::move(nllb);
  }
#else
  if (options.mt_dir != nullptr) {
    std::fputs("xtransd: built without ONNX Runtime/NLLB (no --mt-dir)\n", stderr);
    return 3;
  }
#endif

#if XTRANS_WITH_PIPER
  if (options.voice_dir != nullptr) {
    std::string error;
    xtrans::PiperConfig config;
    config.voice_dir = options.voice_dir;
    config.espeak_data = options.espeak_data;
    auto piper = xtrans::PiperTts::open(config, error);
    if (!piper) {
      std::fprintf(stderr, "xtransd: tts: %s\n", error.c_str());
      return 3;
    }
    engines.tts = std::move(piper);
  }
#else
  if (options.voice_dir != nullptr) {
    std::fputs("xtransd: built without Piper (no --voice-dir)\n", stderr);
    return 3;
  }
#endif

  return 0;
}

// --- pipeline -------------------------------------------------------------

struct TurnResult {
  bool ok = false;
  std::size_t transcript = 0;
  std::size_t translation = 0;
  std::size_t speech = 0;
};

// One utterance through the chain. Buffers are caller-owned and reused.
// Expects the pipeline to be in `transcribing` (Device already released the
// turn, selftest does press+release first).
TurnResult run_turn(Engines& engines, xtrans::Pipeline& pipeline, std::span<const std::int16_t> pcm,
                    xtrans::Lang from, xtrans::Lang to, std::span<char> transcript,
                    std::span<char> translation, std::span<std::int16_t> speech) {
  TurnResult result;
  if (!engines.stt) {
    pipeline.abort();
    return result;
  }
  {
    std::size_t written = 0;
    const xtrans::SttStatus status = engines.stt->transcribe(pcm, from, transcript, written);
    std::printf("stt status=%d chars=%zu\n", static_cast<int>(status), written);
    std::fflush(stdout);
    pipeline.complete(xtrans::Stage::stt, status == xtrans::SttStatus::ok);
    if (status != xtrans::SttStatus::ok) return result;
    result.transcript = written;
  }

  if (!engines.mt) {
    pipeline.abort();
    return result;
  }
  {
    std::size_t written = 0;
    const xtrans::MtStatus status = engines.mt->translate(
        std::string_view(transcript.data(), result.transcript), from, to, translation, written);
    std::printf("mt status=%d chars=%zu\n", static_cast<int>(status), written);
    std::fflush(stdout);
    pipeline.complete(xtrans::Stage::mt, status == xtrans::MtStatus::ok);
    if (status != xtrans::MtStatus::ok) return result;
    result.translation = written;
  }

  if (!engines.tts) {
    pipeline.abort();
    return result;
  }
  {
    std::size_t written = 0;
    const xtrans::TtsStatus status = engines.tts->speak(
        std::string_view(translation.data(), result.translation), to, speech, written);
    std::printf("tts status=%d samples=%zu\n", static_cast<int>(status), written);
    std::fflush(stdout);
    pipeline.complete(xtrans::Stage::tts, status == xtrans::TtsStatus::ok);
    if (status != xtrans::TtsStatus::ok) return result;
    result.speech = written;
  }

  result.ok = true;
  return result;
}

#if XTRANS_WITH_ALSA && XTRANS_WITH_GPIOD
// Plays in short chunks and lets the device poll buttons in between, so a new
// press stops the current playback instead of talking over it.
bool play(xtrans::AudioOut& out, std::span<const std::int16_t> pcm, xtrans::Device& device) {
  constexpr std::size_t kChunk = xtrans::kSampleRate / 10;  // 100 ms
  std::size_t offset = 0;
  while (offset < pcm.size()) {
    (void)device.tick();
    if (device.recording()) return false;  // barge-in: the new turn takes over
    const std::size_t n = std::min(kChunk, pcm.size() - offset);
    if (!out.write(pcm.subspan(offset, n))) return false;
    offset += n;
  }
  out.drain();
  return true;
}
#endif

// --- selftest -------------------------------------------------------------

int run_selftest(const Options& options) {
  Engines engines;
  const int rc = open_engines(options, engines);
  if (rc != 0) return rc;
  if (!engines.stt || !engines.mt || !engines.tts) {
    std::fputs("xtransd: selftest needs --model, --mt-dir and --voice-dir\n", stderr);
    return 3;
  }

  std::vector<std::int16_t> source(kSpeechMax);
  std::vector<char> transcript(kTranscriptMax);
  std::vector<char> translation(kTranscriptMax);
  std::vector<std::int16_t> spoken(kSpeechMax);

  std::size_t source_samples = 0;
  const xtrans::TtsStatus tts_in =
      engines.tts->speak("Bonjour, comment allez-vous ?", xtrans::Lang::fr, source,
                         source_samples);
  if (tts_in != xtrans::TtsStatus::ok || source_samples == 0) {
    std::fprintf(stderr, "xtransd: selftest: input synthesis failed (%d)\n",
                 static_cast<int>(tts_in));
    return 1;
  }
  source.resize(source_samples);

  xtrans::Pipeline pipeline;
  pipeline.press();
  pipeline.release();
  const TurnResult result =
      run_turn(engines, pipeline, source, xtrans::Lang::fr, xtrans::Lang::en, transcript,
               translation, spoken);
  if (!result.ok) {
    std::fprintf(stderr, "xtransd: selftest: pipeline failed\n");
    return 1;
  }

  char in_path[512];
  char out_path[512];
  std::snprintf(in_path, sizeof in_path, "%s/selftest-in.wav", options.out_dir);
  std::snprintf(out_path, sizeof out_path, "%s/selftest-out.wav", options.out_dir);
  const std::size_t in_bytes = xtrans::write_wav(in_path, source, xtrans::kSampleRate);
  const std::size_t out_bytes =
      xtrans::write_wav(out_path, std::span<const std::int16_t>(spoken.data(), result.speech),
                        xtrans::kSampleRate);
  std::printf("selftest ok: in=%zu samples, out=%zu samples, wav=%zu/%zu bytes\n", source.size(),
              result.speech, in_bytes, out_bytes);
  return (in_bytes != 0 && out_bytes != 0) ? 0 : 1;
}

// --- device ---------------------------------------------------------------

int run_device(const Options& options) {
#if XTRANS_WITH_ALSA && XTRANS_WITH_GPIOD
  std::string error;
  auto buttons = xtrans::GpiodButtons::open("/dev/gpiochip0", xtrans::GpioLines{}, error);
  if (!buttons) {
    std::fprintf(stderr, "xtransd: gpio: %s\n", error.c_str());
    return 3;
  }
  auto audio_in = xtrans::AlsaIn::open("default", error);
  if (!audio_in) {
    std::fprintf(stderr, "xtransd: audio in: %s\n", error.c_str());
    return 3;
  }
  auto audio_out = xtrans::AlsaOut::open("default", error);
  if (!audio_out) {
    std::fprintf(stderr, "xtransd: audio out: %s\n", error.c_str());
    return 3;
  }

  Engines engines;
  if (const int rc = open_engines(options, engines); rc != 0) return rc;

  std::vector<std::int16_t> capture(xtrans::kSampleRate * 15);
  xtrans::Device device(*buttons, *audio_in, capture);
  std::vector<char> transcript(kTranscriptMax);
  std::vector<char> translation(kTranscriptMax);
  std::vector<std::int16_t> speech(kSpeechMax);

  // Ready prompt: tell the user the device is alive, in French.
  if (engines.tts) {
    std::size_t prompt = 0;
    if (engines.tts->speak("Prêt.", xtrans::Lang::fr, speech, prompt) ==
            xtrans::TtsStatus::ok &&
        prompt != 0) {
      (void)play(*audio_out, std::span<const std::int16_t>(speech.data(), prompt), device);
    }
  }

  xtrans::Watchdog watchdog{options.stage_timeout_ms};
  std::printf("ready\n");
  std::fflush(stdout);

  const timespec tick_delay{0, 5 * 1000 * 1000};
  while (g_stop == 0) {
    if (device.tick()) {
      std::span<const std::int16_t> turn = device.last_turn();
      if (turn.empty()) {
        std::printf("discarded reason=%d\n", static_cast<int>(device.last_result()));
        std::fflush(stdout);
      } else {
        const unsigned ms =
            static_cast<unsigned>((turn.size() * 1000U) / xtrans::kSampleRate);
        std::printf("turn samples=%zu ms=%u\n", turn.size(), ms);
        std::fflush(stdout);

        const xtrans::Pair& pair = device.controller().selector().pair();
        xtrans::Pipeline& pipeline = device.controller().pipeline();
        watchdog.arm(now_ms());
        const TurnResult result = run_turn(engines, pipeline, turn, pair.from(), pair.to(),
                                           transcript, translation, speech);
        watchdog.disarm();

        if (result.ok) {
          (void)play(*audio_out,
                     std::span<const std::int16_t>(speech.data(), result.speech), device);
        }
      }
    }

    if (watchdog.expired(now_ms())) {
      if (device.controller().pipeline().busy()) {
        std::printf("timeout\n");
        std::fflush(stdout);
        device.controller().pipeline().abort();
      }
      watchdog.disarm();
    }

    if (!device.recording()) watchdog.disarm();
    nanosleep(&tick_delay, nullptr);
  }
  return 0;
#else
  (void)options;
  std::fputs("xtransd: built without ALSA/GPIO adapters (use --stdin)\n", stderr);
  return 3;
#endif
}

}  // namespace

int main(int argc, char** argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (std::strcmp(arg, "--help") == 0) {
      usage(argv[0]);
      return 0;
    }
    if (std::strcmp(arg, "--stdin") == 0) continue;
    if (std::strcmp(arg, "--device") == 0) {
      options.device = true;
      continue;
    }
    if (std::strcmp(arg, "--selftest") == 0) {
      options.selftest = true;
      continue;
    }
    if (std::strcmp(arg, "--model") == 0 && i + 1 < argc) {
      options.model = argv[++i];
      continue;
    }
    if (std::strcmp(arg, "--mt-dir") == 0 && i + 1 < argc) {
      options.mt_dir = argv[++i];
      continue;
    }
    if (std::strcmp(arg, "--voice-dir") == 0 && i + 1 < argc) {
      options.voice_dir = argv[++i];
      continue;
    }
    if (std::strcmp(arg, "--espeak-data") == 0 && i + 1 < argc) {
      options.espeak_data = argv[++i];
      continue;
    }
    if (std::strcmp(arg, "--out") == 0 && i + 1 < argc) {
      options.out_dir = argv[++i];
      continue;
    }
    if (std::strcmp(arg, "--stage-timeout") == 0 && i + 1 < argc) {
      options.stage_timeout_ms = static_cast<std::uint32_t>(std::atoi(argv[++i]));
      continue;
    }
    std::fprintf(stderr, "xtransd: unknown argument '%s' (try --help)\n", arg);
    return 2;
  }

  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);

  if (options.selftest) return run_selftest(options);
  if (options.device) return run_device(options);
  std::signal(SIGPIPE, SIG_IGN);
  return run_stdin();
}
