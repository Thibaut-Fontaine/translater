#!/usr/bin/env bash
# Fetch and verify the pinned translation models into models/nllb/.
#
#   scripts/fetch_models.sh              fetch what is missing or corrupt
#   scripts/fetch_models.sh --self-test  validate the pin file, no network
#
# Models are gitignored; the pin file is the source of truth. NLLB-200 is
# CC-BY-NC-4.0: personal use only.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
PIN="$ROOT/models/nllb.pin"
DEST="$ROOT/models/nllb"

usage() {
  cat <<'EOF'
usage: scripts/fetch_models.sh [--self-test]

Downloads the ONNX files pinned in models/nllb.pin into models/nllb/,
verifying each SHA-256. Existing files with the right digest are kept.
EOF
}

die() {
  echo "fetch_models.sh: $*" >&2
  exit 1
}

pin_value() {
  awk -v key="$1" '$1 == key { print $2; exit }' "$PIN"
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

  local repo revision
  repo="$(pin_value repo)"
  revision="$(pin_value revision)"
  [[ -n "$repo" ]] || { echo "pin: missing repo" >&2; failed=1; }
  [[ "$revision" =~ ^[0-9a-f]{40}$ ]] || { echo "pin: bad revision" >&2; failed=1; }

  local count=0
  while read -r kind _local _remote sha; do
    [[ "$kind" == "file" ]] || continue
    count=$((count + 1))
    [[ "$sha" =~ ^[0-9a-f]{64}$ ]] || { echo "pin: bad sha for $_local" >&2; failed=1; }
  done <"$PIN"
  [[ "$count" -eq 4 ]] || { echo "pin: expected 4 files, got $count" >&2; failed=1; }

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

fetch() {
  local repo revision
  repo="$(pin_value repo)"
  revision="$(pin_value revision)"
  [[ -n "$repo" && -n "$revision" ]] || die "incomplete pin file"
  mkdir -p "$DEST"

  local base="https://huggingface.co/${repo}/resolve/${revision}"
  local local_name remote sha
  while read -r kind local_name remote sha; do
    [[ "$kind" == "file" ]] || continue
    local target="$DEST/$local_name"
    if check_sha "$target" "$sha"; then
      echo "ok      $local_name"
      continue
    fi
    echo "fetch   $local_name"
    curl -fsSL -o "$target.tmp" "$base/$remote"
    echo "$sha  $target.tmp" | sha256sum -c - >/dev/null || die "digest mismatch: $local_name"
    mv "$target.tmp" "$target"
  done <"$PIN"
}

case "${1:-}" in
  "") fetch ;;
  --self-test) self_test; exit $? ;;
  -h | --help) usage; exit 0 ;;
  *) usage >&2; exit 2 ;;
esac
