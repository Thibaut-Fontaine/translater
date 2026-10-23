#!/usr/bin/env bash
# Docker front-end for the portable translator.
#
#   ./scripts/docker.sh test        build the dev image and run the test suite
#   ./scripts/docker.sh smoke       run xtransd in the dev image with scripted input
#   ./scripts/docker.sh image       cross-build the arm64 runtime image (docker tar)
#   ./scripts/docker.sh run         start the runtime stack with docker compose
#   ./scripts/docker.sh --help
#
# Environment:
#   XTRANS_DEV_IMAGE  dev/test tag              (default: xtrans-dev)
#   XTRANS_IMAGE      runtime tag               (default: xtrans:arm64)
#   XTRANS_IMAGE_OUT  exported runtime tarball  (default: build/xtrans-arm64.tar)
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
DEV_IMAGE="${XTRANS_DEV_IMAGE:-xtrans-dev}"
IMAGE="${XTRANS_IMAGE:-xtrans:arm64}"
OUT="${XTRANS_IMAGE_OUT:-$ROOT/build/xtrans-arm64.tar}"

usage() {
  cat <<'EOF'
usage: scripts/docker.sh <command>

commands:
  test    build the dev image (tests run during the build) and re-run ctest
  smoke   run xtransd in the dev image against a scripted stdin sequence
  image   cross-build the linux/arm64 runtime image to a docker-loadable tar
  run     start the runtime stack from docker/compose.yaml on the device

environment:
  XTRANS_DEV_IMAGE  dev/test image tag        (default: xtrans-dev)
  XTRANS_IMAGE      runtime image tag         (default: xtrans:arm64)
  XTRANS_IMAGE_OUT  exported runtime tarball  (default: build/xtrans-arm64.tar)
EOF
}

die() {
  echo "docker.sh: $*" >&2
  exit 1
}

require_docker() {
  command -v docker >/dev/null 2>&1 || die "docker is not installed or not on PATH"
  docker info >/dev/null 2>&1 || die "cannot reach the docker daemon (permission or service)"
}

cmd_test() {
  require_docker
  docker build -f "$ROOT/docker/Dockerfile.dev" -t "$DEV_IMAGE" "$ROOT"
  docker run --rm "$DEV_IMAGE"
}

cmd_smoke() {
  require_docker
  docker run --rm -i "$DEV_IMAGE" sh -c '
    set -e
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug >/dev/null
    cmake --build build -j"$(nproc)" >/dev/null
    exec ./build/xtransd' <<'EOF'
press
release
swap
q
EOF
}

cmd_image() {
  require_docker
  mkdir -p "$(dirname -- "$OUT")"
  docker buildx build \
    --platform linux/arm64 \
    -f "$ROOT/docker/Dockerfile" \
    -t "$IMAGE" \
    --output "type=docker,dest=$OUT" \
    "$ROOT"
  echo "wrote $OUT"
}

cmd_run() {
  require_docker
  docker compose -f "$ROOT/docker/compose.yaml" up -d
}

# Checks that do not need a working daemon: syntax, usage, and the invariants
# the compose file is supposed to guarantee.
self_test() {
  local failed=0
  bash -n "${BASH_SOURCE[0]}" || failed=1
  for f in docker/Dockerfile docker/Dockerfile.dev docker/compose.yaml docs/docker.md; do
    [[ -f "$ROOT/$f" ]] || { echo "missing $f" >&2; failed=1; }
  done
  "$0" --help | grep -q 'smoke' || { echo "help lacks smoke" >&2; failed=1; }
  grep -q 'network_mode: none' "$ROOT/docker/compose.yaml" || {
    echo "compose must stay offline" >&2
    failed=1
  }
  grep -q 'read_only: true' "$ROOT/docker/compose.yaml" || {
    echo "compose must keep the rootfs read-only" >&2
    failed=1
  }
  grep -q '^USER xtrans' "$ROOT/docker/Dockerfile" || {
    echo "runtime image must not run as root" >&2
    failed=1
  }
  if ((failed == 0)); then echo "self-test: ok"; fi
  return "$failed"
}

case "${1:-}" in
  test) cmd_test ;;
  smoke) cmd_smoke ;;
  image) cmd_image ;;
  run) cmd_run ;;
  --self-test) self_test ;;
  -h | --help | help | "") usage ;;
  *) usage >&2; exit 2 ;;
esac
