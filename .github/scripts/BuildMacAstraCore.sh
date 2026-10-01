#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
revision="$(tr -d '\r\n' < "$root/third_party/astracore/upstream-revision.txt")"
checkout="$root/build/astracore-upstream"
git init "$checkout"
git -C "$checkout" remote add origin https://github.com/Cuptu/AstraCore.git
git -C "$checkout" fetch --depth 1 origin "$revision"
git -C "$checkout" checkout --detach FETCH_HEAD
[[ "$(git -C "$checkout" rev-parse HEAD)" == "$revision" ]]
cmp "$root/third_party/astracore/src/astracore.c" "$checkout/src/astracore.c"
cmp "$root/third_party/astracore/include/astracore.h" "$checkout/include/astracore.h"
export HOMEBREW_NO_AUTO_UPDATE=1 HOMEBREW_NO_INSTALL_CLEANUP=1
for package in meson nasm fribidi freetype x264 svt-av1 little-cms2 vulkan-headers; do
    brew list "$package" >/dev/null 2>&1 || brew install "$package"
done
bash "$checkout/scripts/prepare-sources.sh"
bash "$checkout/scripts/build-macos.sh"
runtime="$checkout/artifacts/astracore/osx-arm64"
python3 "$checkout/scripts/generate-manifest.py" --runtime-dir "$runtime" --rid osx-arm64
python3 "$checkout/scripts/verify-runtime.py" --runtime-dir "$runtime"
echo "ASTRACORE_CHECKOUT=$checkout" >> "$GITHUB_ENV"
echo "ASTRACORE_RUNTIME=$runtime" >> "$GITHUB_ENV"
