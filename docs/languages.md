# Language support

Target languages, in order of intended priority. The capability table in
`src/language.cpp` is the source of truth; this document explains it.

| Language | ISO 639-3 | NLLB code | Whisper.cpp STT | Piper TTS | Ready |
| --- | --- | --- | --- | --- | --- |
| French | `fra` | `fra_Latn` | yes (`fr`) | `fr_FR-siwis-medium` | yes |
| English | `eng` | `eng_Latn` | yes (`en`) | `en_US-lessac-medium` | yes |
| Thai | `tha` | `tha_Thai` | yes (`th`) | none validated | no |
| Lao | `lao` | `lao_Laoo` | yes (`lo`) | none validated | no |
| Hmong | `hmn` | `hmn_Latn` | no | none | no |
| Yunnan minority languages | varies | varies | no | no | no |

A language is **ready** only when all three capabilities are present
(`kStt | kMt | kTts`). Ready languages, and only those, appear in the button
cycle (`selectable()`).


## What has to be validated

Each language needs three independent pieces, and all three must exist for the pair to work:

1. **Speech recognition** — Whisper must be able to transcribe the language well enough on
   clear, close-mic speech. Whisper's training data is thin for Lao and absent for most
   Yunnan minority languages.
2. **Translation** — NLLB-200 Distilled 600M must include the language code and produce
   usable output for the pair. Low-resource pairs degrade quickly; some pairs are effectively
   unsupported in the distilled model.
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
