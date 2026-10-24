#!/bin/sh
# Add the Docker Compose CLI plugin to an already-provisioned image.
# Used to patch an existing image and by the provisioner for fresh builds.
#
#   add_compose.sh IMAGE COMPOSE_BINARY
set -eu

IMAGE=$1
BIN=$2

GEOM=$(sfdisk -d "$IMAGE" | sed -n 's/.*img2 : start= *\([0-9]*\), size= *\([0-9]*\).*/\1 \2/p')
START=${GEOM%% *}
SIZE=${GEOM##* }

trap 'rm -f /tmp/p2-compose.img' EXIT INT TERM
dd if="$IMAGE" of=/tmp/p2-compose.img bs=512 skip="$START" count="$SIZE" status=none

cat >/tmp/compose.cmds <<EOF
mkdir /usr/local/lib
mkdir /usr/local/lib/docker
mkdir /usr/local/lib/docker/cli-plugins
write $BIN /usr/local/lib/docker/cli-plugins/docker-compose
sif /usr/local/lib/docker/cli-plugins/docker-compose mode 0100755
EOF

if ! debugfs -w -f /tmp/compose.cmds /tmp/p2-compose.img >/tmp/compose.log 2>&1; then
  tail -5 /tmp/compose.log >&2
  exit 1
fi

debugfs -R "stat /usr/local/lib/docker/cli-plugins/docker-compose" /tmp/p2-compose.img 2>/dev/null |
  grep -E "Inode:|Size:" | head -2

dd if=/tmp/p2-compose.img of="$IMAGE" bs=512 seek="$START" conv=notrunc status=none
sync
rm -f /tmp/p2-compose.img
echo "compose plugin installed"
