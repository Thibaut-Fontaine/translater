# Language support

Target languages, in order of intended priority. The capability table in
`src/language.cpp` is the source of truth; this document explains it.

| Language | ISO 639-3 | NLLB code | Whisper.cpp STT | Piper TTS | Ready |
| --- | --- | --- | --- | --- | --- |
| French | `fra` | `fra_Latn` | yes (`fr`) | `fr_FR-siwis-medium` | yes |
| English | `eng` | `eng_Latn` | yes (`en`) | `en_US-lessac-medium` | yes |
| Thai | `tha` | `tha_Thai` | yes (`th`) | `th_TH-tsync2-medium` exists, unvalidated | no |
| Lao | `lao` | `lao_Laoo` | yes (`lo`) | **none exists** | no |
| Hmong | `hmn` | none (NLLB has no `hmn_Latn`) | no | **none exists** | no |
| Yunnan minority languages | varies | only Tibetan, Jingpho, Shan (see below) | no | none | no |

### What NLLB-200 actually covers in Southeast Asia

The FLORES-200 list (`facebookresearch/flores`, `flores200/README.md`) contains
20 Southeast Asian entries covering 19 languages (Acehnese appears in two
scripts):

| Code | Language | Code | Language |
| --- | --- | --- | --- |
| `tha_Thai` | Thai | `lao_Laoo` | Lao |
| `mya_Mymr` | Burmese | `khm_Khmr` | Khmer |
| `vie_Latn` | Vietnamese | `ind_Latn` | Indonesian |
| `zsm_Latn` | Standard Malay | `tgl_Latn` | Tagalog |
| `ceb_Latn` | Cebuano | `jav_Latn` | Javanese |
| `sun_Latn` | Sundanese | `min_Latn` | Minangkabau |
| `bug_Latn` | Buginese | `ban_Latn` | Balinese |
| `bjn_Latn` | Banjar | `ace_Latn` / `ace_Arab` | Acehnese |
| `shn_Mymr` | Shan | `kac_Latn` | Jingpho (Kachin) |
| `bod_Tibt` | Standard Tibetan | | |

Absent, despite being spoken in the region we target: **Hmong** (`hmn`, `mww`),
**Zhuang**, **Nuosu Yi** (`iii`), **Lisu**, **Lahu**, **Hani**, **Wa**, **Dai /
Tai Lue** (`khb`), **Bai**, **Khmu**, **Mon**, **Northern Thai** (`nod`),
**Isan** (`tts`).

Consequence for the Yunnan goal: only three of the languages we listed have any
NLLB coverage — Tibetan, Jingpho and Shan — and all three are cross-border
languages rather than Yunnan-specific varieties. Everything else in the Yunnan
set needs a different model, and no sub-1B candidate found so far covers them.

### Piper voice coverage (checked 2026-10-23)

`rhasspy/piper-voices` publishes 178 voices across 58 languages. Relevant facts:

- French and English: several voices each (`fr_FR-siwis-medium`,
  `en_US-lessac-medium` chosen).
- Thai: a single voice, `th_TH-tsync2-medium`.
- Lao: **no voice at all**. Hmong: **no voice at all**.
- Also available: Vietnamese, Indonesian, Chinese, Japanese, Korean.

Because the device is bidirectional and screenless, a language without a voice
cannot be a *target*: the translation would have no way to be heard. Lao is
therefore not a usable output language even though NLLB can translate into it,
and the same applies to Hmong and the Yunnan languages. Thai becomes usable in
both directions once its voice is validated on the hardware.


A language is **ready** only when all three capabilities are present
(`kStt | kMt | kTts`). Ready languages, and only those, appear in the button
cycle (`selectable()`).


## What has to be validated

Each language needs three independent pieces, and all three must exist for the pair to work:

1. **Speech recognition** — Whisper must be able to transcribe the language well enough on
   clear, close-mic speech. Whisper's training data is thin for Lao and absent for most
   Yunnan minority languages.
2. **Translation** — NLLB-200 Distilled 600M must include the language code and produce
   usable output for the pair. It has 202 language tokens and **no Hmong**, so Hmong needs a
   different translation model entirely. Low-resource pairs degrade quickly; some pairs are
   effectively unsupported in the distilled model.
3. **Speech synthesis** — a Piper voice must exist for the language. Piper covers many
   languages, but coverage for Southeast Asian minority languages is limited and voice quality
   varies.

## Consequences

- A reliable first version covers **French ↔ English**, then **Thai** and **Lao** if the
  models hold up.
- Hmong and Yunnan minority languages are research items, not deliverables. They should be
  treated as best-effort and may require different STT/TTS models than Whisper/Piper.
- The language list in the UI (button cycles) is generated from the capability
  table, so an unsupported language is never offered silently.
