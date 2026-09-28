#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_ROOT="${BUILD_ROOT:-build}"
ANDROID_PLATFORM="${ANDROID_PLATFORM:-android-28}"
NDK_HOME="${ANDROID_NDK_HOME:-${NDK_HOME:-}}"
if [[ -z "$NDK_HOME" && -n "${ANDROID_HOME:-}" && -d "$ANDROID_HOME/ndk/28.2.13676358" ]]; then
  NDK_HOME="$ANDROID_HOME/ndk/28.2.13676358"
fi
if [[ -z "$NDK_HOME" || ! -d "$NDK_HOME" ]]; then
  echo "Android NDK not found. Export ANDROID_NDK_HOME or NDK_HOME." >&2
  exit 1
fi
cmake -S "$ROOT" -B "$ROOT/${BUILD_ROOT}-config" -G Ninja
cmake --build "$ROOT/${BUILD_ROOT}-config" --target levi_generate_config
cmake -S "$ROOT" -B "$ROOT/${BUILD_ROOT}-arm64-v8a" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM="$ANDROID_PLATFORM" \
  -DANDROID_STL=c++_shared \
  -DLEVI_PACKAGE_CONFIG_DIR="$ROOT/${BUILD_ROOT}-config/generated-config"
cmake --build "$ROOT/${BUILD_ROOT}-arm64-v8a" --target levi_package
