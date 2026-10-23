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
