#!/bin/sh
# Provision a Raspberry Pi OS image without loop devices and without mounting.
#
# Runs inside a Debian trixie container (e2fsprogs >= 1.47, which understands
# the orphan_file feature used by trixie images). Everything is done on files:
# grow the partition table, extract partition 2, e2fsck/resize2fs it, populate
# it with debugfs, write it back.
#
#   provision_nomount.sh IMAGE DOCKER_TGZ PAYLOAD_DIR MODELS_DIR
set -eu

IMAGE=$1
DOCKER_TGZ=$2
PAYLOAD=$3
MODELS=$4
SIZE=${XTRANS_IMAGE_SIZE:-6G}

p2_geometry() {
  sfdisk -d "$IMAGE" | sed -n 's/.*img2 : start= *\([0-9]*\), size= *\([0-9]*\).*/\1 \2/p'
}

echo "== growing image to $SIZE and partition 2 to the end"
truncate -s "$SIZE" "$IMAGE"
printf ',+\n' | sfdisk --no-reread -N 2 "$IMAGE" >/dev/null
set -- $(p2_geometry)
START=$1
OLD_SIZE=$2
TOTAL=$(stat -c %s "$IMAGE")
NEW_SIZE=$(( (TOTAL / 512) - START ))
echo "   p2 start=$START sectors, was $OLD_SIZE, now $NEW_SIZE"

echo "== extracting partition 2 ($OLD_SIZE sectors)"
trap 'rm -f /tmp/p2.img' EXIT INT TERM
rm -f /tmp/p2.img
dd if="$IMAGE" of=/tmp/p2.img bs=512 skip="$START" count="$OLD_SIZE" status=none

echo "== filesystem check and resize"
e2fsck -fy /tmp/p2.img >/dev/null
resize2fs /tmp/p2.img "$(( (NEW_SIZE * 512) / 1024 ))K" >/dev/null

echo "== staging docker binaries"
rm -rf /tmp/docker && mkdir -p /tmp/docker
tar -xzf "$DOCKER_TGZ" -C /tmp/docker

echo "== writing payload with debugfs"
CMDS=/tmp/debugfs.cmds
: >"$CMDS"

emit_files() {
  # write every regular file, then fix the mode
  src=$1; prefix=$2; mode=$3
  ( cd "$src" && find . -type f | sed 's|^\.||' ) | while read -r f; do
    echo "write $src$f $prefix$f" >>"$CMDS"
    echo "sif $prefix$f mode 0100$mode" >>"$CMDS"
  done
}

echo "mkdir /opt" >>"$CMDS"
echo "mkdir /opt/xtrans" >>"$CMDS"
echo "mkdir /opt/xtrans/models" >>"$CMDS"
echo "mkdir /opt/xtrans/models/nllb" >>"$CMDS"
echo "mkdir /opt/xtrans/models/voices" >>"$CMDS"
emit_files "$PAYLOAD" /opt/xtrans 644
emit_files "$MODELS/nllb" /opt/xtrans/models/nllb 644
emit_files "$MODELS/voices" /opt/xtrans/models/voices 644
echo "write $MODELS/ggml-base.bin /opt/xtrans/models/ggml-base.bin" >>"$CMDS"
echo "sif /opt/xtrans/models/ggml-base.bin mode 0100644" >>"$CMDS"

echo "mkdir /usr" >>"$CMDS"
echo "mkdir /usr/local" >>"$CMDS"
echo "mkdir /usr/local/bin" >>"$CMDS"
emit_files /tmp/docker/docker /usr/local/bin 755

# Compose plugin: the static Docker tarball does not ship one, but the unit
# calls `docker compose`.
echo "mkdir /usr/local/lib" >>"$CMDS"
echo "mkdir /usr/local/lib/docker" >>"$CMDS"
echo "mkdir /usr/local/lib/docker/cli-plugins" >>"$CMDS"
echo "write $PAYLOAD/docker-compose /usr/local/lib/docker/cli-plugins/docker-compose" >>"$CMDS"
echo "sif /usr/local/lib/docker/cli-plugins/docker-compose mode 0100755" >>"$CMDS"

echo "mkdir /etc/systemd" >>"$CMDS"
echo "mkdir /etc/systemd/system" >>"$CMDS"
echo "mkdir /etc/systemd/system/multi-user.target.wants" >>"$CMDS"
for unit in xtrans.service docker.service containerd.service xtrans-firstboot.service; do
  echo "write $PAYLOAD/$unit /etc/systemd/system/$unit" >>"$CMDS"
  echo "sif /etc/systemd/system/$unit mode 0100644" >>"$CMDS"
done
echo "symlink /etc/systemd/system/multi-user.target.wants/docker.service /etc/systemd/system/docker.service" >>"$CMDS"
echo "symlink /etc/systemd/system/multi-user.target.wants/containerd.service /etc/systemd/system/containerd.service" >>"$CMDS"
echo "symlink /etc/systemd/system/multi-user.target.wants/xtrans.service /etc/systemd/system/xtrans.service" >>"$CMDS"
echo "symlink /etc/systemd/system/multi-user.target.wants/xtrans-firstboot.service /etc/systemd/system/xtrans-firstboot.service" >>"$CMDS"

debugfs -w -f "$CMDS" /tmp/p2.img >/tmp/debugfs.log 2>&1 || {
  echo "debugfs failed:" >&2
  tail -20 /tmp/debugfs.log >&2
  exit 1
}

echo "== writing partition 2 back"
dd if=/tmp/p2.img of="$IMAGE" bs=512 seek="$START" conv=notrunc status=none
sync
rm -f /tmp/p2.img

echo "== done: /opt/xtrans populated"
