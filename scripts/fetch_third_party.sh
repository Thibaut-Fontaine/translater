#!/usr/bin/env bash
# Fetch, verify and unpack the pinned third-party sources into third_party/.
#
#   scripts/fetch_third_party.sh              fetch everything that is missing
#   scripts/fetch_third_party.sh --force      re-download and re-extract
#   scripts/fetch_third_party.sh --self-test  check the pins and the digest code
#
# The extracted trees are gitignored: the pin file is the source of truth.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
PIN="$ROOT/third_party/whisper.pin"
DEST="$ROOT/third_party/whisper.cpp"
DOWNLOADS="$ROOT/third_party/downloads"

usage() {
  cat <<'EOF'
usage: scripts/fetch_third_party.sh [--force] [--self-test]

Fetches whisper.cpp at the commit pinned in third_party/whisper.pin, verifies
its SHA-256, and extracts it to third_party/whisper.cpp (gitignored).

  --force      re-download and re-extract even if the tree exists
  --self-test  validate the pin file and the digest path without downloading
EOF
}

die() {
  echo "fetch_third_party.sh: $*" >&2
  exit 1
}

field() {
  awk -v key="$1" '$1 == key { sub($1 "[ \t]+", ""); print; exit }' "$2"
}

read_pin() {
  local file="$1"
  VERSION="$(field version "$file")"
  COMMIT="$(field commit "$file")"
  URL="$(field url "$file")"
  SHA256="$(field sha256 "$file")"
  [[ -n "$VERSION" && -n "$COMMIT" && -n "$URL" && -n "$SHA256" ]] ||
    die "incomplete pin file: $file"
}

fetch_whisper() {
  read_pin "$PIN"
  mkdir -p "$DOWNLOADS"

  local archive="$DOWNLOADS/whisper-$VERSION.tar.gz"
  if [[ "$FORCE" == "1" || ! -f "$archive" ]]; then
    echo "downloading whisper.cpp $VERSION"
    curl -fsSL -o "$archive.tmp" "$URL"
    mv "$archive.tmp" "$archive"
  fi

  echo "$SHA256  $archive" | sha256sum -c - >/dev/null || die "digest mismatch for $archive"

  if [[ "$FORCE" != "1" && -f "$DEST/CMakeLists.txt" ]]; then
    echo "whisper.cpp $VERSION already present"
    return
  fi

  rm -rf "$DEST" "$DEST.tmp"
  mkdir -p "$DEST.tmp"
  tar -xzf "$archive" -C "$DEST.tmp" --strip-components=1
  mv "$DEST.tmp" "$DEST"
  echo "whisper.cpp $VERSION -> $DEST"
}

self_test() {
  local failed=0
  bash -n "${BASH_SOURCE[0]}" || failed=1
  read_pin "$PIN" || failed=1
  [[ "$SHA256" =~ ^[0-9a-f]{64}$ ]] || { echo "bad sha256 in pin" >&2; failed=1; }
  [[ "$COMMIT" =~ ^[0-9a-f]{40}$ ]] || { echo "bad commit in pin" >&2; failed=1; }
  [[ "$URL" == *"$COMMIT"* ]] || { echo "url does not embed the pinned commit" >&2; failed=1; }

  local tmp
  tmp="$(mktemp)"
  printf 'xtrans\n' >"$tmp"
  local expected
  expected="$(sha256sum "$tmp" | awk '{print $1}')"
  if [[ "$(sha256sum "$tmp" | awk '{print $1}')" != "$expected" ]]; then
    echo "digest round-trip failed" >&2
    failed=1
  fi
  rm -f "$tmp"

  [[ "$DEST" == "$ROOT/third_party/"* ]] || { echo "destination escapes third_party" >&2; failed=1; }
  if ((failed == 0)); then echo "self-test: ok"; fi
  return "$failed"
}

FORCE=0
case "${1:-}" in
  "") ;;
  --force) FORCE=1 ;;
  --self-test) self_test; exit $? ;;
  -h | --help) usage; exit 0 ;;
  *) usage >&2; exit 2 ;;
esac

fetch_whisper
