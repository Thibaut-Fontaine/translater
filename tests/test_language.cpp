#include "harness.hpp"
#include "xtrans/language.hpp"

#include <array>

using namespace xtrans;

namespace {

int count_selectable() {
  int n = 0;
  for (Lang l : selectable()) {
    if (ready(l)) ++n;
  }
  return n;
}

}  // namespace

TEST_CASE(test_language_every_entry_has_stable_codes) {
  for (std::size_t i = 0; i < kLangCount; ++i) {
    const auto& info = lang_info(static_cast<Lang>(i));
    CHECK_EQ(static_cast<std::size_t>(info.id), i);
    CHECK(!info.iso639_3.empty());
    CHECK_EQ((info.caps & kMt) != 0, !info.nllb.empty());
  }
}

TEST_CASE(test_language_only_french_and_english_are_ready) {
  CHECK(ready(Lang::fr));
  CHECK(ready(Lang::en));
  CHECK(!ready(Lang::th));
  CHECK(!ready(Lang::lo));
  CHECK(!ready(Lang::hmn));
}

TEST_CASE(test_language_whisper_does_not_cover_hmong) {
  CHECK(lang_info(Lang::hmn).whisper.empty());
  CHECK(!lang_info(Lang::lo).whisper.empty());
  CHECK(!lang_info(Lang::th).whisper.empty());
}

TEST_CASE(test_language_ready_requires_all_three_capabilities) {
  CHECK_EQ(lang_info(Lang::fr).caps, kReady);
  CHECK_EQ(lang_info(Lang::th).caps, kStt | kMt);
  CHECK_EQ(lang_info(Lang::hmn).caps, 0);
}

TEST_CASE(test_language_selectable_is_exactly_the_ready_set) {
  CHECK_EQ(selectable().size(), 2U);
  CHECK_EQ(count_selectable(), static_cast<int>(selectable().size()));
  CHECK_EQ(selectable()[0], Lang::fr);
  CHECK_EQ(selectable()[1], Lang::en);
}

TEST_CASE(test_language_selection_swaps_when_only_two_languages_exist) {
  Selector sel{Pair{Lang::fr, Lang::en}};
  CHECK_EQ(sel.pair().a, Lang::fr);
  CHECK_EQ(sel.pair().b, Lang::en);

  sel.cycle(Button::lang_a);
  CHECK_EQ(sel.pair().a, Lang::en);
  CHECK_EQ(sel.pair().b, Lang::fr);

  sel.cycle(Button::lang_a);
  CHECK_EQ(sel.pair().a, Lang::fr);
  CHECK_EQ(sel.pair().b, Lang::en);
}

TEST_CASE(test_language_selection_skips_the_language_used_by_the_other_button) {
  constexpr std::array<Lang, 3> langs{Lang::fr, Lang::en, Lang::th};
  Selector sel{Pair{Lang::fr, Lang::en}, langs};
  sel.cycle(Button::lang_a);
  CHECK_EQ(sel.pair().a, Lang::th);
  CHECK_EQ(sel.pair().b, Lang::en);

  sel.cycle(Button::lang_a);
  CHECK_EQ(sel.pair().a, Lang::fr);
}

TEST_CASE(test_language_selection_ignores_push_to_talk) {
  Selector sel{Pair{Lang::fr, Lang::en}};
  sel.cycle(Button::ptt);
  CHECK_EQ(sel.pair().a, Lang::fr);
  CHECK_EQ(sel.pair().b, Lang::en);
}

TEST_CASE(test_language_pair_direction_swaps) {
  Pair p{Lang::fr, Lang::en};
  CHECK_EQ(p.from(), Lang::fr);
  CHECK_EQ(p.to(), Lang::en);
  p.swap();
  CHECK_EQ(p.from(), Lang::en);
  CHECK_EQ(p.to(), Lang::fr);
  p.swap();
  CHECK_EQ(p.from(), Lang::fr);
}

TEST_CASE(test_language_only_ready_pairs_are_translatable) {
  CHECK(translatable(Lang::fr, Lang::en));
  CHECK(translatable(Lang::en, Lang::fr));
  CHECK(!translatable(Lang::fr, Lang::fr));
  CHECK(!translatable(Lang::fr, Lang::th));
  CHECK(!translatable(Lang::hmn, Lang::en));
  CHECK(!translatable(Lang::lo, Lang::hmn));
}

TEST_CASE(test_language_nllb_ids_match_the_published_tokenizer) {
  CHECK_EQ(lang_info(Lang::en).nllb_id, 256047);
  CHECK_EQ(lang_info(Lang::fr).nllb_id, 256057);
  CHECK_EQ(lang_info(Lang::lo).nllb_id, 256100);
  CHECK_EQ(lang_info(Lang::th).nllb_id, 256175);
  CHECK_EQ(lang_info(Lang::hmn).nllb_id, 0);
}

TEST_CASE(test_language_mt_capability_requires_an_nllb_id) {
  for (std::size_t i = 0; i < kLangCount; ++i) {
    const auto& info = lang_info(static_cast<Lang>(i));
    CHECK_EQ((info.caps & kMt) != 0, info.nllb_id != 0);
  }
}

// --- per-capability model (mobile app) -------------------------------------
// The Pi gates a language on STT + MT + TTS because it can only speak. A phone
// shows text, so translation alone is enough and the catalog is much wider.

TEST_CASE(test_language_capabilities_are_reported_per_component) {
  CHECK_EQ(capabilities(Lang::fr), kReady);
  CHECK_EQ(capabilities(Lang::th), kStt | kMt);
  CHECK_EQ(capabilities(Lang::hmn), 0);
  CHECK_EQ(capabilities(Lang::vie_Latn), kStt | kMt);
  CHECK_EQ(capabilities(Lang::khm_Khmr), kStt | kMt);  // whisper has km
}

TEST_CASE(test_language_can_translate_needs_mt_on_both_sides_only) {
  CHECK(can_translate(Lang::fr, Lang::en));
  CHECK(can_translate(Lang::en, Lang::fr));
  // A voice is no longer required: Lao/Thai and every text-only language.
  CHECK(can_translate(Lang::th, Lang::fr));
  CHECK(can_translate(Lang::fr, Lang::lo));
  CHECK(can_translate(Lang::vie_Latn, Lang::zho_Hans));
  // Same language, or a language with no NLLB token.
  CHECK(!can_translate(Lang::fr, Lang::fr));
  CHECK(!can_translate(Lang::hmn, Lang::fr));
  CHECK(!can_translate(Lang::fr, Lang::hmn));
  CHECK(!can_translate(Lang::vie_Latn, Lang::vie_Latn));
}

TEST_CASE(test_language_supports_rejects_the_missing_component) {
  CHECK(supports(Lang::fr, Mode::speech_to_speech));
  CHECK(supports(Lang::th, Mode::speech_to_text));
  CHECK(!supports(Lang::th, Mode::speech_to_speech));  // no validated voice
  CHECK(supports(Lang::th, Mode::text_to_text));
  CHECK(supports(Lang::vie_Latn, Mode::speech_to_text));
  CHECK(supports(Lang::vie_Latn, Mode::text_to_text));
  CHECK(!supports(Lang::vie_Latn, Mode::speech_to_speech));
  CHECK(!supports(Lang::hmn, Mode::text_to_text));
  CHECK(!supports(Lang::hmn, Mode::speech_to_text));
}

TEST_CASE(test_language_selectable_text_to_text_covers_the_nllb_catalog) {
  const auto text = selectable(Mode::text_to_text);
  CHECK(text.size() >= 190U);
  auto contains = [&](Lang lang) {
    for (Lang l : text) {
      if (l == lang) return true;
    }
    return false;
  };
  CHECK(contains(Lang::fr));
  CHECK(contains(Lang::th));
  CHECK(contains(Lang::vie_Latn));
  CHECK(!contains(Lang::hmn));
}

TEST_CASE(test_language_speech_modes_are_stricter_than_text_modes) {
  const auto speech = selectable(Mode::speech_to_text);
  const auto text = selectable(Mode::text_to_text);
  CHECK(speech.size() < text.size());
  for (Lang l : speech) {
    CHECK(supports(l, Mode::speech_to_text));
    CHECK(supports(l, Mode::text_to_text));
  }
  CHECK(speech.size() >= 80U);
}

TEST_CASE(test_language_selectable_mode_and_supports_agree) {
  constexpr Mode modes[] = {Mode::speech_to_speech, Mode::speech_to_text,
                            Mode::text_to_text, Mode::text_to_speech};
  for (Mode mode : modes) {
    std::size_t expected = 0;
    for (std::size_t i = 0; i < kLangCount; ++i) {
      if (supports(static_cast<Lang>(i), mode)) ++expected;
    }
    CHECK_EQ(selectable(mode).size(), expected);
    for (Lang l : selectable(mode)) CHECK(supports(l, mode));
  }
}

TEST_CASE(test_language_catalog_stays_stable_for_the_phone) {
  // The app ships this list; growing it is fine, shrinking it silently is not.
  CHECK(kLangCount >= 200U);
  CHECK_EQ(lang_info(Lang::vie_Latn).nllb_id, 256193);
  CHECK_EQ(lang_info(Lang::zho_Hans).whisper, "zh");
}
