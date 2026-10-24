# Docker

Two images, one script:

| Image | Built by | Purpose |
| --- | --- | --- |
| `xtrans-dev` | `docker/Dockerfile.dev` | Reproducible build + `ctest` on any host |
| `xtrans:arm64` | `docker/Dockerfile` | The runtime that ships to the Pi |

Both are based on `debian:trixie-slim`; nothing is fetched at runtime.

```bash
./scripts/docker.sh --self-test   # checks the compose invariants, no daemon needed
./scripts/docker.sh test          # dev image: build, all tests, fresh build tree
./scripts/docker.sh smoke         # xtransd in the dev image, scripted stdin
./scripts/docker.sh image         # cross-build arm64 -> build/xtrans-arm64.tar
./scripts/docker.sh run           # runtime stack on the Pi
```

## Dev/test image

Tests run **during** `docker build`, so an image tag never exists for a red
tree. A source bind-mount is deliberately not used: the point is a clean,
reproducible tree, not incremental builds. For iteration, keep using the host
build in `build/`.

It also carries `g++-aarch64-linux-gnu`, so the device binary can be
cross-compiled in the same container:

```bash
docker run --rm -v "$PWD:/src" -w /src xtrans-dev sh -c '
  cmake -S . -B build-arm -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake \
        -DCMAKE_BUILD_TYPE=Release -DXTRANS_BUILD_TESTS=OFF &&
  cmake --build build-arm --target xtransd'
```

## Runtime image

`docker/Dockerfile` is multi-stage:

1. **build** runs on `$BUILDPLATFORM` (the host) and cross-compiles with
   `cmake/aarch64-linux-gnu.cmake` (`-march=armv8.2-a+dotprod+fp16`), then
   strips the binary. No QEMU is involved in producing the image.
2. **runtime** is `debian:trixie-slim` plus `libasound2t64`, `libgpiod3`,
   `libonnxruntime1.21`, `libsentencepiece0`, `libgomp1` and `tini`, with
   `xtransd`, `libpiper.so` and the espeak-ng data as the project payload.

The container runs as uid 10001 (`xtrans`, no shell), not as root. The root
filesystem is read-only, all capabilities are dropped, and there is no network
namespace — the device has nothing to talk to.

### Deploying to the Pi

On the workstation:

```bash
./scripts/docker.sh image
scp build/xtrans-arm64.tar pi:~
scp docker/compose.yaml pi:~
```

On the Pi:

```bash
docker load -i ~/xtrans-arm64.tar
sudo mkdir -p /opt/xtrans/models
# copy whisper/piper/onnx models into /opt/xtrans/models, owned by root, 0755
cd ~ && sudo docker compose -f compose.yaml up -d
docker logs -f xtrans
```

Models are mounted read-only from `${XTRANS_MODELS:-/opt/xtrans/models}`. They
are never baked into the image: replacing a voice or a quantisation must not
require rebuilding the container.

Layout the daemon expects:

```
/opt/xtrans/
  espeak-ng-data/                    # phonemizer data (Piper), from the build
  models/
    ggml-base.bin                    # whisper (STT)
    nllb/encoder_model_int8.onnx     # NLLB (MT), pinned in models/nllb.pin
    nllb/decoder_model_quantized.onnx
    nllb/decoder_with_past_model_quantized.onnx
    nllb/sentencepiece.bpe.model
    voices/fr_FR-siwis-medium.onnx   # Piper (TTS)
    voices/en_US-lessac-medium.onnx
```

### Device access

| Need | Compose setting |
| --- | --- |
| Microphone / speaker | `devices: /dev/snd`, `group_add: audio` |
| Buttons | `devices: /dev/gpiochip0`, `group_add: gpio` |

`group_add` uses the container's group names, so the host `audio` and `gpio`
GIDs must match the ones Debian assigns inside the image. Check with
`getent group audio gpio` on both sides. If a device is missing, remove its
`devices` entry; the daemon must still start for the parts that exist.

### Current scope

`xtransd` today is the tested button/pipeline core with a line-based control
interface on stdin. Audio capture, GPIO polling and the model adapters are not
wired yet; `docker/compose.yaml` already declares the devices and the model
mount they will use, so the deployment surface does not change when they land.

## Why not run the tests in the runtime image

The runtime image has no compiler, no shell and no test binary — by design.
Tests belong to the dev image; the runtime image only carries what the device
executes at boot.

## Rules

- Pin the base image by tag (`trixie-slim`), never `latest`.
- No `apt upgrade`, no package added to the runtime stage without a reason
  written in the Dockerfile.
- The image must build without network access to the *project* (no
  `FetchContent`); base packages still come from Debian at build time.
- Keep the compose invariants (`network_mode: none`, `read_only: true`,
  non-root `USER`): `scripts/docker.sh --self-test` enforces them.
