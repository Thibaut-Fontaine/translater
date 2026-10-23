#!/bin/sh
# Provision a Raspberry Pi OS image with the translator stack.
# Runs inside a privileged container: needs losetup, mount, resize2fs.
#
#   provision.sh IMAGE DOCKER_TGZ PAYLOAD_DIR
set -eu

IMAGE=$1
DOCKER_TGZ=$2
PAYLOAD=$3
SIZE=${XTRANS_IMAGE_SIZE:-8G}

echo "== growing the image to $SIZE and partition 2 to the end"
truncate -s "$SIZE" "$IMAGE"
printf ',+\n' | sfdisk --no-reread -N 2 "$IMAGE" >/dev/null

LOOP=$(losetup -f --show "$IMAGE")
# The Docker VM does not expose loop partitions via the kernel; kpartx maps
# them in /dev/mapper instead.
kpartx -as "$LOOP" >/dev/null
BASE=$(basename "$LOOP")
ROOT_DEV=
BOOT_DEV=
cleanup() {
  set +e
  umount /mnt/boot 2>/dev/null
  umount /mnt/root 2>/dev/null
  [ -n "$LOOP" ] && kpartx -d "$LOOP" 2>/dev/null
  [ -n "$LOOP" ] && losetup -d "$LOOP" 2>/dev/null
}
trap cleanup EXIT INT TERM

ROOT_DEV=/dev/mapper/${BASE}p2
BOOT_DEV=/dev/mapper/${BASE}p1
e2fsck -fy "$ROOT_DEV" || true
resize2fs "$ROOT_DEV" >/dev/null

mkdir -p /mnt/root /mnt/boot
mount "$ROOT_DEV" /mnt/root
mount "$BOOT_DEV" /mnt/boot

echo "== payload"
mkdir -p /mnt/root/opt/xtrans/models
cp "$PAYLOAD/compose.yaml" /mnt/root/opt/xtrans/
cp "$PAYLOAD/README.md" /mnt/root/opt/xtrans/
cp -a "$PAYLOAD/models/." /mnt/root/opt/xtrans/models/
cp "$PAYLOAD/xtrans-arm64.tar" /mnt/root/opt/xtrans/

echo "== docker (static arm64, no apt)"
tar -xzf "$DOCKER_TGZ" -C /tmp
cp /tmp/docker/* /mnt/root/usr/local/bin/
chmod 755 /mnt/root/usr/local/bin/docker /mnt/root/usr/local/bin/dockerd \
          /mnt/root/usr/local/bin/containerd /mnt/root/usr/local/bin/runc \
          /mnt/root/usr/local/bin/containerd-shim-runc-v2 2>/dev/null || true

echo "== units"
install -m 644 "$PAYLOAD/containerd.service" /mnt/root/etc/systemd/system/containerd.service
install -m 644 "$PAYLOAD/docker.service" /mnt/root/etc/systemd/system/docker.service
install -m 644 "$PAYLOAD/xtrans.service" /mnt/root/etc/systemd/system/xtrans.service
mkdir -p /mnt/root/etc/systemd/system/multi-user.target.wants
ln -sf /etc/systemd/system/containerd.service \
       /mnt/root/etc/systemd/system/multi-user.target.wants/containerd.service
ln -sf /etc/systemd/system/docker.service \
       /mnt/root/etc/systemd/system/multi-user.target.wants/docker.service
ln -sf /etc/systemd/system/xtrans.service \
       /mnt/root/etc/systemd/system/multi-user.target.wants/xtrans.service

echo "== first boot: load the embedded container image"
cat >"$PAYLOAD/xtrans-firstboot.service" <<'EOF'
[Unit]
Description=Load the translator container image once
After=docker.service
Requires=docker.service
ConditionPathExists=!/var/lib/xtrans/.image-loaded

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/usr/bin/sh -c 'docker load -i /opt/xtrans/xtrans-arm64.tar && mkdir -p /var/lib/xtrans && touch /var/lib/xtrans/.image-loaded'

[Install]
WantedBy=multi-user.target
EOF
install -m 644 "$PAYLOAD/xtrans-firstboot.service" /mnt/root/etc/systemd/system/xtrans-firstboot.service
ln -sf /etc/systemd/system/xtrans-firstboot.service \
       /mnt/root/etc/systemd/system/multi-user.target.wants/xtrans-firstboot.service

sync
echo "== provisioned $(du -sh /mnt/root/opt/xtrans | cut -f1) under /opt/xtrans"
