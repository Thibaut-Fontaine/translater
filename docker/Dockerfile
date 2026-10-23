# syntax=docker/dockerfile:1
#
# Runtime image for the Raspberry Pi 5 (linux/arm64).
#
#   docker buildx build --platform linux/arm64 -f docker/Dockerfile \
#     -t xtrans:arm64 --load .
#
# The build stage runs on the host CPU and cross-compiles with
# g++-aarch64-linux-gnu against Debian's arm64 libraries (multiarch), so no
# emulation is needed to produce the image. The arm64 tests still require
# binfmt/qemu, which is only used when someone runs the image on x86.
FROM --platform=$BUILDPLATFORM debian:trixie-slim AS build

ARG DEBIAN_FRONTEND=noninteractive
RUN dpkg --add-architecture arm64 \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential \
      ca-certificates \
      cmake \
      git \
      g++-aarch64-linux-gnu \
      gcc-aarch64-linux-gnu \
      libc6-dev-arm64-cross \
      libasound2-dev:arm64 \
      libgpiod-dev:arm64 \
      libonnxruntime-dev:arm64 \
      libsentencepiece-dev:arm64 \
      ninja-build \
      nlohmann-json3-dev \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt ./
COPY cmake/ cmake/
COPY include/ include/
COPY src/ src/
COPY apps/ apps/
COPY third_party/whisper.cpp/ third_party/whisper.cpp/
COPY third_party/piper/ third_party/piper/

RUN cmake -S . -B build -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DXTRANS_BUILD_TESTS=OFF \
 && cmake --build build --target xtransd -j"$(nproc)" \
 && aarch64-linux-gnu-strip build/xtransd build/piper/libpiper.so

# Runtime: no compiler, no package manager writes, no network.
FROM --platform=linux/arm64 debian:trixie-slim

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      libasound2t64 \
      libgpiod3 \
      libgomp1 \
      libonnxruntime1.21 \
      libsentencepiece0 \
      tini \
 && ln -sf /usr/lib/aarch64-linux-gnu/libonnxruntime.so.1.21 \
          /usr/lib/aarch64-linux-gnu/libonnxruntime.so.1 \
 && rm -rf /var/lib/apt/lists/* \
 && useradd --system --uid 10001 --user-group \
      --home-dir /var/lib/xtrans --create-home --shell /usr/sbin/nologin xtrans

COPY --from=build /src/build/xtransd /usr/local/bin/xtransd
COPY --from=build /src/build/piper/libpiper.so /usr/local/lib/libpiper.so
COPY --from=build /src/build/espeak_ng-install/share/espeak-ng-data /opt/xtrans/espeak-ng-data

ENV XTRANS_MODELS=/opt/xtrans/models
ENV LD_LIBRARY_PATH=/usr/local/lib
WORKDIR /var/lib/xtrans

USER xtrans
ENTRYPOINT ["/usr/bin/tini", "--"]
CMD ["xtransd", "--device"]
