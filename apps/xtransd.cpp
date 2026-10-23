// xtransd — headless controller for the portable translator.
//
// Two modes:
//   --stdin   control lines on stdin (default; no hardware needed)
//   --device  real buttons and microphone via libgpiod and ALSA
//
// The device mode currently records a turn and reports it, then aborts the
// pipeline: the STT/MT/TTS adapters land in the next milestones.
#include <csignal>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

#include "xtrans/control.hpp"
#include "xtrans/device.hpp"

#if XTRANS_WITH_ALSA && XTRANS_WITH_GPIOD
#include "xtrans/adapters/alsa.hpp"
#include "xtrans/adapters/gpiod.hpp"
#endif

namespace {

volatile std::sig_atomic_t g_stop = 0;

extern "C" void handle_signal(int) { g_stop = 1; }

void usage(const char* argv0) {
  std::printf(
      "usage: %s [--stdin | --device | --help]\n"
      "\n"
      "  --stdin   read control lines (press|release|abort|lang-a|lang-b|swap|"
      "quit)\n"
      "            on stdin and print the controller status after each one\n"
      "  --device  use the real buttons and microphone; each finished turn is\n"
      "            reported with its sample count and duration\n"
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

int run_device() {
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
      device.controller().pipeline().abort();  // until STT/MT/TTS exist
    }
    nanosleep(&tick_delay, nullptr);
  }
  return 0;
#else
  std::fputs("xtransd: built without ALSA/GPIO adapters (use --stdin)\n", stderr);
  return 3;
#endif
}

}  // namespace

int main(int argc, char** argv) {
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--help") == 0) {
      usage(argv[0]);
      return 0;
    }
    if (std::strcmp(argv[i], "--stdin") == 0) continue;
    if (std::strcmp(argv[i], "--device") == 0) {
      std::signal(SIGINT, handle_signal);
      std::signal(SIGTERM, handle_signal);
      return run_device();
    }
    std::fprintf(stderr, "xtransd: unknown argument '%s' (try --help)\n", argv[i]);
    return 2;
  }

  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
  std::signal(SIGPIPE, SIG_IGN);
  return run_stdin();
}
