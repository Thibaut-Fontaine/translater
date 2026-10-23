# NLLB ONNX — investigation notes

Status: **blocked on model quality, not on our code.** Date: 2026-10-23.

## What works

- Debian trixie supplies `libonnxruntime-dev` 1.21 and `libsentencepiece-dev`
  0.2; no vendor tarball needed.
- ONNX Runtime's C++ API works on the device-class toolchain, decoder included.
- SentencePiece encodes NLLB text correctly (ids match the reference tokenizer's
  pieces).
- The tokenizer's language ids are known: `eng_Latn` 256047, `fra_Latn` 256057,
  `lao_Laoo` 256100, `tha_Thai` 256175. **`hmn_Latn` does not exist in NLLB** —
  Hmong has no translation path and must be dropped from the MT table.

## Two real problems, one solved

### 1. ONNX Runtime and SentencePiece kill each other (solved)

Linking both into one process segfaults before `main()`, and forcing the link
order aborts with:

```
libprotobuf FATAL: compiled against 3.14.0, installed 3.21.12
```

Cause: ORT statically embeds its own protobuf; libsentencepiece uses the system
`libprotobuf.so.32`. Loading both globally makes one interpose on the other.

Fix in use: **delay-load ORT**. The binary defines `OrtGetApiBase()` itself,
which `dlopen`s `libonnxruntime.so` with `RTLD_LOCAL | RTLD_DEEPBIND` on first
call. SentencePiece keeps its normal shared-protobuf path, ORT keeps its private
copy, and the unmodified ORT C++ API works. Verified: SentencePiece load +
encode and an ORT session in the same process, exit code 0.

Also note: Debian's ORT prints harmless `Schema error ... already registered`
lines to stderr at session creation. They are noise, not failures.

### 2. The ONNX decoders produce degenerate output (open)

With the export from `Xenova/nllb-200-distilled-600M` (revision
`261c31d1…`), a French translation request returns the target-language token
followed by one repeated token forever (`256057, 7249, 7249, …` → "കികിക"). The
same result is reproduced **in Python with onnxruntime alone**, so it is not a
bug in our C++ loop.

Tested and ruled out:

| Variant | Result |
| --- | --- |
| `decoder_model_quantized.onnx` (int8), start `[eos]` + forced target | repeated token |
| `decoder_model.onnx` + `encoder_model.onnx` (fp32, 3.5 GB) | identical garbage |
| `decoder_model_merged_quantized.onnx`, `use_cache_branch=false` | identical garbage |
| start `[pad]` + forced target | copies the first source token, then loops |
| start `[target]` with no forcing | wrong token, then loops |

So the defect is independent of quantization scheme, cache mode, and startup
token. Two candidate explanations remain:

1. the published export is broken for the standard HF convention (encoder
   `src_lang` prefix + `decoder_start_token_id=2` + forced BOS), or
2. the exported encoder output is not the tensor this decoder expects (shape is
   right: `[1, 8, 1024]`, float32, mean |x| ≈ 0.29).

## Options

- **Export ourselves** with `optimum-cli export onnx` from the official weights.
  Needs the gated `facebook/nllb-200-distilled-600M` and roughly 10 GB disk plus
  8+ GB RAM; this workstation has 14 GB free and 6 GB RAM, so it is a build on
  another machine.
- **Cross-check with another export** (e.g. M2M100-418M, same architecture) to
  learn whether the fault is this repository or the convention. Cheap, and it
  also tells us whether a redistributable model family is viable.
- **Change the MT engine**: CTranslate2 has a mature NLLB int8 path, but the
  conversion needs the same gated weights and torch.
- **Change the model**: OPUS-MT fr↔en pairs are small (≈300 MB), CC-BY-4.0,
  and not gated; they would also lift the non-commercial restriction, at the
  cost of one model per language pair. That is an owner decision.

## Artifacts on the workstation

`models/nllb/` (gitignored) holds the int8 encoder (415 MB), the quantized
decoder (471 MB) and `sentencepiece.bpe.model`; the fp32 and merged variants were
deleted after the tests above.
