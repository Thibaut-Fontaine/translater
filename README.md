# Portable Offline Voice Translator

A compact, fully offline, bidirectional voice translator built around a Raspberry Pi 5.

Speech is captured by a miniature microphone, transcribed with **Whisper.cpp**, translated with
**NLLB-200 Distilled 600M**, and spoken back with **Piper**. Three physical buttons select
language A, language B, and "press to speak"; there is no screen and no internet connection
required after installation.

```
Speech A → Whisper.cpp → Text A → NLLB-200 → Text B → Piper → Speech B
```

The system works in both directions: a button switches between **A → B** and **B → A**.

## Status

Early project preparation. No pipeline code yet — see [docs/](docs/) for the current
design notes.

## Languages

Target languages: French, English, Thai, Lao, Hmong, and selected minority languages of
Yunnan. Speech recognition and speech synthesis require models suitable for each selected
language; support for Hmong and the Yunnan minority languages still has to be validated.
See [docs/languages.md](docs/languages.md).

## Hardware

| Part | Notes |
| --- | --- |
| Raspberry Pi 5 | Pipeline control, audio management, language selection |
| Miniature USB microphone | Audio capture |
| 2–3 W amplifier + miniature speaker | Audio output |
| USB-C battery with Power Delivery | Portable power |
| 64 GB microSD card | Raspberry Pi OS Lite 64-bit, models |
| 3 push buttons | Language A, language B, press to speak |

## Software stack

| Stage | Component |
| --- | --- |
| OS | Raspberry Pi OS Lite 64-bit |
| Storage | 64 GB microSD card |
| Speech-to-text | Whisper.cpp (base model) |
| Translation | NLLB-200 Distilled 600M |
| Text-to-speech | Piper (per-language voices) |
| Display | None |
| Network | Not required after installation; optional |

## System overview

```text
┌──────────────────────────────────────────────────────────────────┐
│                PORTABLE OFFLINE VOICE TRANSLATOR                 │
│                         Raspberry Pi 5                            │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌───────────────────┐                                           │
│  │     MICROPHONE    │                                           │
│  │   Miniature USB   │                                           │
│  └─────────┬─────────┘                                           │
│            │ Audio                                               │
│            ▼                                                     │
│  ┌────────────────────────────────┐                              │
│  │         SPEECH-TO-TEXT         │                              │
│  │          Whisper.cpp           │                              │
│  │          Base Model            │                              │
│  └───────────────┬────────────────┘                              │
│                  │ Source text                                   │
│                  ▼                                               │
│  ┌────────────────────────────────┐                              │
│  │           TRANSLATION          │                              │
│  │     NLLB-200 Distilled 600M    │                              │
│  │     Language A  ──►  Language B│                              │
│  └───────────────┬────────────────┘                              │
│                  │ Translated text                               │
│                  ▼                                               │
│  ┌────────────────────────────────┐                              │
│  │          TEXT-TO-SPEECH        │                              │
│  │             Piper              │                              │
│  │       French / English voices  │                              │
│  └───────────────┬────────────────┘                              │
│                  │ Audio                                         │
│                  ▼                                               │
│  ┌────────────────────────────────┐                              │
│  │           AMPLIFIER            │                              │
│  │            2–3 W               │                              │
│  └───────────────┬────────────────┘                              │
│                  │                                               │
│                  ▼                                               │
│  ┌────────────────────────────────┐                              │
│  │            SPEAKER             │                              │
│  │           Miniature            │                              │
│  └────────────────────────────────┘                              │
│                                                                  │
├──────────────────────────────────────────────────────────────────┤
│                         USER CONTROLS                            │
│                                                                  │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────────────┐   │
│  │    BUTTON    │  │    BUTTON    │  │         BUTTON        │   │
│  │  LANGUAGE A  │  │  LANGUAGE B  │  │   PRESS TO SPEAK      │   │
│  └──────┬───────┘  └──────┬───────┘  └───────────┬───────────┘   │
│         └─────────────────┴──────────────────────┘               │
│                            │                                     │
│                            ▼                                     │
│                 ┌─────────────────────┐                          │
│                 │   RASPBERRY PI 5    │                          │
│                 │   Software control  │                          │
│                 │   Audio management  │                          │
│                 │   Language selection│                          │
│                 │   Pipeline control  │                          │
│                 └─────────────────────┘                          │
│                                                                  │
├──────────────────────────────────────────────────────────────────┤
│                           POWER                                  │
│                                                                  │
│  ┌─────────────────────┐                                         │
│  │    USB-C BATTERY    │                                         │
│  │    Power Delivery   │                                         │
│  └──────────┬──────────┘                                         │
│             ▼                                                    │
│  ┌─────────────────────┐                                         │
│  │   RASPBERRY PI 5    │                                         │
│  └─────────────────────┘                                         │
└──────────────────────────────────────────────────────────────────┘
```

## Critical consideration

Offline operation is possible, but the pipeline is only as good as the available models.
Whisper and Piper both need language-specific coverage, NLLB-200 must actually support the
language pair, and Hmong and the minority languages of Yunnan remain to be validated.
