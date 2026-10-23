// xtrans-tts — synthesize one sentence with Piper and write a WAV.
//
// Development tool: verifies voice output on machines without a speaker and
// measures first-chunk latency.
//   xtrans-tts --voice-dir DIR --espeak-data DIR --lang fr --out out.wav "Bonjour"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <string_view>
#include <vector>

#include "xtrans/audio.hpp"
#include "xtrans/language.hpp"
#include "xtrans/wav.hpp"

#if XTRANS_WITH_PIPER
#include "xtrans/adapters/piper.hpp"
#include "xtrans/tts.hpp"
#endif

namespace {

void usage(const char* argv0) {
  std::fprintf(stderr,
               "usage: %s --voice-dir DIR --espeak-data DIR --lang LANG [--out FILE] TEXT...\n"
               "  LANG: fr, en, th\n",
               argv0);
}

#if XTRANS_WITH_PIPER
xtrans::Lang parse_lang(std::string_view code, bool& ok) {
  ok = true;
  if (code == "fr") return xtrans::Lang::fr;
  if (code == "en") return xtrans::Lang::en;
  if (code == "th") return xtrans::Lang::th;
  ok = false;
  return xtrans::Lang::fr;
}
#endif

}  // namespace

int main(int argc, char** argv) {
#if XTRANS_WITH_PIPER
  const char* voice_dir = nullptr;
  const char* espeak_data = nullptr;
  const char* lang_code = nullptr;
  const char* out_path = "/tmp/opencode/xtrans-tts.wav";
  int repeat = 1;
  std::string text;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--voice-dir") == 0 && i + 1 < argc) {
      voice_dir = argv[++i];
    } else if (std::strcmp(argv[i], "--espeak-data") == 0 && i + 1 < argc) {
      espeak_data = argv[++i];
    } else if (std::strcmp(argv[i], "--lang") == 0 && i + 1 < argc) {
      lang_code = argv[++i];
    } else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
      out_path = argv[++i];
    } else if (std::strcmp(argv[i], "--repeat") == 0 && i + 1 < argc) {
      repeat = std::atoi(argv[++i]);
      if (repeat < 1) repeat = 1;
    } else if (std::strcmp(argv[i], "--help") == 0) {
      usage(argv[0]);
      return 0;
    } else {
      if (!text.empty()) text += ' ';
      text += argv[i];
    }
  }
  if (voice_dir == nullptr || lang_code == nullptr || text.empty()) {
    usage(argv[0]);
    return 2;
  }

  bool ok = false;
  const xtrans::Lang lang = parse_lang(lang_code, ok);
  if (!ok) {
    std::fprintf(stderr, "xtrans-tts: unknown language '%s'\n", lang_code);
    return 2;
  }

  xtrans::PiperConfig config;
  config.voice_dir = voice_dir;
  config.espeak_data = espeak_data;

  std::string error;
  auto tts = xtrans::PiperTts::open(config, error);
  if (!tts) {
    std::fprintf(stderr, "xtrans-tts: %s\n", error.c_str());
    return 3;
  }

  std::vector<std::int16_t> pcm(xtrans::kSampleRate * 30);
  std::size_t written = 0;
  xtrans::TtsStatus status = xtrans::TtsStatus::failed;
  long ms = 0;
  for (int run = 0; run < repeat; ++run) {
    timespec begin{};
    clock_gettime(CLOCK_MONOTONIC, &begin);
    status = tts->speak(text, lang, pcm, written);
    timespec end{};
    clock_gettime(CLOCK_MONOTONIC, &end);
    ms = (end.tv_sec - begin.tv_sec) * 1000L + (end.tv_nsec - begin.tv_nsec) / 1000000L;
    if (status != xtrans::TtsStatus::ok) break;
    std::fprintf(stderr, "run %d: %ld ms\n", run + 1, ms);
  }

  if (status != xtrans::TtsStatus::ok) {
    std::fprintf(stderr, "xtrans-tts: status=%d\n", static_cast<int>(status));
    return 1;
  }

  pcm.resize(written);
  const std::size_t bytes = xtrans::write_wav(out_path, pcm, xtrans::kSampleRate);
  if (bytes == 0) {
    std::fprintf(stderr, "xtrans-tts: cannot write %s\n", out_path);
    return 1;
  }
  std::printf("%s: %zu samples (%.2f s), %zu bytes\n", out_path, written,
              static_cast<double>(written) / xtrans::kSampleRate, bytes);
  std::fprintf(stderr, "%ld ms synthesis\n", ms);
  return 0;
#else
  (void)argc;
  usage(argv[0]);
  std::fputs("xtrans-tts: built without Piper\n", stderr);
  return 3;
#endif
}
