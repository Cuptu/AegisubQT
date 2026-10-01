#!/usr/bin/env bash
set -euo pipefail
image="$1"
mount="$(mktemp -d "${RUNNER_TEMP:-/tmp}/AegisubQT dmg 核验.XXXXXX")"
copy="$(mktemp -d "${RUNNER_TEMP:-/tmp}/AegisubQT relocated 核验.XXXXXX")"
cleanup() {
    hdiutil detach "$mount" >/dev/null 2>&1 || true
    rm -rf "$mount" "$copy"
}
trap cleanup EXIT
hdiutil attach "$image" -readonly -nobrowse -mountpoint "$mount"
ditto "$mount/AegisubQT.app" "$copy/AegisubQT.app"
hdiutil detach "$mount"
codesign --verify --deep --strict --verbose=2 "$copy/AegisubQT.app"
python3 .github/scripts/VerifyMacStartup.py "$copy/AegisubQT.app"
python3 "$ASTRACORE_CHECKOUT/scripts/verify-runtime.py" \
    --runtime-dir "$copy/AegisubQT.app/Contents/Resources/astracore"
# Exercise the application's resolver and Qt bridge using only this payload.
runtime="$copy/AegisubQT.app/Contents/Resources/astracore"
find "$runtime" -type f -print0 | while IFS= read -r -d '' binary; do
    if file -b "$binary" | grep -q 'Mach-O'; then
        codesign --verify --strict "$binary"
    fi
done
python3 "$ASTRACORE_CHECKOUT/scripts/make-test-inputs.py" "$copy"
env -u DYLD_LIBRARY_PATH -u DYLD_FALLBACK_LIBRARY_PATH -u DYLD_INSERT_LIBRARIES \
    "$runtime/ffmpeg" -nostdin -v error -y -framerate 10 -i "$copy/frame%02d.png" \
    -i "$copy/tone.wav" -c:v libx264 -bf 3 -crf 18 -pix_fmt yuv420p \
    -c:a pcm_s16le -shortest "$copy/media.mkv"
cp build/astra_deployed_bridge_smoke "$copy/AegisubQT.app/Contents/MacOS/"
codesign --force --timestamp=none --sign - "$copy/AegisubQT.app/Contents/MacOS/astra_deployed_bridge_smoke"
env -u DYLD_LIBRARY_PATH -u DYLD_FALLBACK_LIBRARY_PATH -u DYLD_INSERT_LIBRARIES \
    "$copy/AegisubQT.app/Contents/MacOS/astra_deployed_bridge_smoke" "$copy/media.mkv"
