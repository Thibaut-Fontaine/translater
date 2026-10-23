#include "xtrans/language.hpp"

#include <array>

namespace xtrans {
namespace {

// Capabilities mirror docs/languages.md. A language moves to kReady only after
// its recognizer, translator and voice have been measured on the device.
constexpr std::array<LangInfo, kLangCount> kTable{{
    {Lang::fr, "fra", "fra_Latn", "fr", "fr_FR-siwis-medium", kReady},
    {Lang::en, "eng", "eng_Latn", "en", "en_US-lessac-medium", kReady},
    {Lang::th, "tha", "tha_Thai", "th", "", kStt | kMt},
    {Lang::lo, "lao", "lao_Laoo", "lo", "", kStt | kMt},
    {Lang::hmn, "hmn", "hmn_Latn", "", "", kMt},
}};

constexpr std::array<Lang, 2> kSelectable{Lang::fr, Lang::en};

std::size_t index_of(Lang lang) noexcept {
  const auto i = static_cast<std::size_t>(lang);
  return i < kLangCount ? i : 0;
}

}  // namespace

const LangInfo& lang_info(Lang id) noexcept { return kTable[index_of(id)]; }

bool ready(Lang id) noexcept { return lang_info(id).caps == kReady; }

bool translatable(Lang from, Lang to) noexcept {
  return from != to && ready(from) && ready(to);
}

std::span<const Lang> selectable() noexcept { return kSelectable; }

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
