# syntax=docker/dockerfile:1
#
# Runtime image for the Raspberry Pi 5 (linux/arm64).
#
#   docker buildx build --platform linux/arm64 -f docker/Dockerfile \
#     -t xtrans:arm64 --output type=docker,dest=build/xtrans-arm64.tar .
#
# The build stage runs on the native host CPU and cross-compiles for aarch64,
# so no QEMU emulation is needed to produce the image.
FROM --platform=$BUILDPLATFORM debian:bookworm-slim AS build

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential \
      cmake \
      g++-aarch64-linux-gnu \
      gcc-aarch64-linux-gnu \
      libc6-dev-arm64-cross \
      ninja-build \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt ./
COPY cmake/ cmake/
COPY include/ include/
COPY src/ src/
COPY apps/ apps/

RUN cmake -S . -B build -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DXTRANS_BUILD_TESTS=OFF \
 && cmake --build build --target xtransd -j"$(nproc)" \
 && aarch64-linux-gnu-strip build/xtransd

# Runtime: no compiler, no package manager writes, no network.
FROM debian:bookworm-slim

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      libasound2 \
      libgpiod2 \
      tini \
 && rm -rf /var/lib/apt/lists/* \
 && useradd --system --uid 10001 --user-group \
      --home-dir /var/lib/xtrans --create-home --shell /usr/sbin/nologin xtrans

COPY --from=build /src/build/xtransd /usr/local/bin/xtransd

ENV XTRANS_MODELS=/opt/xtrans/models
WORKDIR /var/lib/xtrans

# uid 10001 with no shell; audio and gpio group membership comes from compose.
USER xtrans
ENTRYPOINT ["/usr/bin/tini", "--"]
CMD ["xtransd"]
