#!/usr/bin/env bash
# Build a flashable Raspberry Pi SD image containing the translator.
#
#   scripts/build_image.sh              download base, build, provision, verify
#   scripts/build_image.sh --keep       keep the working image after success
#   scripts/build_image.sh --verify-only  re-run verification on the last image
#   scripts/build_image.sh --self-test  check pins and tools without network
#
# Requires Docker with privileged containers (losetup/mount inside the VM).
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
PIN="$ROOT/images/raspios.pin"
WORK="$ROOT/build/image"
BASE_XZ="$WORK/base.img.xz"
IMAGE="$WORK/xtrans.img"
PAYLOAD="$WORK/payload"
CONTAINER_IMAGE="${XTRANS_CONTAINER_IMAGE:-xtrans:arm64}"

pin() { awk -v k="$1" '$1 == k { print $2; exit }' "$PIN"; }

usage() {
  cat <<'EOF'
usage: scripts/build_image.sh [--keep] [--verify-only] [--self-test]

Builds build/image/xtrans.img from the pinned Raspberry Pi OS base, the
container image xtrans:arm64 and models/ (NLLB, whisper, Piper voices).

  --keep         keep the working image (a flashable copy is always kept)
  --verify-only  verify the existing image, do not rebuild
  --self-test    validate pins and required files without touching the network
EOF
}

self_test() {
  local failed=0
  bash -n "${BASH_SOURCE[0]}" || failed=1
  [[ -f "$PIN" ]] || { echo "missing $PIN" >&2; failed=1; }

  local url sha
  url="$(pin base_url)"; sha="$(pin base_sha256)"
  [[ -n "$url" && "$sha" =~ ^[0-9a-f]{64}$ ]] || { echo "bad base pin" >&2; failed=1; }
  [[ "$(pin docker_sha256)" =~ ^[0-9a-f]{64}$ ]] || { echo "bad docker pin" >&2; failed=1; }

  for f in docker/image/provision.sh docker/image/verify.sh docker/compose.yaml \
           docker/xtrans.service docker/image/docker.service docker/image/containerd.service; do
    [[ -f "$ROOT/$f" ]] || { echo "missing $f" >&2; failed=1; }
  done
  for d in models/nllb models/voices models/ggml-base.bin; do
    [[ -e "$ROOT/$d" ]] || { echo "missing $d (run scripts/fetch_models.sh)" >&2; failed=1; }
  done
  if ((failed == 0)); then echo "self-test: ok"; fi
  return "$failed"
}

prepare_base() {
  mkdir -p "$WORK"
  local url sha
  url="$(pin base_url)"
  sha="$(pin base_sha256)"
  if [[ ! -f "$BASE_XZ" ]]; then
    echo "downloading Raspberry Pi OS base"
    curl -fL --retry 3 -o "$BASE_XZ.tmp" "$url"
    mv "$BASE_XZ.tmp" "$BASE_XZ"
  fi
  echo "$sha  $BASE_XZ" | sha256sum -c - >/dev/null
  rm -f "$IMAGE"
  echo "decompressing base"
  xz -dc "$BASE_XZ" >"$IMAGE"
}

prepare_payload() {
  rm -rf "$PAYLOAD"
  mkdir -p "$PAYLOAD/models"
  docker save "$CONTAINER_IMAGE" -o "$PAYLOAD/xtrans-arm64.tar"
  cp -a "$ROOT/docker/compose.yaml" "$PAYLOAD/"
  cp -a "$ROOT/docker/xtrans.service" "$PAYLOAD/"
  cp -a "$ROOT/docker/image/docker.service" "$PAYLOAD/"
  cp -a "$ROOT/docker/image/containerd.service" "$PAYLOAD/"
  cp -a "$ROOT/README.md" "$PAYLOAD/"
  cp -a "$ROOT/models/." "$PAYLOAD/models/"
  local url sha
  url="$(pin docker_url)"
  sha="$(pin docker_sha256)"
  if [[ ! -f "$WORK/docker.tgz" ]]; then
    curl -fL --retry 3 -o "$WORK/docker.tgz" "$url"
  fi
  echo "$sha  $WORK/docker.tgz" | sha256sum -c - >/dev/null
}

run_privileged() {
  docker run --rm --privileged \
    -v "$ROOT:/work" -w /work \
    debian:trixie-slim sh -c '
      set -e
      apt-get update -qq >/dev/null 2>&1
      apt-get install -y -qq util-linux e2fsprogs fdisk dosfstools kpartx >/dev/null 2>&1
      sh /work/docker/image/provision.sh /work/build/image/xtrans.img \
         /work/build/image/docker.tgz /work/build/image/payload'
}

verify() {
  docker run --rm --privileged \
    -v "$ROOT:/work" -w /work \
    debian:trixie-slim sh -c '
      set -e
      apt-get update -qq >/dev/null 2>&1
      apt-get install -y -qq util-linux e2fsprogs kpartx >/dev/null 2>&1
      sh /work/docker/image/verify.sh /work/build/image/xtrans.img'
}

case "${1:-}" in
  --self-test) self_test; exit $? ;;
  --verify-only) verify; exit $? ;;
  --keep) ;;
  "") ;;
  -h | --help) usage; exit 0 ;;
  *) usage >&2; exit 2 ;;
esac

self_test >/dev/null || { self_test; exit 1; }
prepare_base
prepare_payload
run_privileged
verify
echo "image ready: $IMAGE"
