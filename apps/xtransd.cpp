// xtransd — headless controller for the portable translator.
//
// Two modes:
//   --stdin   control lines on stdin (default; no hardware needed)
//   --device  real buttons and microphone via libgpiod and ALSA
//
// Device mode runs the turn through the adapters that were built in: capture,
// speech recognition and (optionally) translation. Piper playback is the last
// stage still missing, so a turn ends after MT until M5 lands.
#include <array>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "xtrans/control.hpp"
#include "xtrans/device.hpp"

#include "xtrans/stt.hpp"

#if XTRANS_WITH_ALSA && XTRANS_WITH_GPIOD
#include "xtrans/adapters/alsa.hpp"
#include "xtrans/adapters/gpiod.hpp"
#endif

#if XTRANS_WITH_WHISPER
#include "xtrans/adapters/whisper.hpp"
#endif

#if XTRANS_WITH_NLLB
#include "xtrans/adapters/nllb.hpp"
#include "xtrans/mt.hpp"
#endif

namespace {

volatile std::sig_atomic_t g_stop = 0;

extern "C" void handle_signal(int) { g_stop = 1; }

void usage(const char* argv0) {
  std::printf(
      "usage: %s [--stdin | --device [--model FILE] [--mt-dir DIR] | --help]\n"
      "\n"
      "  --stdin   read control lines (press|release|abort|lang-a|lang-b|swap|"
      "quit)\n"
      "            on stdin and print the controller status after each one\n"
      "  --device  use the real buttons and microphone; each finished turn is\n"
      "            reported with its sample count and duration\n"
      "  --model   whisper.cpp model for speech recognition (device mode only)\n"
      "  --mt-dir  directory holding the NLLB ONNX files and the tokenizer\n"
      "\n"
      "Exits on EOF (--stdin), SIGINT or SIGTERM.\n",
      argv0);
}

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
      if (std::feof(stdin) != 0) break;  // no input left: nothing to control
      continue;                          // interrupted by a signal
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

int run_device(const char* model_path, const char* mt_dir) {
#if XTRANS_WITH_ALSA && XTRANS_WITH_GPIOD
  std::string error;

  auto buttons =
      xtrans::GpiodButtons::open("/dev/gpiochip0", xtrans::GpioLines{}, error);
  if (!buttons) {
    std::fprintf(stderr, "xtransd: gpio: %s\n", error.c_str());
    return 3;
  }

  auto audio = xtrans::AlsaIn::open("default", error);
  if (!audio) {
    std::fprintf(stderr, "xtransd: audio: %s\n", error.c_str());
    return 3;
  }

  std::unique_ptr<xtrans::Stt> stt;
#if XTRANS_WITH_WHISPER
  if (model_path != nullptr) {
    auto whisper = xtrans::WhisperStt::open(model_path, error);
    if (!whisper) {
      std::fprintf(stderr, "xtransd: stt: %s\n", error.c_str());
      return 3;
    }
    stt = std::move(whisper);
  }
#else
  if (model_path != nullptr) {
    std::fputs("xtransd: built without whisper.cpp (no --model support)\n", stderr);
    return 3;
  }
#endif

  std::unique_ptr<xtrans::Mt> mt;
#if XTRANS_WITH_NLLB
  if (mt_dir != nullptr) {
    char encoder[512];
    char decoder[512];
    char decoder_with_past[512];
    char tokenizer[512];
    std::snprintf(encoder, sizeof encoder, "%s/encoder_model_int8.onnx", mt_dir);
    std::snprintf(decoder, sizeof decoder, "%s/decoder_model_quantized.onnx", mt_dir);
    std::snprintf(decoder_with_past, sizeof decoder_with_past,
                  "%s/decoder_with_past_model_quantized.onnx", mt_dir);
    std::snprintf(tokenizer, sizeof tokenizer, "%s/sentencepiece.bpe.model", mt_dir);

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
    mt = std::move(nllb);
  }
#else
  if (mt_dir != nullptr) {
    std::fputs("xtransd: built without ONNX Runtime/NLLB (no --mt-dir support)\n", stderr);
    return 3;
  }
#endif

  // 15 s of 16 kHz mono: the turn cap from AGENTS.md §6, allocated once.
  std::vector<std::int16_t> capture(xtrans::kSampleRate * 15);
  xtrans::Device device(*buttons, *audio, capture);

  std::printf("ready\n");
  std::fflush(stdout);

  const timespec tick_delay{0, 5 * 1000 * 1000};  // 5 ms
  while (g_stop == 0) {
    if (device.tick()) {
      const auto turn = device.last_turn();
      if (turn.empty()) {
        std::printf("discarded reason=%d\n", static_cast<int>(device.last_result()));
      } else {
        const unsigned ms =
            static_cast<unsigned>((turn.size() * 1000U) / xtrans::kSampleRate);
        std::printf("turn samples=%zu ms=%u\n", turn.size(), ms);
      }
      std::fflush(stdout);

      if (stt && !turn.empty()) {
        const xtrans::Pair& pair = device.controller().selector().pair();
        std::array<char, 1024> transcript{};
        std::size_t written = 0;
        const xtrans::SttStatus status = stt->transcribe(turn, pair.from(), transcript, written);
        // Transcript content stays private: log the outcome, not the words.
        std::printf("stt status=%d chars=%zu\n", static_cast<int>(status), written);
        std::fflush(stdout);
        device.controller().pipeline().complete(xtrans::Stage::stt,
                                               status == xtrans::SttStatus::ok);

        if (mt && status == xtrans::SttStatus::ok && written != 0) {
          std::array<char, 1024> translated{};
          std::size_t translated_size = 0;
          const xtrans::MtStatus mt_status = mt->translate(
              std::string_view(transcript.data(), written), pair.from(), pair.to(), translated,
              translated_size);
          std::printf("mt status=%d chars=%zu\n", static_cast<int>(mt_status), translated_size);
          std::fflush(stdout);
          device.controller().pipeline().complete(xtrans::Stage::mt,
                                                 mt_status == xtrans::MtStatus::ok);
        }
      }
      device.controller().pipeline().abort();  // until TTS plays the result
    }
    nanosleep(&tick_delay, nullptr);
  }
  return 0;
#else
  (void)model_path;
  (void)mt_dir;
  std::fputs("xtransd: built without ALSA/GPIO adapters (use --stdin)\n", stderr);
  return 3;
#endif
}

}  // namespace

int main(int argc, char** argv) {
  bool device = false;
  const char* model_path = nullptr;
  const char* mt_dir = nullptr;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--help") == 0) {
      usage(argv[0]);
      return 0;
    }
    if (std::strcmp(argv[i], "--stdin") == 0) continue;
    if (std::strcmp(argv[i], "--device") == 0) {
      device = true;
      continue;
    }
    if (std::strcmp(argv[i], "--model") == 0 && i + 1 < argc) {
      model_path = argv[++i];
      continue;
    }
    if (std::strcmp(argv[i], "--mt-dir") == 0 && i + 1 < argc) {
      mt_dir = argv[++i];
      continue;
    }
    std::fprintf(stderr, "xtransd: unknown argument '%s' (try --help)\n", argv[i]);
    return 2;
  }

  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
  if (device) return run_device(model_path, mt_dir);

  std::signal(SIGPIPE, SIG_IGN);
  return run_stdin();
}
