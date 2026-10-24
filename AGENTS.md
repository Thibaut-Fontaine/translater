# AGENTS.md — Portable Offline Voice Translator

Operating rules for any agent working in this repository. Read this file fully
before acting. It is project-specific and overrides general habits; global and
host rules still apply and come first.

## 1. What this is

A handheld, **fully offline** bidirectional voice translator on a Raspberry Pi 5.
Three physical buttons (language A, language B, press-to-speak), no screen, no
network at runtime:

```
Speech A → Whisper.cpp → Text A → NLLB-200 Distilled 600M → Text B → Piper → Speech B
```

Push-to-talk captures one utterance, transcribes it, translates it, and speaks
the result. Pressing the language button swaps direction (A↔B).

Target languages: French, English, Thai, Lao, Hmong, and selected Yunnan
minority languages. Only languages whose **recognizer, translator, and voice**
are all validated may be offered by the UI; today that is French and English.
See `docs/languages.md` and `docs/language-matrix.md`.

**Current state (2026-10-24):** the whole pipeline works on the host, the arm64
container passes `xtransd --selftest` under QEMU, and `scripts/build_image.sh`
produces a verified 6 GB SD image. **Nothing has booted on a Pi yet.** Audio,
GPIO, latency against the 1.6 s target and thermals are M9 in
`docs/roadmap.md`. Do not describe the device as working until M9 runs.

## 2. Non-negotiables

1. **TDD only.** No production code before a failing test that demands it.
   Red → green → refactor, one behaviour at a time. A commit must never contain
   a test that fails, and no header/source change is merged without a test that
   would have caught its absence.
2. **C++20, no exceptions, no RTTI.** `-fno-exceptions -fno-rtti` stay on for
   library code. Errors are returned, not thrown.
3. **Dense and low-level by default.** Fixed buffers, compile-time sizes,
   `std::span`, `std::string_view`, `constexpr`, `noexcept`. No allocation, no
   locking, no logging and no `std::string` in the audio or sample path.
4. **Everything works offline.** No telemetry, no network call, no runtime
   dependency fetch, no cloud API. Ever.
5. **Hardware is behind interfaces.** The pipeline must be testable on an
   x86_64 laptop with fakes: no real microphone, GPIO, or model in unit tests.
6. **No new dependency without the owner's approval.** Vendored code is pinned;
   see §9.
7. **No secrets, no models, no audio captures in git.** Models are downloaded
   once during provisioning and live outside the repo.

## 3. Target platform

| Item | Value |
| --- | --- |
| Board | Raspberry Pi 5 (Cortex-A76, 4× 2.4 GHz, NEON, dotprod, fp16) |
| OS | Raspberry Pi OS Lite 64-bit, Debian 13 "trixie" (verified 2026-10-06 image), headless |
| Toolchain | GCC 12+ / Clang 15+ (`aarch64-linux-gnu` for cross builds) |
| Build | CMake ≥ 3.22, Ninja on device |
| Storage | 64 GB microSD, models under `/opt/xtrans/models` |
| Memory | 4 GB minimum; pipeline must stay under ~1.5 GB RSS |
| Power | USB-C PD battery; no active cooling assumptions, thermal-aware |

Do not assume a screen, keyboard, GPU compute, or network on the device.

## 4. Repository layout

```
include/xtrans/    public headers, one subsystem per header
src/               implementation, mirrors include/xtrans/
apps/              daemon binaries, wiring only
tests/             unit tests + minimal harness, no external test framework
cmake/             toolchain files, packaging helpers
docker/            dev/test image, runtime image, compose stack
docker/image/      SD image provisioner, verifier, systemd units
images/            pinned Raspberry Pi OS / Docker / Compose versions
scripts/           shell front-ends, each with a --self-test mode
docs/              design notes that are not code
third_party/       pinned vendored sources (never edited in place)
```

Rules:

- One subsystem, one header + one source. Keep the header self-contained.
- Only expose what tests and the daemon need; everything else stays in an
  anonymous namespace in the `.cpp`.
- `docs/` holds decisions and measurements, not tutorials.

## 5. TDD protocol (strict)

The loop, for every change:

1. **Red.** Add a test in `tests/test_<subsystem>.cpp` that expresses the next
   small behaviour and fails for the right reason. Run `ctest`, confirm it fails.
2. **Green.** Write the smallest implementation that passes. No extra options,
   no speculative generality, no unused parameters.
3. **Refactor.** Remove duplication, tighten types, keep tests green after every
   step.

Test rules:

- **Determinism.** No sleeps, no wall-clock dependencies, no randomness without
  a seeded generator, no real devices, no network. Time and audio come in as
  explicit arguments.
- **One behaviour per case.** A failing test name must say what broke:
  `test_pipeline_release_without_press_is_rejected`, not `test_pipeline`.
- **Boundaries and rejections matter as much as the happy path.** Test empty
  input, overflow, wrap-around, invalid state transitions, and every error
  return.
- **Fakes over mocks.** Implement the interface with a stub that records calls
  and returns scripted results; no mocking framework.
- **Shell scripts** get a test too (`bash -n` plus a `--self-test` mode), or
  they are not worth committing.
- **Coverage.** Core logic in `src/` targets 100 % line and branch coverage of
  its state machines and arithmetic. Run with
  `-DCMAKE_BUILD_TYPE=Coverage -DXTRANS_SANITIZE=address,undefined`.

Commands:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

Green is the only state in which you commit. Commit messages:

```
test: <what the new test pins down>
feat: <behaviour added>
perf: <what got faster, with the measured number>
fix: <bug, and the test that reproduces it>
docs: <what changed>
```

## 6. Performance rules

Budgets on a Pi 5, measured end to end (release of the button → first audio
sample out), for a 3 s utterance:

| Stage | Budget |
| --- | --- |
| Capture + gate close | ≤ 150 ms after last voiced frame |
| Whisper.cpp (base, 4 threads) | ≤ 700 ms |
| NLLB-200 600M (int8) | ≤ 500 ms |
| Piper (medium voice) | ≤ 250 ms |
| Whole turn | ≤ 1.6 s |

- No heap allocation after initialisation on the audio path. Buffers are
  allocated once at startup and passed as `std::span`.
- Capture and playback use a lock-free SPSC ring of `int16_t` at 16 kHz mono.
  Atomics with `relaxed`/`acquire`/`release`; never a mutex in the audio thread.
- Integer arithmetic stays in `int16_t`/`int32_t` unless overflow is possible;
  use `int64_t` for accumulations (RMS).
- Prefer `float` for DSP, keep the hot loop branch-free where reasonable, and
  let NEON work through plain loops before touching intrinsics.
- Never build strings per sample or per frame. Format only at state changes.
- Measure before and after: `perf stat -e cycles,instructions,cache-misses` and
  a `test`-level microbenchmark committed alongside the change.
- Optimisation flags: `-O3 -flto` in release, `-march=armv8.2-a+dotprod+fp16`
  for the device build only, never for the host tests.

## 7. Style

- Google-ish formatting, 100 columns, 2-space indent, `.clang-format` in repo.
- Types `PascalCase`, functions and variables `snake_case`, members `name_`,
  constants `kName`, namespaces `xtrans`.
- Headers: `#pragma once`, include what you use, no transitive includes.
- `noexcept` on everything that cannot fail; `constexpr` wherever possible.
- No `iostream`, no `printf` in library code; logging goes through a thin
  `log` interface so tests stay silent.
- Warnings are errors: `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
  -Werror`. If a warning is genuinely wrong, silence it locally with a comment
  explaining why.
- Comments explain *why* (hardware quirk, model constraint, measured value),
  never *what* the next line does.

## 8. Architecture rules

Three layers, dependencies point inward:

```
apps/xtransd        wiring, config, systemd, buttons loop
   │
core (pipeline)     state machine, selection, orchestration — pure, testable
   │
ports               AudioIn, AudioOut, Stt, Mt, Tts, ButtonSource interfaces
   │
adapters            ALSA, libgpiod, whisper.cpp, ONNX Runtime, Piper
```

- **Core never includes an adapter header.** It talks to port interfaces only.
- Ports return explicit status codes (`enum class Status`), never exceptions and
  never booleans that hide the reason.
- The pipeline state machine is single-threaded and deterministic:
  `press()`, `release()`, `abort()`, `complete(stage, status)`.
- Adapters are thin: they translate bytes and status, nothing else.
- Model loading happens once at startup in `apps/`, not on first utterance.
- **Nothing links ONNX Runtime.** ORT embeds protobuf 3.14, which crashes
  libsentencepiece's descriptor registration against the system protobuf.
  ORT is delay-loaded: `src/adapters/ort_delay_load.cpp` defines
  `OrtGetApiBase()` and `dlopen`s the library with `RTLD_DEEPBIND`; that object
  is linked into **every** executable (`xtrans_ort_loader` OBJECT library).
  The CMake target `onnxruntime::onnxruntime` is interface-only on purpose.
  Breaking this invariant reproduces a startup segfault.
- Vendored code is patched only through `third_party/patches/`, applied by
  `scripts/fetch_third_party.sh`: Piper's version read, its ONNX Runtime 1.22
  API use, the CLI target, and cross-compiler forwarding for espeak-ng and
  open_jtalk.

## 9. External components

| Component | Use | Policy |
| --- | --- | --- |
| whisper.cpp | STT | pinned digest under `third_party/`, built static |
| NLLB-200 Distilled 600M | MT | ONNX int8 via ONNX Runtime, model outside repo |
| Piper | TTS | one ONNX voice per validated language |
| ALSA (`libasound2`) | capture/playback | system package |
| libgpiod | buttons | system package |
| Docker | dev/test + device images | `debian:trixie-slim`, never `latest` |
| Raspberry Pi OS Lite | SD base | pinned in `images/raspios.pin` |
| Docker static + Compose plugin | device runtime | pinned in `images/raspios.pin` |

- Debian packages first: `libonnxruntime-dev`, `libsentencepiece-dev`, and
  friends come from apt, never from a vendor tarball, when the target release
  ships them. Vendor only what Debian does not package (whisper.cpp, Piper).
- Pin versions; record the digest in `third_party/README.md` when adding one.
- No CMake `FetchContent` at build time on the device; provisioning is a
  separate, documented script.
- `XTRANS_OFFLINE=1` must produce a working build from an already-populated
  tree, with no network access.
- Never patched vendored code silently: keep patches in `third_party/patches/`
  and describe them.

## 10. Runtime deployment

- **Shipping path:** an SD image assembled by `scripts/build_image.sh` from a
  pinned Raspberry Pi OS Lite base, the cross-built `xtrans:arm64` container and
  the pinned models. The builder never mounts or loop-mounts anything: it grows
  the partition table, extracts partition 2 with `dd`, runs
  `e2fsck`/`resize2fs`/`debugfs` on it as a file inside a trixie container, and
  writes it back. That is deliberate — trixie images use the `orphan_file` ext4
  feature that older hosts cannot read, and Docker Desktop VMs expose no loop
  partitions. Do not replace this with mount-based provisioning.
- On the device: `containerd` → `docker` (started with `--iptables=false
  --bridge=none`) → `xtrans-firstboot.service` (loads the embedded image once) →
  `xtrans.service` (`docker compose up -d`). The container runs as uid 10001
  with a read-only rootfs, `network_mode: none`, `/dev/snd` and `/dev/gpiochip0`
  passed through, models read-only under `/opt/xtrans/models`. See
  `docs/docker.md`.
- Never ship a container that needs the network to start. The image is
  verified before flashing (14 checks: container tar, models, voices, dockerd,
  compose plugin, units enabled).
- `xtrans-install` is the only sanctioned way to write a card: it refuses
  fixed disks, mounted targets and images that do not fit, and verifies by
  read-back. Never document `dd` as an alternative.
- Buttons come from `/dev/gpiochip*` via the `gpio` group.
- Audio via ALSA `default` device; the config file is generated at provisioning,
  not committed with machine-specific card indices.
- Logs go to the journal with stage timings at `info`, samples and text at
  `debug` only. No utterance content is logged at default level.
- A hung stage must time out and return to `idle`; the device never needs a
  reboot to recover.

## 11. Privacy and safety

- The microphone is only live between press and release. Nothing is buffered
  to disk unless the user explicitly enables capture (`XTRANS_CAPTURE=1`).
- Never commit, print, or log utterance audio, transcripts, or translations.
- No identifiers, no analytics, no clock-based telemetry.

## 12. Definition of done

A change is done when:

- [ ] A test failed first and now passes.
- [ ] `ctest` is green, warnings are errors, sanitizers clean.
- [ ] New logic has a boundary/failure test, not only a happy path.
- [ ] No allocation, lock, or logging was added to a hot path.
- [ ] `docs/` updated if a decision, budget, or validated language changed.
- [ ] If the image path changed: `scripts/build_image.sh` verifies, and the
      README deploy steps still match the scripts.
- [ ] Commit message follows §5, no unrelated changes mixed in.

## 13. Ask before

- Adding any dependency, package, service, or network access.
- Changing the target OS, board, model, or language readiness flags.
- Introducing threads, a new state, or a public API break.
- Deleting or rewriting existing tests to make something pass.
- Anything destructive: force-push, history rewrite, removing models.
