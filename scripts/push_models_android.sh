#!/usr/bin/env bash
# Push the model files the phone needs into the app's external files directory.
#
#   scripts/push_models_android.sh                 # auto-detect the device
#   scripts/push_models_android.sh --device R58M   # adb serial
#   scripts/push_models_android.sh --self-test     # check the local files only
#
# The app reads /sdcard/Android/data/com.xtrans.app/files/models. Nothing is
# uploaded anywhere; adb copies over USB.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PKG="com.xtrans.app"
DEST="/sdcard/Android/data/${PKG}/files/models"

NLLB_FILES=(
  "encoder_model_int8.onnx"
  "decoder_model_quantized.onnx"
  "decoder_with_past_model_quantized.onnx"
)
WHISPER_PREFERRED="ggml-base-q5_1.bin"
WHISPER_FALLBACK="ggml-base.bin"
BPE="nllb_bpe.bin"

SERIAL=()
SELF_TEST=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --self-test) SELF_TEST=1; shift ;;
    --device) SERIAL=(-s "$2"); shift 2 ;;
    -h|--help) sed -n '2,12p' "$0"; exit 0 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

missing=0
check() {
  local path="$1"
  if [[ -f "$path" ]]; then
    printf '  ok      %-44s %s\n' "$(basename "$path")" "$(du -h "$path" | cut -f1)"
  else
    printf '  MISSING %s\n' "$path"
    missing=$((missing + 1))
  fi
}

echo "models in ${ROOT}/models:"
for name in "${NLLB_FILES[@]}"; do check "$ROOT/models/nllb/$name"; done
check "$ROOT/models/$BPE"
whisper="$ROOT/models/$WHISPER_PREFERRED"
[[ -f "$whisper" ]] || whisper="$ROOT/models/$WHISPER_FALLBACK"
check "$whisper"

if [[ $missing -gt 0 ]]; then
  echo
  echo "run scripts/fetch_models.sh first, then scripts/gen_nllb_bpe.py" >&2
  exit 1
fi

if [[ $SELF_TEST -eq 1 ]]; then
  echo "self-test ok"
  exit 0
fi

command -v adb >/dev/null || { echo "adb not found" >&2; exit 1; }
echo
echo "device:"
adb "${SERIAL[@]}" shell getprop ro.product.model

echo "creating $DEST"
adb "${SERIAL[@]}" shell mkdir -p "$DEST"

for name in "${NLLB_FILES[@]}"; do
  echo "pushing $name"
  adb "${SERIAL[@]}" push "$ROOT/models/nllb/$name" "$DEST/$name"
done
echo "pushing $BPE"
adb "${SERIAL[@]}" push "$ROOT/models/$BPE" "$DEST/$BPE"
echo "pushing $(basename "$whisper")"
adb "${SERIAL[@]}" push "$whisper" "$DEST/$(basename "$whisper")"

echo
echo "done. Open Xtrans, then Models: every line should be green."
