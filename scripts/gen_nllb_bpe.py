#!/usr/bin/env python3
"""Turn the NLLB tokenizer into a compact blob the phone can read.

    python3 scripts/gen_nllb_bpe.py --tokenizer models/nllb/tokenizer.json \
        --out models/nllb_bpe.bin [--verify]

Why not SentencePiece
  libsentencepiece needs protobuf and is not packaged for Android; building it
  for the NDK means cross-compiling protoc. The NLLB tokenizer is a plain
  byte-pair encoding over a fixed vocabulary, so the data is exported once and
  encoded by src/bpe.cpp. Normalisation (NFKC) is done by the platform, which
  has a real Unicode implementation: java.text.Normalizer on Android,
  NSString on iOS.

Blob layout (little endian)
  "XBPE1"  u32 version(=1)  u32 unk_id
  u32 vocab_count           vocab_count * {u32 id, u32 len, bytes}
  u32 merge_count           merge_count * {u32 left, u32 right, u32 result, u32 rank}

The two arrays are sorted at generation time: vocab by bytes, merges by
(left << 32) | right, so the encoder can binary-search without a hash map.
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

MAGIC = b"XBPE1"
VERSION = 1


def build(tokenizer_path: Path):
    data = json.loads(tokenizer_path.read_text(encoding="utf-8"))
    model = data["model"]
    if model["type"] != "BPE":
        raise SystemExit(f"expected a BPE model, got {model['type']}")

    vocab = {piece: int(i) for piece, i in model["vocab"].items()}
    unk = vocab[model["unk_token"]]

    entries = sorted((piece.encode("utf-8"), i) for piece, i in vocab.items())
    merges = []
    for rank, merge in enumerate(model["merges"]):
        left, right = merge.split(" ")
        left_id = vocab.get(left)
        right_id = vocab.get(right)
        # A merge is only usable when both sides and the result are in the
        # vocabulary; HF drops the others too.
        result = vocab.get(left + right)
        if left_id is None or right_id is None or result is None:
            continue
        merges.append((left_id, right_id, result, rank))
    merges.sort(key=lambda m: (m[0], m[1]))
    return unk, entries, merges


def write(path: Path, unk: int, entries, merges) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as f:
        f.write(MAGIC)
        f.write(struct.pack("<III", VERSION, unk, len(entries)))
        for piece, i in entries:
            f.write(struct.pack("<II", i, len(piece)))
            f.write(piece)
        f.write(struct.pack("<I", len(merges)))
        for left, right, result, rank in merges:
            f.write(struct.pack("<IIII", left, right, result, rank))
    print(f"{path}: {len(entries)} pieces, {len(merges)} merges, {path.stat().st_size} bytes")


def verify(tokenizer_path: Path, samples) -> int:
    try:
        from tokenizers import Tokenizer
    except ImportError:
        print("tokenizers not installed; skipping verification", file=sys.stderr)
        return 0
    tok = Tokenizer.from_file(str(tokenizer_path))
    failures = 0
    for text in samples:
        ids = tok.encode(text).ids
        print(f"  {text!r} -> {ids}")
    return failures


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--tokenizer", default="models/nllb/tokenizer.json")
    ap.add_argument("--out", default="models/nllb_bpe.bin")
    ap.add_argument("--verify", action="store_true")
    args = ap.parse_args()

    tokenizer_path = Path(args.tokenizer)
    if not tokenizer_path.exists():
        print(f"missing {tokenizer_path}; run scripts/fetch_models.sh first", file=sys.stderr)
        return 1

    unk, entries, merges = build(tokenizer_path)
    write(Path(args.out), unk, entries, merges)
    if args.verify:
        verify(tokenizer_path, [
            "Bonjour le monde.",
            "The quick brown fox.",
            "Ceci est un test.",
            "Xin chào thế giới",
            "สวัสดีชาวโลก",
        ])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
