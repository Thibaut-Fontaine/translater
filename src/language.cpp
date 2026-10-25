#include "xtrans/language.hpp"

#include <array>

namespace xtrans {
namespace {

// Capabilities: STT from whisper.cpp coverage, MT from the NLLB token id, TTS
// only where a validated Piper voice is shipped. The table and the per-mode
// lists are generated; see scripts/gen_language_catalog.py.
#include "lang_table_generated.inc"

constexpr std::array<Lang, 2> kSelectable{Lang::fr, Lang::en};

std::size_t index_of(Lang lang) noexcept {
  const auto i = static_cast<std::size_t>(lang);
  return i < kLangCount ? i : 0;
}

}  // namespace

const LangInfo& lang_info(Lang id) noexcept { return kTable[index_of(id)]; }

std::uint8_t capabilities(Lang id) noexcept { return lang_info(id).caps; }

bool supports(Lang id, Mode mode) noexcept {
  const std::uint8_t caps = capabilities(id);
  const bool stt = (caps & kStt) != 0;
  const bool mt = (caps & kMt) != 0;
  const bool tts = (caps & kTts) != 0;
  switch (mode) {
    case Mode::speech_to_speech:
      return stt && mt && tts;
    case Mode::speech_to_text:
      return stt && mt;
    case Mode::text_to_text:
      return mt;
    case Mode::text_to_speech:
      return mt && tts;
  }
  return false;
}

bool ready(Lang id) noexcept { return lang_info(id).caps == kReady; }

bool translatable(Lang from, Lang to) noexcept {
  return from != to && ready(from) && ready(to);
}

bool can_translate(Lang from, Lang to) noexcept {
  return from != to && (capabilities(from) & kMt) != 0 && (capabilities(to) & kMt) != 0;
}

std::span<const Lang> selectable() noexcept { return kSelectable; }

std::span<const Lang> selectable(Mode mode) noexcept {
  switch (mode) {
    case Mode::speech_to_speech:
      return kSpeechToSpeech;
    case Mode::speech_to_text:
      return kSpeechToText;
    case Mode::text_to_text:
      return kTextToText;
    case Mode::text_to_speech:
      return kTextToSpeech;
  }
  return {};
}

void Selector::cycle(Button button) noexcept {
  const auto langs = cycle_;
  if (button == Button::ptt || langs.size() < 2) return;

  Lang& target = button == Button::lang_a ? pair_.a : pair_.b;
  const Lang other = button == Button::lang_a ? pair_.b : pair_.a;

  std::size_t i = 0;
  while (i < langs.size() && langs[i] != target) ++i;
  if (i == langs.size()) i = langs.size() - 1;

  for (std::size_t step = 1; step <= langs.size(); ++step) {
    const Lang candidate = langs[(i + step) % langs.size()];
    if (candidate != other && candidate != target) {
      target = candidate;
      return;
    }
  }

  // Only the other side's language is available: swap instead of collapsing
  // both directions onto the same language.
  const Lang tmp = pair_.a;
  pair_.a = pair_.b;
  pair_.b = tmp;
}

}  // namespace xtrans
