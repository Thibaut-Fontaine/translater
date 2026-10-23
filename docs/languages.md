# Language support

Target languages, in order of intended priority:

| Language | Code (NLLB) | Whisper.cpp STT | Piper TTS | Status |
| --- | --- | --- | --- | --- |
| French | `fra_Latn` | yes | yes | planned |
| English | `eng_Latn` | yes | yes | planned |
| Thai | `tha_Thai` | partial | yes | to validate |
| Lao | `lao_Laoo` | partial | limited | to validate |
| Hmong | `hmn_Latn` (Hmong Daw) | no / unvalidated | no / unvalidated | unknown |
| Yunnan minority languages | varies | no | no | unknown |

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
- The language list in the UI (button cycles) should be generated from a validated-support
  matrix, so an unsupported language is never offered silently.
