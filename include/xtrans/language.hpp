#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace xtrans {

// Console order of the language cycle. Keep entries appended: the numeric value
// is stored in the config file and must stay stable.
enum class Lang : std::uint8_t { fr = 0, en, th, lo, hmn, count };

inline constexpr std::size_t kLangCount = static_cast<std::size_t>(Lang::count);

// A language is only usable when the recognizer, the translator and a voice all
// exist. Anything less is hidden from the button cycle.
inline constexpr std::uint8_t kStt = 1U << 0;
inline constexpr std::uint8_t kMt = 1U << 1;
inline constexpr std::uint8_t kTts = 1U << 2;
inline constexpr std::uint8_t kReady = kStt | kMt | kTts;

struct LangInfo {
  Lang id;
  std::string_view iso639_3;   // ISO 639-3, used in filenames and logs
  std::string_view nllb;       // NLLB-200 code, e.g. "fra_Latn"
  std::string_view whisper;    // whisper.cpp code; empty = not supported
  std::string_view piper;      // Piper voice id; empty = no voice validated
  std::uint8_t caps;           // bitmask of kStt | kMt | kTts
};

const LangInfo& lang_info(Lang id) noexcept;

// True when the language has a validated STT, MT and TTS path.
bool ready(Lang id) noexcept;

// Languages the language buttons may select, in cycle order.
std::span<const Lang> selectable() noexcept;

struct Pair {
  Lang a;
  Lang b;
  bool swapped = false;

  Lang from() const noexcept { return swapped ? b : a; }
  Lang to() const noexcept { return swapped ? a : b; }
  void swap() noexcept { swapped = !swapped; }
};

enum class Button : std::uint8_t { lang_a, lang_b, ptt };

class Selector {
 public:
  // `cycle` must be a pre-validated list (default: the ready languages). Tests
  // inject a wider list to exercise skipping.
  explicit Selector(Pair start = {Lang::fr, Lang::en},
                    std::span<const Lang> cycle = selectable()) noexcept
      : pair_(start), cycle_(cycle) {}

  const Pair& pair() const noexcept { return pair_; }

  // Advances the language of `button`, skipping the one in use on the other
  // side so the two directions never collapse onto the same language. When no
  // other language exists, the pair simply swaps.
  void cycle(Button button) noexcept;

  // Flips the speaking direction without touching the selected languages.
  void swap_direction() noexcept { pair_.swap(); }

 private:
  Pair pair_;
  std::span<const Lang> cycle_;
};

}  // namespace xtrans
