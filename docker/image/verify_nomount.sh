#!/bin/sh
# Read-only verification of a provisioned image, without mounting it.
# Runs inside a Debian trixie container; works on the partition as a file.
#
#   verify_nomount.sh IMAGE
set -eu

IMAGE=$1
START=$(sfdisk -d "$IMAGE" | sed -n 's/.*img2 : start= *\([0-9]*\), size= *\([0-9]*\).*/\1 \2/p' | cut -d' ' -f1)
SIZE=$(sfdisk -d "$IMAGE" | sed -n 's/.*img2 : start= *\([0-9]*\), size= *\([0-9]*\).*/\1 \2/p' | cut -d' ' -f2)

rm -f /tmp/p2.img
dd if="$IMAGE" of=/tmp/p2.img bs=512 skip="$START" count="$SIZE" status=none

fail=0
check() {
  path=$1
  label=$2
  if debugfs -R "stat $path" /tmp/p2.img 2>/dev/null | grep -q "Inode:"; then
    echo "ok      $label"
  else
    echo "MISSING $label"
    fail=1
  fi
}

check /opt/xtrans/compose.yaml "compose.yaml"
check /opt/xtrans/xtrans-arm64.tar "container image tar"
check /opt/xtrans/models/ggml-base.bin "whisper model"
check /opt/xtrans/models/nllb/sentencepiece.bpe.model "nllb tokenizer"
check /opt/xtrans/models/nllb/encoder_model_int8.onnx "nllb encoder"
check /opt/xtrans/models/voices/fr_FR-siwis-medium.onnx "french voice"
check /opt/xtrans/models/voices/en_US-lessac-medium.onnx "english voice"
check /usr/local/bin/dockerd "dockerd"
check /usr/local/lib/docker/cli-plugins/docker-compose "compose plugin"
check /etc/systemd/system/xtrans.service "xtrans unit"
check /etc/systemd/system/docker.service "docker unit"
check /etc/systemd/system/multi-user.target.wants/docker.service "docker enabled"
check /etc/systemd/system/multi-user.target.wants/xtrans.service "xtrans enabled"
check /etc/systemd/system/multi-user.target.wants/xtrans-firstboot.service "firstboot enabled"

echo "== payload size:"
debugfs -R "stat /opt/xtrans" /tmp/p2.img 2>/dev/null | grep -E "Inode:|Size:" | head -2 || true

rm -f /tmp/p2.img
exit "$fail"
