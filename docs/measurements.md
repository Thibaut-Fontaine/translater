# Measurements

Numbers taken on real hardware, with the command that produced them. Host
numbers are only a sanity check; budgets are decided on the Pi 5.

## STT — whisper.cpp

Date: 2026-10-23. Build: v1.9.5, `GGML_NATIVE=OFF`, 4 threads, static libs,
`-O3` (Release). Host: x86_64 VM, 8 vCPU (old server part), Ubuntu 22.04,
GCC 11.4. Audio: `whisper.cpp/samples/jfk.wav` (16 kHz mono, 11 s) and its first
3 s.

| Model | Audio | Wall time | Transcript |
| --- | --- | --- | --- |
| `ggml-base.bin` (f16, 142 MB) | 11 s | 21.9 s | exact JFK sentence |
| `ggml-base.bin` | 3 s | 21.2 s | "And so my fellow Americans" |
| `ggml-base-q5_1.bin` (57 MB) | 3 s | 55.2 s | "And so my fellow Americans!" |

Reading:

- Cost is dominated by the fixed 30 s window `whisper_full()` always processes,
  so a 3 s utterance costs the same as an 11 s one. Shorter utterances do not
  buy latency with this API.
- This host runs at roughly 0.7× real time for a 30 s window, which says
  nothing useful about a Cortex-A76 at 2.4 GHz with NEON/dotprod. A Pi 5
  measurement is required before the 700 ms budget in `AGENTS.md` §6 can be
  kept, adjusted, or replaced by a smaller model.
- q5_1 being 2.6× slower here is a host artefact (no optimized quantized dot
  product on this CPU). Do not generalize it; measure on the Pi.

## MT — NLLB-200 int8 encoder/decoder pair

Date: 2026-10-23. Host as above, ONNX Runtime 1.21 (Debian), 4 intra-op
threads, KV cache with `decoder_model_quantized` + `decoder_with_past_model_quantized`.

| Input | Decoder steps | Wall time | Output |
| --- | --- | --- | --- |
| "Hi" (en→fr) | 6 | 4.4 s | "Salut" |
| "Hello, how are you today?" (en→fr) | 12 | 6.6 s | "Bonjour, comment allez-vous aujourd'hui ?" |
| "Bonjour, comment allez-vous aujourd hui ?" (fr→en) | 8 | 4.9 s | "Hi, how are you today?" |

Cost is ~550–730 ms per decoder step, so it scales with output length, not
input length. This host has no VNNI; the Pi 5 with NEON dotprod may differ in
either direction. If the Pi confirms this order of magnitude, the MT stage
needs a different runtime (CTranslate2) or the latency budget needs revisiting.

Pi 5 numbers go here as soon as the device can run `xtransd --device --model`.
