#!/usr/bin/env bash
# Fetch and verify the pinned models into models/.
#
#   scripts/fetch_models.sh              fetch what is missing or corrupt
#   scripts/fetch_models.sh --self-test  validate the pin files, no network
#
# Models are gitignored; the pin files are the source of truth.
# NLLB-200 is CC-BY-NC-4.0: personal use only. Piper voices are CC-BY / MIT
# per voice; see each voice's MODEL_CARD.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

# pin file : destination directory
PINS=(
  "$ROOT/models/nllb.pin:$ROOT/models/nllb"
  "$ROOT/models/voices.pin:$ROOT/models/voices"
)

usage() {
  cat <<'EOF'
usage: scripts/fetch_models.sh [--self-test]

Downloads the files pinned in models/*.pin, verifying each SHA-256.
Existing files with the right digest are kept.
EOF
}

die() {
  echo "fetch_models.sh: $*" >&2
  exit 1
}

pin_value() {
  awk -v key="$1" '$1 == key { print $2; exit }' "$2"
}

check_sha() {
  local file="$1" expected="$2"
  [[ -f "$file" ]] || return 1
  local actual
  actual="$(sha256sum "$file" | awk '{print $1}')"
  [[ "$actual" == "$expected" ]]
}

self_test() {
  local failed=0
  bash -n "${BASH_SOURCE[0]}" || failed=1

  local entry pin dest
  for entry in "${PINS[@]}"; do
    IFS=: read -r pin dest <<<"$entry"
    [[ -f "$pin" ]] || { echo "missing pin: $pin" >&2; failed=1; continue; }

    local repo base revision
    repo="$(pin_value repo "$pin")"
    base="$(pin_value base "$pin")"
    revision="$(pin_value revision "$pin")"
    [[ -n "$repo" ]] || { echo "$(basename "$pin"): missing repo" >&2; failed=1; }
    [[ -n "$base" || "$revision" =~ ^[0-9a-f]{40}$ ]] || {
      echo "$(basename "$pin"): needs a base URL or a full revision" >&2; failed=1; }

    local count=0
    while read -r kind _local _remote sha; do
      [[ "$kind" == "file" ]] || continue
      count=$((count + 1))
      [[ "$sha" =~ ^[0-9a-f]{64}$ ]] || { echo "$(basename "$pin"): bad sha for $_local" >&2; failed=1; }
    done <"$pin"
    [[ "$count" -gt 0 ]] || { echo "$(basename "$pin"): no files" >&2; failed=1; }
  done

  local tmp
  tmp="$(mktemp)"
  printf 'xtrans\n' >"$tmp"
  local expected
  expected="$(sha256sum "$tmp" | awk '{print $1}')"
  check_sha "$tmp" "$expected" || { echo "sha round-trip failed" >&2; failed=1; }
  check_sha "$tmp" "0000" && { echo "sha mismatch not detected" >&2; failed=1; }
  rm -f "$tmp"

  if ((failed == 0)); then echo "self-test: ok"; fi
  return "$failed"
}

fetch_pin() {
  local pin="$1" dest="$2"
  local repo base revision
  repo="$(pin_value repo "$pin")"
  base="$(pin_value base "$pin")"
  revision="$(pin_value revision "$pin")"
  [[ -n "$repo" ]] || die "incomplete pin file: $pin"
  if [[ -z "$base" ]]; then
    [[ -n "$revision" ]] || die "pin needs base or revision: $pin"
    base="https://huggingface.co/${repo}/resolve/${revision}"
  fi
  mkdir -p "$dest"

  local kind local_name remote sha
  while read -r kind local_name remote sha; do
    [[ "$kind" == "file" ]] || continue
    local target="$dest/$local_name"
    if check_sha "$target" "$sha"; then
      echo "ok      $local_name"
      continue
    fi
    echo "fetch   $local_name"
    curl -fsSL --retry 3 -o "$target.tmp" "$base/$remote"
    echo "$sha  $target.tmp" | sha256sum -c - >/dev/null || die "digest mismatch: $local_name"
    mv "$target.tmp" "$target"
  done <"$pin"
}

case "${1:-}" in
  "")
    for entry in "${PINS[@]}"; do
      IFS=: read -r pin dest <<<"$entry"
      fetch_pin "$pin" "$dest"
    done
    # The phone cannot use SentencePiece; this exports the same tokenizer as a
    # compact blob for src/bpe.cpp. Harmless for the Pi build.
    if [[ -f models/nllb/tokenizer.json ]] && command -v python3 >/dev/null; then
      python3 scripts/gen_nllb_bpe.py
    fi
    ;;
  --self-test) self_test; exit $? ;;
  -h | --help) usage; exit 0 ;;
  *) usage >&2; exit 2 ;;
esac
