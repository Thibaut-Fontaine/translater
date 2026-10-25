#!/usr/bin/env bash
# Fetch the ONNX Runtime Android package and unpack the arm64 pieces the app
# links against. Run once per checkout; the result is gitignored.
#
#   scripts/fetch_ort_android.sh
#   scripts/fetch_ort_android.sh --self-test
#
# Why an AAR and not the Debian package: Android uses bionic, so the arm64
# library from the Pi image cannot be linked. The AAR ships the C++ headers and
# the full (not reduced) runtime, which is what an NLLB export needs.
set -euo pipefail

VERSION="1.21.0"
SHA256="8b675e9680b8cc02dca706a5e3b4e35cc8506de5bdf206fdae68081cbd804414"
URL="https://repo1.maven.org/maven2/com/microsoft/onnxruntime/onnxruntime-android/${VERSION}/onnxruntime-android-${VERSION}.aar"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CACHE="$ROOT/third_party/downloads"
AAR="$CACHE/onnxruntime-android-${VERSION}.aar"
CPP="$ROOT/mobile/android/app/src/main/cpp/ort"
LIBS="$ROOT/mobile/android/app/src/main/jniLibs"
ABI="${ORT_ABI:-arm64-v8a}"

self_test() {
  printf 'version %s\nsha256  %s\nurl     %s\nabi     %s\n' "$VERSION" "$SHA256" "$URL" "$ABI"
  command -v curl >/dev/null || { echo "curl missing" >&2; return 1; }
  command -v unzip >/dev/null || { echo "unzip missing" >&2; return 1; }
  echo "self-test ok"
}

if [[ "${1:-}" == "--self-test" ]]; then
  self_test
  exit 0
fi

mkdir -p "$CACHE" "$CPP/include/onnxruntime" "$LIBS/$ABI"

if [[ ! -f "$AAR" ]]; then
  echo "downloading onnxruntime-android ${VERSION}"
  curl -fL --retry 3 -o "$AAR.part" "$URL"
  mv "$AAR.part" "$AAR"
fi

actual="$(sha256sum "$AAR" | cut -d' ' -f1)"
if [[ "$actual" != "$SHA256" ]]; then
  echo "checksum mismatch for $AAR" >&2
  echo "  expected $SHA256" >&2
  echo "  actual   $actual" >&2
  exit 1
fi

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
unzip -q "$AAR" 'headers/*' "jni/$ABI/libonnxruntime.so" -d "$tmp"

cp "$tmp"/headers/*.h "$CPP/include/onnxruntime/"
cp "$tmp/jni/$ABI/libonnxruntime.so" "$LIBS/$ABI/libonnxruntime.so"
printf '%s\n' "$VERSION" > "$CPP/VERSION"

echo "headers -> ${CPP#"$ROOT"/}/include/onnxruntime"
echo "library -> ${LIBS#"$ROOT"/}/$ABI/libonnxruntime.so"
du -h "$LIBS/$ABI/libonnxruntime.so" | cut -f1
