#!/usr/bin/env python3
import argparse
import hashlib
import json
import os
import subprocess
import sys

def sha256_file(filepath):
    h = hashlib.sha256()
    with open(filepath, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest().upper()

def run_cmd(cmd, env=None):
    try:
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, check=True, env=env)
        return res.stdout
    except subprocess.CalledProcessError as e:
        sys.exit(f"Command failed: {' '.join(cmd)}\nOutput:\n{e.stdout}")

def main():
    parser = argparse.ArgumentParser(description="Verify AstraCore runtime bundle")
    parser.add_argument("--runtime-dir", required=True, help="Directory containing the built AstraCore runtime")
    parser.add_argument("--max-size-mb", type=int, default=200, help="Maximum allowable total size in MB")
    args = parser.parse_args()

    runtime_dir = os.path.abspath(args.runtime_dir)
    manifest_path = os.path.join(runtime_dir, "astracore-runtime.json")
    if not os.path.isfile(manifest_path):
        sys.exit(f"Error: Missing manifest {manifest_path}")

    with open(manifest_path, "r", encoding="utf-8") as f:
        manifest = json.load(f)

    print(f"==> Verifying AstraCore runtime for RID: {manifest.get('runtimeIdentifier')}")

    # 1. Verify files against manifest
    total_bytes = 0
    for entry in manifest.get("files", []):
        file_path = os.path.join(runtime_dir, entry["path"])
        if not os.path.isfile(file_path):
            sys.exit(f"Error: Manifest file missing: {entry['path']}")
        actual_size = os.path.getsize(file_path)
        total_bytes += actual_size
        if actual_size != entry["size"]:
            sys.exit(f"Error: File size mismatch for {entry['path']}: {actual_size} != {entry['size']}")
        actual_hash = sha256_file(file_path)
        if actual_hash != entry["sha256"]:
            sys.exit(f"Error: File SHA256 mismatch for {entry['path']}")

    total_mb = total_bytes / (1024 * 1024)
    print(f"==> Total bundle size: {total_mb:.2f} MB (limit: {args.max_size_mb} MB)")
    if total_mb > args.max_size_mb:
        sys.exit(f"Error: Bundle size {total_mb:.2f} MB exceeds limit {args.max_size_mb} MB")

    # 2. Test FFmpeg functionality
    ffmpeg_bin = os.path.join(runtime_dir, manifest["components"]["ffmpeg"])
    ffprobe_bin = os.path.join(runtime_dir, manifest["components"]["ffprobe"])

    # Do not let the build prefix hide non-relocatable dependency references.
    env = os.environ.copy()
    system_path = os.path.join(os.environ.get("SystemRoot", "C:\\Windows"), "System32") if os.name == "nt" else "/usr/bin:/bin"
    env["PATH"] = f"{runtime_dir}{os.pathsep}{system_path}"
    for key in ("LD_LIBRARY_PATH", "LD_PRELOAD", "DYLD_LIBRARY_PATH", "DYLD_FALLBACK_LIBRARY_PATH", "DYLD_INSERT_LIBRARIES"):
        env.pop(key, None)
    print("==> Auditing packaged FFmpeg dependency closure...")
    print(run_cmd([sys.executable, os.path.join(os.path.dirname(__file__), "verify-media-dependencies.py"), runtime_dir], env=env))

    print("==> Checking ffprobe...")
    run_cmd([ffprobe_bin, "-hide_banner", "-version"], env=env)

    print("==> Checking ffmpeg decoders, encoders, and filters...")
    decoders_out = run_cmd([ffmpeg_bin, "-hide_banner", "-decoders"], env=env)
    for dec in ["h264", "hevc", "av1", "libdav1d", "vp9", "aac", "flac", "opus"]:
        if dec not in decoders_out:
            sys.exit(f"Error: Required decoder missing: {dec}")

    encoders_out = run_cmd([ffmpeg_bin, "-hide_banner", "-encoders"], env=env)
    for enc in ["libx264", "libx265", "libsvtav1", "aac", "pcm_s16le", "png"]:
        if enc not in encoders_out:
            sys.exit(f"Error: Required encoder missing: {enc}")

    filters_out = run_cmd([ffmpeg_bin, "-hide_banner", "-filters"], env=env)
    for flt in ["subtitles", "scale", "format", "fps", "aresample", "atempo", "volume"]:
        if flt not in filters_out:
            sys.exit(f"Error: Required filter missing: {flt}")

    print("==> Testing deployed native video sessions and independent libmpv...")
    print(run_cmd([sys.executable, os.path.join(os.path.dirname(__file__), "test-native-runtime.py"), runtime_dir], env=env))
    print("==> All verification checks passed successfully!")

if __name__ == "__main__":
    main()
