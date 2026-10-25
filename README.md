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

The pipeline is complete and tested on the host; the device image builds and
verifies; **nothing has been booted on a Raspberry Pi yet** (M9 in
[docs/roadmap.md](docs/roadmap.md)).

| Piece | State |
| --- | --- |
| Core (language table, gamma gate, ring, turn engine, state machine) | done, unit-tested |
| whisper.cpp STT | works with a real model |
| NLLB-200 int8 MT with KV cache | en→fr and fr→en verified against the reference |
| Piper TTS | fr/en verified, WAVs compared |
| Full chain `--selftest` | passes on x86 and on arm64 (under QEMU) |
| ALSA capture/playback, GPIO buttons | compiled; never run on hardware |
| arm64 container | built, selftest passes emulated |
| SD image (6 GB) | built and verified (14 checks) |
| Flasher | refuses unsafe targets; verifies by read-back |
| Latency vs the 1.6 s target | unproven; needs the Pi |

Not implemented: Thai speech output (its voice needs a Piper ≥ 1.9 frontend),
Hmong and Yunnan minority languages (no model coverage — see the
[language coverage](#language-coverage-whisper-vs-piper-vs-nllb-200) table).

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

Release, sanitizers and cross-compilation:

```bash
cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release
cmake -S . -B build-asan -DXTRANS_SANITIZE=address,undefined
cmake -S . -B build-arm -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake \
      -DCMAKE_BUILD_TYPE=Release
```

Unit tests never touch audio, GPIO or a model. `xtransd --selftest` is the
integration check: it synthesises French, transcribes it, translates to English,
speaks the result and writes both WAVs.

## Deploy on a Raspberry Pi 5

### What you need

- Raspberry Pi 5 (8 GB recommended), microSD card **≥ 8 GB**, card reader.
- USB-C PD battery, USB microphone, 2–3 W amplifier + speaker.
- Three momentary buttons wired between **BCM 17 / 27 / 22** and **GND**
  (internal pull-ups are enabled in software).
- A Linux workstation with Docker + buildx and **~15 GB free**.
- No QEMU is needed to *build*: the arm64 container is cross-compiled on the
  host CPU. QEMU/binfmt is only needed if you want to run it under emulation.

### 1. Fetch the pinned sources and models (~2 GB download)

```bash
git clone https://github.com/Thibaut-Fontaine/translater.git
cd translater
scripts/fetch_third_party.sh    # whisper.cpp + Piper sources (pinned commits)
scripts/fetch_models.sh         # NLLB int8 + whisper model + fr/en/th voices
```

### 2. Build the arm64 container

```bash
./scripts/docker.sh image       # cross-builds and loads xtrans:arm64
```

Optional check, emulated, ~16 minutes:

```bash
docker run --rm --platform linux/arm64 \
  -v "$PWD/models/nllb:/nllb:ro" -v "$PWD/models/voices:/voices:ro" \
  -v "$PWD/models/ggml-base.bin:/whisper.bin:ro" xtrans:arm64 \
  xtransd --selftest --model /whisper.bin --mt-dir /nllb --voice-dir /voices \
    --espeak-data /opt/xtrans/espeak-ng-data --out /tmp
```

### 3. Build the SD image (~10 minutes, no root)

```bash
./scripts/build_image.sh
```

It downloads the pinned Raspberry Pi OS Lite base, grows the partition, writes
the payload with `debugfs` and verifies the result — all without mounting
anything or requiring sudo. Output: `build/image/xtrans.img` (6 GB).

### 4. Flash the card

```bash
lsblk                                            # find the card, e.g. sdb
./build/xtrans-install --image build/image/xtrans.img --device sdb
```

The tool refuses fixed disks, mounted targets and images that do not fit, asks
you to type the device name, then verifies the card byte for byte. Cards larger
than 6 GB keep the extra space unused (the image is not grown on boot).

### 5. First boot

Insert the card, connect the battery, wait for the spoken **"Prêt."**. No
screen, keyboard or network is used. Under the hood: `containerd` starts,
`docker` starts with networking and iptables disabled (`network_mode: none`),
a one-shot unit loads the embedded container image, then `xtrans.service` runs
`docker compose up -d`.

Useful checks over SSH or a serial console:

```bash
journalctl -u xtrans -f
systemctl status containerd docker xtrans
docker ps                       # container "xtrans", restart unless-stopped
```

Troubleshooting: if the prompt never plays, check `journalctl -u xtrans` first;
the container logs `ready`, then one line per turn (`stt status=`, `mt status=`,
`tts status=`) without ever printing the transcript or translation.

## Mobile app (Android; iOS shell)

`mobile/` is a React Native app around the same C++ core. The screen shows the
transcript and the translation, so a pair only needs NLLB: **202 languages**
(201 translatable, 87 with speech input) instead of the Pi's two. Languages
without a whisper code are typed; languages without a voice are read out by the
OS speech engine.

Status: the Android APK builds (73 MB, `arm64-v8a`) with a TurboModule, native
capture, ONNX Runtime 1.21 and whisper.cpp. **It has not run on a phone yet.**
The iOS UI is shared but its Objective-C++ bridge still needs a Mac
([docs/mobile-ios.md](docs/mobile-ios.md)).

```bash
scripts/fetch_third_party.sh && scripts/fetch_models.sh
scripts/fetch_ort_android.sh
cd mobile && npm install
cd android && ./gradlew assembleRelease
cd ../.. && scripts/push_models_android.sh   # 1.4 GB over USB
adb install -r mobile/android/app/build/outputs/apk/release/app-release.apk
```

Design and decisions: [docs/mobile-app.md](docs/mobile-app.md). Build details:
[mobile/README.md](mobile/README.md).

## Container (development)

The dev image is for tests and benchmarks; the arm64 image is what ships.

```bash
./scripts/docker.sh --self-test   # compose invariants, no daemon needed
./scripts/docker.sh test          # build dev image, run the whole suite in it
./scripts/docker.sh smoke         # xtransd with scripted stdin
```

See [docs/docker.md](docs/docker.md) for the security posture (non-root,
read-only rootfs, no network) and the model layout.

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

## Language coverage: Whisper vs Piper vs NLLB-200

A language can only be translated end to end when **all three** components exist:
Whisper (speech in), NLLB-200 (translation), Piper (speech out). Counts verified
2026-10-24 from:

- **Whisper** — [whisper-api.com/docs/languages](https://whisper-api.com/docs/languages/):
  **100 languages**, including Lao (`lo`), Thai (`th`), Tibetan (`bo`), but no
  Hmong, Jingpho or Shan.
- **Piper** — [voice repository](https://huggingface.co/rhasspy/piper-voices):
  **178 voices across 53 languages**. The upstream
  [VOICES.md](https://github.com/OHF-Voice/piper1-gpl/blob/main/docs/VOICES.md)
  lists only 44 and omits Thai, so the repository is what we use.
- **NLLB-200** — [FLORES-200](https://github.com/facebookresearch/flores/blob/main/flores200/README.md):
  **200 languages** (205 codes; some languages have two scripts).

| Language | Whisper (STT) | NLLB-200 (MT) | Piper (TTS) | All three |
| --- | --- | --- | --- | --- |
| French | `fr` | `fra_Latn` | 7 voices (`fr_FR`) | **yes** |
| English | `en` | `eng_Latn` | 38 voices (`en_US`, `en_GB`) | **yes** |
| Vietnamese | `vi` | `vie_Latn` | 3 voices (`vi_VN`) | yes |
| Chinese | `zh` | `zho_Hans`, `zho_Hant` | 4 voices (`zh_CN`) | yes |
| Indonesian | `id` | `ind_Latn` | 1 voice (`id_ID`) | yes |
| Thai | `th` | `tha_Thai` | 1 voice (`th_TH-tsync2-medium`), needs a Piper ≥ 1.9 frontend | **no** |
| Lao | `lo` | `lao_Laoo` | **no voice** | no |
| Khmer | `km` | `khm_Khmr` | **no voice** | no |
| Burmese | `my` | `mya_Mymr` | **no voice** | no |
| Malay | `ms` | `zsm_Latn` | **no voice** | no |
| Tagalog | `tl` | `tgl_Latn` | **no voice** | no |
| Tibetan | `bo` | `bod_Tibt` | **no voice** | no |
| Jingpho (Kachin) | **no** | `kac_Latn` | **no voice** | no |
| Shan | **no** | `shn_Mymr` | **no voice** | no |

What this means for the device:

- **Only five languages have all three components**, and this project validates
  just French and English end to end today.
- **Thai is close**: STT and MT exist, and a voice exists, but it declares
  `phoneme_type: "thai"`, a frontend Piper 1.8 does not implement. Until Piper
  gains it, Thai has no speech output.
- **The screenless, bidirectional design makes TTS mandatory.** A language
  without a voice cannot be a *target*: Lao, Khmer, Burmese, Malay, Tagalog and
  Tibetan can be translated into but never spoken.
- **Hmong is absent from all three**, and of the Yunnan languages NLLB covers
  only Tibetan, Jingpho and Shan — cross-border varieties, not the full set.
  No model evaluated (NLLB, SMaLL-100, Sailor, Qwen 0.5B) covers Hmong.

The full matrix of every language covered by at least one of the three stacks is in [docs/language-matrix.md](docs/language-matrix.md); per-language project decisions
are in [docs/languages.md](docs/languages.md).

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
