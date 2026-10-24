#!/bin/sh
# Read-only verification of a provisioned image. Runs privileged.
set -eu
IMAGE=$1
LOOP=$(losetup -fP --show "$IMAGE")
sleep 1
if [ -b "${LOOP}p2" ]; then
  DEV=${LOOP}p2
else
  kpartx -as "$LOOP" >/dev/null
  DEV=/dev/mapper/$(basename "$LOOP")p2
fi
cleanup() { set +e; umount /mnt/root 2>/dev/null; kpartx -d "$LOOP" 2>/dev/null; losetup -d "$LOOP" 2>/dev/null; }
trap cleanup EXIT INT TERM
mkdir -p /mnt/root
mount -o ro "$DEV" /mnt/root

fail=0
check() { if [ -e "$1" ]; then echo "ok      $2"; else echo "MISSING $2"; fail=1; fi; }
check /mnt/root/opt/xtrans/compose.yaml "compose.yaml"
check /mnt/root/opt/xtrans/xtrans-arm64.tar "container image tar"
check /mnt/root/opt/xtrans/models/ggml-base.bin "whisper model"
check /mnt/root/opt/xtrans/models/nllb/sentencepiece.bpe.model "nllb tokenizer"
check /mnt/root/opt/xtrans/models/voices/fr_FR-siwis-medium.onnx "french voice"
check /mnt/root/usr/local/bin/dockerd "dockerd"
check /mnt/root/etc/systemd/system/multi-user.target.wants/docker.service "docker enabled"
check /mnt/root/etc/systemd/system/multi-user.target.wants/xtrans.service "xtrans enabled"
check /mnt/root/etc/systemd/system/multi-user.target.wants/xtrans-firstboot.service "firstboot enabled"

echo "== /opt/xtrans:"
du -sh /mnt/root/opt/xtrans 2>/dev/null || true
echo "== free space:"
df -h /mnt/root | tail -1
exit "$fail"
