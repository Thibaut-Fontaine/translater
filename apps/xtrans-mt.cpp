// xtrans-mt — translate one sentence with the NLLB adapter.
//
// Development and benchmarking tool; the daemon never prints translations.
//   xtrans-mt --mt-dir DIR --from fr --to en "Bonjour tout le monde"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <string_view>

#include "xtrans/language.hpp"

#if XTRANS_WITH_NLLB
#include "xtrans/adapters/nllb.hpp"
#include "xtrans/mt.hpp"
#endif

namespace {

void usage(const char* argv0) {
  std::fprintf(stderr,
               "usage: %s --mt-dir DIR --from LANG --to LANG TEXT...\n"
               "  LANG: fr, en, th, lo\n",
               argv0);
}

#if XTRANS_WITH_NLLB
xtrans::Lang parse_lang(std::string_view code, bool& ok) {
  ok = true;
  if (code == "fr") return xtrans::Lang::fr;
  if (code == "en") return xtrans::Lang::en;
  if (code == "th") return xtrans::Lang::th;
  if (code == "lo") return xtrans::Lang::lo;
  ok = false;
  return xtrans::Lang::fr;
}
#endif

}  // namespace

int main(int argc, char** argv) {
#if XTRANS_WITH_NLLB
  const char* dir = nullptr;
  const char* from_code = nullptr;
  const char* to_code = nullptr;
  std::string text;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--mt-dir") == 0 && i + 1 < argc) {
      dir = argv[++i];
    } else if (std::strcmp(argv[i], "--from") == 0 && i + 1 < argc) {
      from_code = argv[++i];
    } else if (std::strcmp(argv[i], "--to") == 0 && i + 1 < argc) {
      to_code = argv[++i];
    } else if (std::strcmp(argv[i], "--help") == 0) {
      usage(argv[0]);
      return 0;
    } else {
      if (!text.empty()) text += ' ';
      text += argv[i];
    }
  }
  if (dir == nullptr || from_code == nullptr || to_code == nullptr || text.empty()) {
    usage(argv[0]);
    return 2;
  }

  bool ok = false;
  const xtrans::Lang from = parse_lang(from_code, ok);
  if (!ok) {
    std::fprintf(stderr, "xtrans-mt: unknown source language '%s'\n", from_code);
    return 2;
  }
  const xtrans::Lang to = parse_lang(to_code, ok);
  if (!ok) {
    std::fprintf(stderr, "xtrans-mt: unknown target language '%s'\n", to_code);
    return 2;
  }

  std::string encoder = std::string(dir) + "/encoder_model_int8.onnx";
  std::string decoder = std::string(dir) + "/decoder_model_quantized.onnx";
  std::string decoder_with_past = std::string(dir) + "/decoder_with_past_model_quantized.onnx";
  std::string tokenizer = std::string(dir) + "/sentencepiece.bpe.model";

  xtrans::NllbConfig config;
  config.encoder = encoder.c_str();
  config.decoder = decoder.c_str();
  config.decoder_with_past = decoder_with_past.c_str();
  config.tokenizer = tokenizer.c_str();

  std::string error;
  auto mt = xtrans::NllbMt::open(config, error);
  if (!mt) {
    std::fprintf(stderr, "xtrans-mt: %s\n", error.c_str());
    return 3;
  }

  char out[1024];
  std::size_t written = 0;
  const timespec start{};
  timespec begin{};
  clock_gettime(CLOCK_MONOTONIC, &begin);
  const xtrans::MtStatus status = mt->translate(text, from, to, out, written);
  timespec end{};
  clock_gettime(CLOCK_MONOTONIC, &end);
  const long ms = (end.tv_sec - begin.tv_sec) * 1000L +
                  (end.tv_nsec - begin.tv_nsec) / 1000000L;

  if (status != xtrans::MtStatus::ok) {
    std::fprintf(stderr, "xtrans-mt: status=%d\n", static_cast<int>(status));
    return 1;
  }
  std::printf("%.*s\n", static_cast<int>(written), out);
  std::fprintf(stderr, "%ld ms, %zu decoder steps\n", ms, mt->last_tokens());
  (void)start;
  return 0;
#else
  (void)argc;
  usage(argv[0]);
  std::fputs("xtrans-mt: built without ONNX Runtime/NLLB\n", stderr);
  return 3;
#endif
}
