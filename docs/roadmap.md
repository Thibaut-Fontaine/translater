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

**Status:** blocked on model quality. The delay-load fix for the ORT /
sentencepiece protobuf clash works, the `Mt` port and pair gate are in, and the
language ids are known. The published ONNX export produces degenerate output
even in reference Python, so no adapter can be validated against it. Findings,
tests and options: `docs/nllb-onnx.md`.

**Acceptance:** measured latency inside budget, no allocation per token, model
loaded once; unsupported pair returns an explicit status instead of garbage.

---

## M5 — TTS: Piper

**Todo**

- [ ] `Tts` port: text in, PCM chunks out through a callback or a ring.
- [ ] Adapter: per-language voice resolution from `LangInfo::piper`, streaming
      synthesis, first-chunk latency measured.
- [ ] Playback path from M2 reused; barge-in (PTT during speech) stops output.
- [ ] Tests with a fake voice; silence/length checks on real voices.

**Acceptance:** French and English spoken inside budget; a language without a
voice cannot reach the pipeline (already enforced by `ready()`).

---

## M6 — Runtime container: everything running on the Pi

**Todo**

- [ ] Dockerfile stages for whisper.cpp, ONNX Runtime and Piper, all aarch64,
      all pinned, all built with LTO where sane; final image ≤ 400 MB without
      models.
- [ ] Entrypoint runs `xtransd` with the real adapters; `--selftest` plays a
      canned utterance through STT→MT→TTS and exits non-zero on mismatch.
- [ ] systemd unit / compose restart policy, journal logging, stage timeouts
      (needed since M1) with recovery to idle.
- [ ] Audible ready prompt ("prêt" / "ready") through the speaker at boot.
- [ ] Compose invariants kept: no network, read-only rootfs, non-root, devices.

**Acceptance:** on the Pi, no keyboard, no network: press, speak, hear the
translation in both directions; `--selftest` green inside the container.

---

## M7 — SD image builder

**Todo**

- [ ] Reproducible image build script: Raspberry Pi OS Lite 64-bit base, Docker
      engine, `xtrans` image tar, models, compose file, unit, ALSA config,
      read-only log partition.
- [ ] SHA-256 manifest of every input; deterministic output where the format
      allows it.
- [ ] Verification job that mounts the produced image read-only (loopback) and
      checks: unit enabled, image tar present, model hashes, no network unit.
- [ ] Image size budget: ≤ 8 GB, leaves ≥ 50 GB usable on a 64 GB card.

**Acceptance:** image builds from a clean checkout with one command; the
loopback verification passes in test; the image boots on the Pi (M9).

---

## M8 — Flasher binary

**Todo**

- [ ] C++20, single static binary, no runtime deps; `--device`, `--dry-run`,
      `--yes`, `--image`, `--log`.
- [ ] Enumerates block devices and refuses anything not removable or not
      explicitly named; refuses mounted targets; never guesses.
- [ ] Writes image, verifies by reading back and comparing SHA-256, reports
      progress, flushes to the device, prints eject instructions.
- [ ] Tests against loopback files (`/tmp` images), simulated write errors,
      short writes, interruption at every step; no real card in tests.
- [ ] Optional: post-flash verification of the boot partition contents.

**Acceptance:** host test suite green; a 64 GB card flashed twice yields
byte-identical checksums; no way to hit the wrong disk without typing it.

---

## M9 — Hardware acceptance and release

**Todo**

- [ ] Full hardware loop: battery, mic, amp, speaker, three buttons, both
      directions, French ↔ English, Thai/Lao if validated.
- [ ] Thermal soak (30 min), power draw, SD-card wear check.
- [ ] Cold boot to ready prompt time measured.
- [ ] Docs: assembly, flashing, recovery, known limits (Hmong/Yunnan).
- [ ] Tag release with image hash and installer hash.

**Acceptance:** a person who has never seen the project can flash a card, plug
the battery, and translate.

---

## Ordering notes

- M1 is pure core and unblocked.
- M2 needs Docker access; M3–M5 need the dev image for verification too.
- M7 can start once M6's image is buildable, but its loopback verification runs
  on the host.
- No milestone starts before the previous one's reflection is written.
