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

## M2 — OS adapters: ALSA capture/playback and GPIO buttons

**Goal:** the same `Device` driving real hardware interfaces, compiled in the
dev image (which carries `libasound2-dev` and `libgpiod-dev`).

**Todo**

- [ ] `AlsaIn` / `AlsaOut`: 16 kHz mono S16_LE, non-blocking capture thread into
      the existing SPSC ring, blocking playback with underrun counting.
- [ ] `GpiodButtons`: three lines, active-low with pull-ups, edge polling,
      debounce delegated to `Debouncer` (no logic in the adapter).
- [ ] CMake: `XTRANS_WITH_ALSA`, `XTRANS_WITH_GPIOD` (auto-detect, OFF if the
      headers are absent), so host tests stay dependency-free.
- [ ] Dev image installs the two `-dev` packages; a `--fake` mode keeps the
      container runnable without devices.
- [ ] Device-side smoke script: record 2 s, measure RMS, play back a tone.

**Acceptance:** adapters compile in the dev image with warnings-as-errors; the
smoke script runs on the Pi; no project logic inside adapters.

**Blocked by:** Docker access on the workstation (build verification).

---

## M3 — STT: whisper.cpp behind the `Stt` port

**Goal:** PCM utterance in, text out, with the language fixed by the pair.

**Todo**

- [ ] Vendor whisper.cpp at a pinned commit under `third_party/`, static lib.
- [ ] `Stt` port: `transcribe(span<const int16_t>, Lang) -> text` into a caller
      buffer, explicit status codes.
- [ ] Adapter: model loaded once at startup, 4 threads, no prompt, language from
      `LangInfo::whisper`, empty result means "nothing recognised".
- [ ] Tests with a fake `Stt`; real-model check in the container only.
- [ ] Microbenchmark committed: 3 s utterance → ms, on the Pi and on the host.

**Acceptance:** French and English utterances transcribe on the device inside the
budget from `AGENTS.md` §6; unready languages are never requested.

---

## M4 — MT: NLLB-200 Distilled 600M via ONNX Runtime

**Todo**

- [ ] `Mt` port: `translate(span<const char> src, Lang from, Lang to, span<char> out)`.
- [ ] SentencePiece tokenizer wired to NLLB's 256k vocabulary, truncation at a
      fixed token budget.
- [ ] ONNX Runtime adapter: int8 model, `src_lang`/`tgt_lang` tokens, greedy
      decode, one session per process.
- [ ] Tests: token space mapping, truncation, empty input, unknown pair; fake
      model in unit tests.
- [ ] Benchmark: fr→en and en→fr latency, RSS.

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
