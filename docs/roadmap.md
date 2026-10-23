# Roadmap

Milestones for turning the tested core into a device that boots from an SD card
and translates offline. Every milestone is delivered test-first, ends with a
green `ctest`, and is followed by a written reflection before the next one
starts. A milestone is not "done" because code exists; it is done when its
acceptance criteria are met in the environment listed.

Architecture recap (see `AGENTS.md` §8): core state machine + ports, thin
adapters around them, one container, one installer.

---

## M0 — Core and rules (done)

**Acceptance:** `AGENTS.md`, language table, audio primitives, pipeline, control
layer and stdin-mode daemon merged; 46 tests green in Debug, Release, ASan/UBSan.

**Reflection:** the first TDD pass paid off in exactly one place — the button
cycle collapsed both directions onto the same language, which the tests caught
before any UI existed. Weakness: the daemon is a stdin shell, so nothing yet
proves the hardware path. The state machine also has no notion of time, which
means stage timeouts (AGENTS §10) are still unimplemented and must be added
before the container milestone.

---

## M1 — Turn engine: ports, debounce, recorder, device loop

**Goal:** turn a PTT press into a captured utterance, using only host-testable
code. No OS dependency, no audio device, no allocation.

**Todo**

- [x] Ports: `ButtonSource`, `AudioIn`, `AudioOut` (pure virtual, `noexcept`).
- [x] `Debouncer`: 3-sample stable detection, edge events for A/B/PTT,
      one feed yields at most one edge per button.
- [x] `Recorder`: fixed capture buffer, gate-based endpointing, max duration,
      overflow flag, `speech()` so silence-only turns are dropped.
- [x] `Device`: one `tick()` = poll buttons → debounce → start/stop a turn →
      read one frame → feed recorder; emits a finished turn to the caller.
- [x] Fake `ButtonSource` / `AudioIn` in tests; no sleeps, no threads.
- [x] Tests: debounce bounce, multi-button edges, silence discard, endpointing,
      max-duration cut, audio error abort, language buttons while idle.

**Acceptance:** `ctest` green on the host; no adapter code, no new dependency;
`Device` provably drives `Controller` from scripted button and audio input.
**Met:** 70 tests green (24 new), two deliberate mutations (removing the
speech check, shortening the debounce) each broke the corresponding tests.

**Reflection:** the port/fake split paid off immediately — the entire
button→utterance path is tested with no hardware, no sleeps and no threads.
Two things the tests forced that I would otherwise have shipped:

- a silence-only push-to-talk produces a *discard*, not a zero-length turn for
  Whisper. The first sketch dispatched it; the `has_speech()` test caught it.
- an audio failure in mid-turn must drop the utterance rather than send a
  truncated recording downstream.

Weaknesses to carry forward: `Device` has no notion of time, so per-stage
timeouts (`AGENTS.md` §10) and a "recording too long" cue are still missing;
`AudioOut` is defined but unused; the daemon is still stdin-only, so none of
this runs on hardware until M2. The virtual `AudioIn::read` call is once per
20 ms frame — acceptable, but it is the first thing to inline if profiling ever
shows up in the sampling loop.

---

## M2 — OS adapters: ALSA capture/playback and GPIO buttons (compile-verified)

**Goal:** the same `Device` driving real hardware interfaces, compiled in the
dev image (which carries `libasound2-dev` and `libgpiod-dev`).

**Todo**

- [x] `AlsaIn` / `AlsaOut`: 16 kHz mono S16_LE, non-blocking capture thread into
      the existing SPSC ring, blocking playback with underrun counting.
- [x] `GpiodButtons`: three lines, active-low with pull-ups, edge polling,
      debounce delegated to `Debouncer` (no logic in the adapter).
- [x] CMake: `XTRANS_WITH_ALSA`, `XTRANS_WITH_GPIOD` (auto-detect, OFF if the
      headers are absent), so host tests stay dependency-free.
- [x] Dev image installs the two `-dev` packages; `--device` mode in the daemon.
- [~] Device-side smoke script (record 2 s, RMS, tone): moved to M6, where the
      container and the Pi exist; `--device` already fails loudly (exit 3) when
      there is no audio hardware.

**Acceptance:** adapters compile in the dev image with warnings-as-errors; the
smoke script runs on the Pi; no project logic inside adapters.
**Met (compile side):** `scripts/docker.sh test` builds ALSA + libgpiod adapters
with `-Werror` and the full suite passes inside the image; `scripts/docker.sh
smoke` asserts the adapters were built and that `--device` exits 3 without a
sound device.

**Reflection:** building in the dev image, not just on the host, immediately
paid for itself. Two defects existed only in that configuration:

- `#if defined(GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP)` is **always false** —
  libgpiod's flags are enum constants, not macros. The pull-up would have been
  silently disabled and the buttons would float on real hardware. No host test
  could have seen this; only compiling against the real header did.
- `gpiod_line_request_input_flags` takes an `int`, not an `unsigned long`;
  `-Wconversion` + `-Werror` caught the narrowing.

Lesson recorded in the code: no `#if defined()` on enum constants, libgpiod
>= 1.6 is required, and adapters must be compiled in the image on every build.

Still unverified: actual ALSA/GPIO I/O, which no test environment here can
exercise. `AudioOut` is implemented but unused until Piper (M5). The daemon's
device mode is record-only and aborts the pipeline after each turn until M3.

**Blocked by:** Docker access on the workstation (build verification).

---

## M3 — STT: whisper.cpp behind the `Stt` port

**Goal:** PCM utterance in, text out, with the language fixed by the pair.

**Todo**

- [x] Vendor whisper.cpp at a pinned commit under `third_party/`, static lib
      (pin + digest + `scripts/fetch_third_party.sh`, no submodule).
- [x] `Stt` port: `transcribe(span<const int16_t>, Lang) -> text` into a caller
      buffer, explicit status codes; `TextBuffer` for bounded UTF-8 assembly.
- [x] Adapter: model loaded once at startup, 4 threads, no prompt, language from
      `LangInfo::whisper`, empty result means `no_speech`.
- [x] Tests for the text assembly (join, trim, truncation, UTF-8 boundary);
      real-model check run with the base model on `jfk.wav`.
- [~] Microbenchmark: host numbers recorded below; device numbers move to M6.

**Acceptance:** French and English utterances transcribe on the device inside the
budget from `AGENTS.md` §6; unready languages are never requested.
**Partially met:** English verified end to end on the host (real model, real
speech). French not yet measured; device latency not yet measured.

**Reflection:** three things worth recording.

- **Vendoring by pin, not submodule.** The repo keeps a 4-line pin file; the
  9 MB tarball and 40 MB tree stay out of git, and `fetch_third_party.sh`
  verifies the digest before extracting. CI can still build offline once the
  tree is present.
- **The first adapter draft was wrong** and only the real-model probe showed
  it: it split the utterance into 16 k-sample chunks and called `whisper_full`
  per chunk, which restarts decoding and silently drops context. Replaced with
  a 15 s float buffer allocated once at `open()`, matching the recorder's cap.
- **Measured latency is not the budget yet.** `jfk.wav` (11 s, English, base
  model, 4 threads) takes ~22 s wall on this x86 host: far outside real time.
  A 3 s clip costs the same ~21 s, confirming that `whisper_full` always
  processes a 30 s window. q5_1 was *slower* here (55 s), a host artefact with
  no optimized quantized dot product. Numbers and caveats are in
  `docs/measurements.md`. The 700 ms budget needs a Pi 5 measurement before it
  is kept or changed; model choice (`base`, `base.en`, `tiny`) is a decision
  for the owner, not a guess to bury in code.

Privacy held: the daemon logs `stt status=N chars=M`, never the transcript.

---

## M4 — MT: NLLB-200 Distilled 600M via ONNX Runtime

**Todo**

- [x] ONNX Runtime from Debian trixie (`libonnxruntime-dev` 1.21.0, arm64),
      same package Piper links against in M5 — no vendor binary. Images moved
      to trixie as part of this, which forced the libgpiod 2.x port.
- [x] `Mt` port and `translatable()` gate (both languages ready, pair distinct).
- [ ] Model provisioning: NLLB-200-distilled-600M as ONNX int8 (pinned revision
      + per-file SHA-256); the device never converts, only loads. License is
      CC-BY-NC-4.0 — personal use only, confirmed by the owner.
      Options under evaluation: pinned community export (encoder int8 415 MB +
      decoder quantized 471 MB + 5 MB tokenizer, ~900 MB) versus exporting
      locally with optimum (needs ~10 GB disk and 8+ GB RAM; this workstation
      has 15 GB free, 6 GB RAM).

- [ ] `Mt` port: `translate(span<const char> src, Lang from, Lang to, span<char> out)`.
- [ ] SentencePiece tokenizer wired to NLLB's 256k vocabulary, truncation at a
      fixed token budget.
- [ ] ONNX Runtime adapter: int8 model, `src_lang`/`tgt_lang` tokens, greedy
      decode, one session per process.
- [ ] Tests: token space mapping, truncation, empty input, unknown pair; fake
      model in unit tests.
- [ ] Benchmark: fr→en and en→fr latency, RSS.

**Status:** done for correctness, open on performance.

- `NllbMt` runs the int8 encoder + decoder pair with KV cache; en→fr and fr→en
  produce the same output as the reference implementation.
- Model provenance: `models/nllb.pin` (revision + per-file SHA-256), fetched by
  `scripts/fetch_models.sh`; 1.3 GB total.
- Latency on this x86 host: ~4.4 s for 6 decoder steps and ~6.6 s for 12, i.e.
  ~550–730 ms per decoder step. Far from the 500 ms budget, but this host is an
  old VM without VNNI; the Pi 5 measurement decides whether the ONNX int8 path
  is viable or whether we switch the MT stage to CTranslate2.
- `xtransd --device --model … --mt-dir …` now runs STT then MT per turn and
  logs status plus lengths, never the text.

**Acceptance:** partially met — quality and wiring verified, latency unproven.

**Reflection:** three findings worth keeping:

1. The merged decoder export cannot take an empty cache: ORT fails inside an
   `If` branch reshape, reproduced in reference Python. The classic pair
   (`decoder_model` + `decoder_with_past_model`) works and is only 100 MB
   larger, so that is what we ship.
2. The with-past decoder emits only self-attention KV (24 tensors), not
   cross-attention: the cache has to be rotated by name, keeping encoder slots
   from the first step. A naive "presents in order" rotation silently feeds
   wrong tensors.
3. Decoder cost is per-step, not per-sentence: 6 vs 12 steps scales the time.
   If the Pi confirms ~600 ms/step, the fix is a faster decoder path, not a
   smaller model — the tokens are already int8. The delay-load fix for the ORT /
sentencepiece protobuf clash works, the `Mt` port and pair gate are in, and the
language ids are known. The published ONNX export produces degenerate output
even in reference Python, so no adapter can be validated against it. Findings,
tests and options: `docs/nllb-onnx.md`.

**Acceptance:** measured latency inside budget, no allocation per token, model
loaded once; unsupported pair returns an explicit status instead of garbage.

---

## Phases

- **Phase 1 — no hardware required.** Everything that can be built and exercised
  on this workstation or in the dev container: TTS, full pipeline wiring,
  self-test, SD image construction, flasher. All functional risk is retired
  here.
- **Phase 2 — hardware.** Only what needs a Pi: audio/GPIO on real devices,
  latency tuning against the 1.6 s target, thermals, power, release.

Two facts bound Phase 1 and were verified, not assumed:

- **arm64 cannot be executed here**: `docker run --platform linux/arm64`
  returns `exec format error` (no binfmt/QEMU). An aarch64 image can still be
  *built* (cross toolchain + a sysroot assembled from Debian .debs), but never
  *run* on this host. So arm64 binaries are packaging artifacts until Phase 2.
- **Piper does not build from Debian packages alone**: `libpiper` compiles
  espeak-ng from source at build time (`ExternalProject` → git), and needs a
  newer espeak-ng than trixie's 1.52 (it uses
  `espeak_TextToPhonemesWithTerminator`). Both must be vendored and pinned, and
  the ExternalProject pointed at the local tree.

---

## M5 — TTS: Piper (Phase 1)

**Todo, with feasibility**

| Sub-step | Feasible here? | Notes |
| --- | --- | --- |
| Pin `piper1-gpl` (GPL-3) + espeak-ng tarballs by SHA-256 | yes | extend `third_party/` pins + `fetch_third_party.sh` |
| Build `libpiper` against vendored espeak-ng + Debian ONNX Runtime | yes | patch `ExternalProject` to use the local espeak-ng tree (`third_party/patches/`, documented) |
| Voice files fr/en/th (`.onnx` + `.onnx.json`) | yes | ~60–75 MB each; lao/hmong have no voice |
| `Tts` port + adapter, sentence resolution from `LangInfo::piper` | yes | streaming PCM out; no hardware needed |
| Verify audio by writing WAV to disk (no speaker) | yes | check format 16 kHz mono, duration, RMS; compare against known phrases |
| First-chunk latency on x86 | yes | relative signal only; Pi numbers in M9 |
| Barge-in (PTT stops playback) | yes | feed the `Device` loop with a fake `AudioOut` |

**Acceptance:** `xtrans-tts` produces a correct WAV for fr/en; a sentence
synthesises in < 250 ms on the dev host (Pi number deferred).
**Met except latency:** fr/en WAVs are correct (1.4–1.5 s of audio); steady
state is 256–289 ms on this host, just over budget; Thai is blocked (below).

**Reflection:** five defects, four of them only visible in the image build:

1. A stray `docker run --platform linux/arm64` had retagged the local
   `debian:trixie-slim` as arm64, so the next dev build died with
   `exec format error`. The dev build now pins `--platform linux/amd64`; the
   lesson is that local image tags are mutable state.
2. Piper's CMake reads `${CMAKE_SOURCE_DIR}/../setup.py` for its version, which
   escapes our repo under `add_subdirectory`. Patched to read relative to the
   CMakeLists (`third_party/patches/piper-cmake-version.patch`).
3. Piper 1.8 uses ONNX Runtime ≥ 1.22's `GetOutputNames()`; trixie ships 1.21.
   Patched to the allocator API (`piper-ort121.patch`).
4. Piper downloaded its own ONNX Runtime because Debian ships no CMake package
   config. Added `cmake/onnxruntimeConfig.cmake` so both Piper and our adapter
   link the distro library — this also matters for arm64 cross builds.
5. `piece_count`/API differences aside, the Thai voice declares
   `phoneme_type: "thai"`, a frontend piper 1.8 does not implement: synthesis
   fails cleanly. Thai therefore has **no usable voice** today, which
   reinforces the language table.

Two design choices that paid off: float→int16 conversion and 22.05→16 kHz
resampling are ours (unit-tested, no dependency), so the audio path stays at
one rate; and voices are warmed in `open()` because the first call costs ~9.6 s
(model + ORT session) versus 289 ms warm.

---

## M6 — Full pipeline wiring and self-test (Phase 1)

**Todo, with feasibility**

| Sub-step | Feasible here? | Notes |
| --- | --- | --- |
| Wire STT → MT → TTS in `xtransd --device` (no abort after MT) | yes | pure code + fakes |
| `--selftest`: WAV in → transcript → translation → WAV out, non-zero exit on failure | yes | use `jfk.wav` (en→fr) and a locally synthesised French WAV (fr→en) |
| Stage timeouts in `Pipeline` (`AGENTS.md` §10) | yes | deterministic tests with an injected clock value, no sleeps |
| Audible ready prompt generated at startup | yes | write WAV, verify non-empty; playback only in M9 |
| systemd unit + compose files | yes | `systemd-analyze verify` in the container; boot test in M9 |
| Model/voice layout under `/opt/xtrans/models` | yes | already documented in `docs/docker.md` |

**Acceptance:** `--selftest` passes on the x86 dev container with the real
models, and produces two WAV files whose duration is plausible.
**Met.** In the trixie image: French prompt synthesised (29 535 samples) →
transcribed (29 chars) → translated (16 chars) → English speech (19 133
samples); both WAVs written. 102 tests green.

**Reflection:** the selftest paid for itself immediately by exposing a crash no
unit test could reach.

1. **Piper links ONNX Runtime directly.** That pulls ORT's bundled protobuf
   into the global namespace at startup, and libsentencepiece's descriptor
   registration segfaults *before* `main` (`ReflectionOps::FindInitializationErrors`
   inside `DescriptorPool::InternalAddGeneratedFile`). The delay-load design
   only works if nothing links ORT, so the CMake shim is now an **interface**
   target (headers only) and piper's CLI subdirectory is skipped
   (`piper-no-cli.patch`). Every executable gets the loader object.
2. **Static archives are scanned before shared libraries.** Keeping
   `ort_delay_load.cpp` inside `libxtrans.a` left `OrtGetApiBase` undefined
   when `libpiper.so` asked for it. The loader is an OBJECT library added to
   each executable now — link order, not code, was the bug.
3. **Timeouts are state-machine only.** The watchdog catches a stage that
   never reports; a native call that blocks forever still needs the process
   supervisor (`Restart=on-failure`, M9 adds `WatchdogSec`). That limitation
   is written down rather than hidden.
4. Barge-in is chunked playback (100 ms) with a `Device::tick()` between
   chunks; it stops speech on a new press. Untested on hardware.

---

## M7 — SD image construction (Phase 1)

**Todo, with feasibility**

| Sub-step | Feasible here? | Notes |
| --- | --- | --- |
| Download and pin Raspberry Pi OS Lite arm64 (trixie) | yes | ~1.2 GB, SHA-256 pinned |
| Inject Docker engine, compose file, unit, models, container tar | yes | needs Docker arm64 packages (download .deb, no execution) |
| Cross-build the arm64 container | yes, build-only | sysroot from Debian arm64 .debs; cannot be run here |
| Read-only data partition + boot config | yes | file-level edits, no execution |
| Loopback verification (mount, check hashes, unit enabled) | yes, with caveats | needs `--privileged` container or sudo; verify early with a small ext4 image |
| Reproducibility | partial | filesystem timestamps vary; pin inputs and document the nondeterminism |

**Acceptance:** image builds with one command; verification script reports
model hashes, unit, docker, and free space; image ≤ 8 GB.

**Risks:** mounting inside the Docker Desktop VM may be restricted. If
`--privileged` mounting fails, fall back to `mtools`/`e2ls`-style inspection of
the FAT boot partition plus a hash manifest, and accept that full mount
verification happens in M9.

---

## M8 — Flasher binary (Phase 1)

**Todo, with feasibility**

| Sub-step | Feasible here? | Notes |
| --- | --- | --- |
| Enumerate block devices, refuse non-removable/mounted/ambiguous | yes | `/sys/class/block`, `/proc/mounts`; no hardware needed |
| Write image, flush, read-back SHA-256 verify | yes | tests against regular files in `/tmp`; block device path only in M9 |
| Progress, `--dry-run`, `--yes`, `--log` | yes | plain C++20, no deps |
| Tests: short writes, interrupted write, wrong device, EACCES | yes | injectable writer interface + fakes |
| Static-ish binary (no runtime deps beyond libc) | yes | `-static-libstdc++ -static-libgcc`, verify with `ldd` |

**Acceptance:** loopback test suite green; checksum mismatch is detected and
reported; no code path writes without an explicit `--device`.
**Met for the policy:** 7 new tests cover every refusal; on this workstation the
tool refuses the fixed disk (`not removable`, exit 3) and a path instead of a
bare name (exit 2). The write and read-back paths are code-complete but only
executable with real hardware (M9).

**Reflection:** the useful design decision was to make the safety policy a
**pure function** (`assess_target`) over an injected `BlockDevice`. Every
refusal — fixed disk, mounted, too small — is a table test, not a mock of a
syscall. The I/O is deliberately dumb: 1 MB chunks, `O_SYNC`, `fsync`, then a
byte-for-byte read-back; no checksum library, no cleverness.

Honest gaps:

- there is a **race** between the mount check and the first write (a filesystem
  could be mounted in between). `O_EXCL` does not protect block devices; the
  fix is a lock or a udev-level check, and it matters more for a tool that
  might be handed to someone else than for the owner flashing their own card;
- `device_mounted()` matches the device name as a substring of `/proc/mounts`,
  which is right for `sdb`/`sdb1` but could in principle match a path that
  merely contains the name;
- the progress output and the read-back loop are untested until hardware.

---

## M9 — Hardware bring-up, tuning and release (Phase 2)

Only this milestone requires the Pi 5 (8 GB) and the card reader.

| Sub-step | Needs |
| --- | --- |
| Flash a card, boot, read the journal over serial/network | Pi + SD |
| ALSA capture/playback on the real mic/amp/speaker | Pi + audio hardware |
| GPIO buttons on real switches (debounce under real bounce) | Pi + buttons |
| Latency measurement per stage; 1.6 s target | Pi |
| Runtime decision if over budget: CTranslate2 for MT, `tiny`/`audio_ctx` for STT | Pi + owner decision |
| Ready prompt, both directions, spoken output | Pi + speaker |
| Thermal soak (30 min), power draw, SD wear | Pi + battery |
| Release: image hash + flasher hash, docs | — |

**Acceptance:** a person who has never seen the project flashes a card, plugs
the battery, and translates.

---

## Ordering notes

- No milestone after M4 may depend on the Pi until M9.
- arm64 artifacts are built, not executed, before M9. Any claim about Pi
  behaviour is deferred, not assumed.
- The 1.6 s promise stays a hard target; the lever set (CTranslate2, whisper
  model size/audio_ctx) is prepared in Phase 1 where it can be measured
  relatively, and decided in M9 with Pi numbers.
