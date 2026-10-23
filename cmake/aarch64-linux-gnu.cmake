# Cross-compilation for the Raspberry Pi 5 (Cortex-A76, Debian aarch64).
# Usage:
#   cmake -S . -B build-arm \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake \
#         -DCMAKE_BUILD_TYPE=Release
#
# -march is applied to the device build only: the host test build must stay
# runnable on x86_64.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

set(XTRANS_DEVICE_MARCH "-march=armv8.2-a+dotprod+fp16")
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} ${XTRANS_DEVICE_MARCH}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
