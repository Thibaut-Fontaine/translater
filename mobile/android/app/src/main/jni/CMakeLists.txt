# RN's app-level build. It MUST stay the top-level project: its OnLoad.cpp
# installs the Java TurboModule provider (DefaultTurboModuleManagerDelegate).
# Replacing this file with a plain library was the startup crash: every
# TurboModule, PlatformConstants included, became unreachable.
cmake_minimum_required(VERSION 3.22)
project(appmodules)

include(${REACT_ANDROID_DIR}/cmake-utils/ReactNative-application.cmake)

# The translator itself, in its own directory so it is not compiled into
# appmodules (which only needs RN's OnLoad.cpp).
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../cpp xtrans-native)
