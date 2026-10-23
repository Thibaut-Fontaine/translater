// xtransd — headless controller for the portable translator.
//
// Current stage: the button/pipeline core with a line-based control interface
// on stdin, so the container can be exercised end to end before the GPIO, ALSA
// and model adapters land. Same binary name and same core as the final device;
// only the input adapter changes.
#include <csignal>
#include <cstdio>
#include <cstring>

#include "xtrans/control.hpp"

namespace {

volatile std::sig_atomic_t g_stop = 0;

extern "C" void handle_signal(int) { g_stop = 1; }

void usage(const char* argv0) {
  std::printf(
      "usage: %s [--help]\n"
      "\n"
      "Reads one command per line on stdin: press|release|abort|lang-a|lang-b|"
      "swap|quit\n"
      "(aliases: p r a la lb s q). Prints the controller status after every "
      "accepted command.\n"
      "Exits on EOF, SIGINT or SIGTERM.\n",
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

}  // namespace

int main(int argc, char** argv) {
  if (argc > 1) {
    if (std::strcmp(argv[1], "--help") == 0) {
      usage(argv[0]);
      return 0;
    }
    std::fprintf(stderr, "xtransd: unknown argument '%s' (try --help)\n", argv[1]);
    return 2;
  }

  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
  std::signal(SIGPIPE, SIG_IGN);

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
