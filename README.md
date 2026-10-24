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

Early project preparation. The core is C++20, built test-first; see
[AGENTS.md](AGENTS.md) for the rules every change follows.

Implemented and tested so far:

- language capability table, pair selection and button cycling
  (`include/xtrans/language.hpp`)
- 16 kHz PCM level measurement, lock-free SPSC sample ring, voice-activity gate
  with hysteresis (`include/xtrans/audio.hpp`)
- the turn state machine: idle → recording → transcribing → translating →
  speaking → idle (`include/xtrans/pipeline.hpp`)
- controller + line protocol (`include/xtrans/control.hpp`) and the `xtransd`
  daemon skeleton (`apps/xtransd.cpp`), driven on stdin until the GPIO adapter
  lands
- turn engine (`include/xtrans/device.hpp`): button debounce, capture buffer,
  gate endpointing, silence-turn discard, max-duration cut — tested against
  fake buttons and fake audio
- ALSA capture/playback and libgpiod buttons (`include/xtrans/adapters/`),
  compiled with `-Werror` in the dev image; `xtransd --device` runs them,
  `--stdin` stays the default
- speech recognition: pinned whisper.cpp behind the `Stt` port
  (`include/xtrans/stt.hpp`, `WhisperStt`), model loaded once, transcripts
  assembled in a bounded UTF-8 buffer
- translation: NLLB-200-distilled-600M int8 ONNX behind the `Mt` port
  (`include/xtrans/adapters/nllb.hpp`), KV-cached decoder, pinned by SHA-256 in
  `models/nllb.pin`; en→fr and fr→en match the reference implementation

`xtransd --device --model FILE --mt-dir DIR` runs recognition and translation
per turn and logs status and lengths only, never the text. `xtrans-mt` and
`xtrans-tts` are one-shot tools for benchmarking.

`xtrans-install --image FILE --device NAME` writes an SD card and verifies it
byte for byte; it refuses fixed disks, mounted targets and images that do not
fit (`--dry-run` runs the checks only).

`xtransd --selftest` runs a sentence through the whole chain without hardware:
Piper synthesises French, Whisper transcribes it, NLLB translates to English,
Piper speaks the result, and both WAVs are written for inspection.

Not implemented yet: Piper voices, playback, and the systemd unit. Hardware I/O
is compile-verified but not yet exercised on a Pi. Latency is measured on the
host only. See [docs/roadmap.md](docs/roadmap.md) for the milestone order.

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

Release, sanitizers and cross-compilation for the Pi:

```bash
cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release
cmake -S . -B build-asan -DXTRANS_SANITIZE=address,undefined
cmake -S . -B build-arm -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake \
      -DCMAKE_BUILD_TYPE=Release
```

## Container

Two images: a dev/test image and a cross-built arm64 runtime image for the Pi.

```bash
./scripts/docker.sh --self-test   # compose invariants, no daemon needed
./scripts/docker.sh test          # build image, run all tests during the build
./scripts/docker.sh smoke         # run xtransd with scripted input
./scripts/docker.sh image         # build/xtrans-arm64.tar for the device
```

See [docs/docker.md](docs/docker.md) for deployment, device passthrough and the
security posture (non-root, read-only rootfs, no network).

## Languages

Target languages: French, English, Thai, Lao, Hmong, and selected minority languages of
Yunnan. Speech recognition and speech synthesis require models suitable for each selected
language. NLLB-200 has no Hmong at all, and of the Yunnan languages it covers only Tibetan,
Jingpho and Shan — the rest need a different translation model. See
[docs/languages.md](docs/languages.md) for the verified list.

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

## License

The code in this repository is **MIT** (see [LICENSE](LICENSE)).

That covers this project's own sources only. The device image and the vendored
dependencies carry their own terms:

| Component | License | Notes |
| --- | --- | --- |
| whisper.cpp | MIT | fetched at build time |
| ONNX Runtime | MIT | Debian package |
| SentencePiece | Apache-2.0 | Debian package |
| Piper | **GPL-3** | fetched at build time; a distributed image must offer its source |
| espeak-ng, open_jtalk | GPL-3 / MIT | built by Piper at build time |
| libasound2, libgpiod | LGPL-2.1 | Debian packages |
| NLLB-200-distilled-600M | **CC-BY-NC-4.0** | not in this repository; bundled in the image, **non-commercial use only** |
| Piper voices | per voice (CC-BY / MIT) | see each voice's MODEL_CARD |

So: the repository is MIT, but an SD image built from it is **not** MIT-only —
it is a GPL-3 work (Piper) that embeds a non-commercial model (NLLB). If you
want to distribute the image, keep the GPL-3 source offer and respect the
NLLB non-commercial restriction, or replace NLLB with a redistributable model.
