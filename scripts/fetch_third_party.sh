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
DOWNLOADS="$ROOT/third_party/downloads"

# name:pin-file:destination
PINS=(
  "whisper:$ROOT/third_party/whisper.pin:$ROOT/third_party/whisper.cpp"
  "piper:$ROOT/third_party/piper.pin:$ROOT/third_party/piper"
)

usage() {
  cat <<'EOF'
usage: scripts/fetch_third_party.sh [--force] [--self-test]

Fetches whisper.cpp and Piper at the commits pinned in third_party/*.pin,
verifies their SHA-256, and extracts them (gitignored).

  --force      re-download and re-extract even if the trees exist
  --self-test  validate the pin files and the digest path without downloading
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

apply_patches() {
  local name="$1" dest="$2" patch_file
  for patch_file in "$ROOT/third_party/patches/$name"-*.patch; do
    [[ -f "$patch_file" ]] || continue
    echo "patch   $name: $(basename "$patch_file")"
    patch -p1 -d "$dest" <"$patch_file" || die "patch failed: $patch_file"
  done
}

fetch_pin() {
  local name="$1" pin="$2" dest="$3"
  read_pin "$pin"
  mkdir -p "$DOWNLOADS"

  local archive="$DOWNLOADS/$name-$VERSION.tar.gz"
  if [[ "$FORCE" == "1" || ! -f "$archive" ]]; then
    echo "downloading $name $VERSION"
    curl -fsSL -o "$archive.tmp" "$URL"
    mv "$archive.tmp" "$archive"
  fi

  echo "$SHA256  $archive" | sha256sum -c - >/dev/null || die "digest mismatch for $archive"

  if [[ "$FORCE" != "1" && -f "$dest/CMakeLists.txt" ]]; then
    echo "$name $VERSION already present"
    return
  fi

  rm -rf "$dest" "$dest.tmp"
  mkdir -p "$dest.tmp"
  tar -xzf "$archive" -C "$dest.tmp" --strip-components=1
  mv "$dest.tmp" "$dest"
  apply_patches "$name" "$dest"
  echo "$name $VERSION -> $dest"
}

self_test() {
  local failed=0
  bash -n "${BASH_SOURCE[0]}" || failed=1

  local entry name pin dest
  for entry in "${PINS[@]}"; do
    IFS=: read -r name pin dest <<<"$entry"
    read_pin "$pin" || failed=1
    [[ "$SHA256" =~ ^[0-9a-f]{64}$ ]] || { echo "$name: bad sha256" >&2; failed=1; }
    [[ "$COMMIT" =~ ^[0-9a-f]{40}$ ]] || { echo "$name: bad commit" >&2; failed=1; }
    [[ "$URL" == *"$COMMIT"* ]] || { echo "$name: url lacks commit" >&2; failed=1; }
    [[ "$dest" == "$ROOT/third_party/"* ]] || { echo "$name: destination escapes" >&2; failed=1; }
  done

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

for entry in "${PINS[@]}"; do
  IFS=: read -r name pin dest <<<"$entry"
  fetch_pin "$name" "$pin" "$dest"
done
