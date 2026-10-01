#!/usr/bin/env bash
set -euo pipefail
bundle="$1"
destination="$bundle/Contents/Frameworks/astracore"
[[ -d "$ASTRACORE_RUNTIME" && ! -e "$destination" ]]
# Preserve the complete upstream payload and its flat @loader_path layout.
ditto "$ASTRACORE_RUNTIME" "$destination"
# The application must use the upstream binary, not the Homebrew-linked build.
rm -f "$bundle/Contents/Frameworks/libAstraCore.Native.dylib"
python3 "$ASTRACORE_CHECKOUT/scripts/verify-runtime.py" --runtime-dir "$destination"
