#!/usr/bin/env python3
"""Exercise packaged native/session and independent libmpv APIs in a fresh process."""
import ctypes as c
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    runtime = Path(sys.argv[1]).resolve()
    manifest = json.loads((runtime / 'astracore-runtime.json').read_text())
    dll_scope = None
    if sys.platform == 'win32':
        import os
        dll_scope = os.add_dll_directory(str(runtime))
    native = c.CDLL(str(runtime / manifest['components']['native']))
    native.ac_abi_version.restype = c.c_uint32
    native.ac_has_feature.argtypes = [c.c_char_p]
    native.ac_has_feature.restype = c.c_int
    if native.ac_abi_version() != 5 or native.ac_has_feature(b'video_session') != 1:
        raise RuntimeError('ABI 5 with persistent video_session capability is required')
    open_session = native.ac_video_open_session_utf8
    open_session.argtypes = [c.c_char_p, c.c_void_p, c.c_size_t]
    open_session.restype = c.c_void_p
    grab = native.ac_video_session_grab_frame
    grab.argtypes = [c.c_void_p, c.c_double, c.c_int, c.c_int, c.c_int, c.c_void_p,
                    c.c_size_t, c.POINTER(c.c_int), c.POINTER(c.c_int),
                    c.POINTER(c.c_double), c.c_void_p, c.c_size_t]
    grab.restype = c.c_int
    close = native.ac_video_close_session
    close.argtypes = [c.c_void_p]
    close.restype = None
    spec = importlib.util.spec_from_file_location('inputs', Path(__file__).with_name('make-test-inputs.py'))
    inputs = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(inputs)
    with tempfile.TemporaryDirectory(prefix='astracore-deployed-') as temp:
        inputs.generate(temp)
        media = Path(temp) / 'fixture.mkv'
        subprocess.run([str(runtime / manifest['components']['ffmpeg']), '-nostdin', '-v', 'error', '-y',
            '-framerate', '10', '-i', str(Path(temp) / 'frame%02d.png'), '-i', str(Path(temp) / 'tone.wav'),
            '-c:v', 'libx264', '-bf', '3', '-crf', '18', '-pix_fmt', 'yuv420p',
            '-c:a', 'pcm_s16le', '-shortest', str(media)], check=True, timeout=30)
        error = c.create_string_buffer(512)
        session = open_session(str(media).encode('utf8'), error, len(error))
        if not session:
            raise RuntimeError(f'Cannot open real video: {error.value!r}')
        try:
            previous = None
            for timestamp in (0.3, 0.3, 0.6, 0.1, 0.9, 0.9):
                pixels = (c.c_ubyte * (48 * 32 * 4))()
                width, height, actual = c.c_int(), c.c_int(), c.c_double(-1)
                result = grab(session, timestamp, 48, 32, 1, pixels, len(pixels),
                    c.byref(width), c.byref(height), c.byref(actual), error, len(error))
                if result or (width.value, height.value) != (48, 32) or abs(actual.value - timestamp) > 0.001:
                    raise RuntimeError(f'Frame {timestamp} failed: {result}, pts={actual.value}, {error.value!r}')
                rgba = bytes(pixels)
                if any(rgba[i] != 255 for i in range(3, len(rgba), 4)) or not any(rgba[0::4]):
                    raise RuntimeError('Decoded pixels are empty or invalid')
                if previous and previous[0] == timestamp and previous[1] != rgba:
                    raise RuntimeError('Repeated frame request returned different pixels')
                previous = timestamp, rgba
        finally:
            close(session)
    mpv = c.CDLL(str(runtime / manifest['components']['libMpv']))
    mpv.mpv_client_api_version.restype = c.c_ulong
    api = mpv.mpv_client_api_version()
    if api >> 16 != 2 or (api & 0xffff) < 5:
        raise RuntimeError(f'Unexpected libmpv client API: {api:#x}')
    mpv.mpv_create.restype = c.c_void_p
    mpv.mpv_initialize.argtypes = [c.c_void_p]
    mpv.mpv_initialize.restype = c.c_int
    mpv.mpv_set_option_string.argtypes = [c.c_void_p, c.c_char_p, c.c_char_p]
    mpv.mpv_set_option_string.restype = c.c_int
    mpv.mpv_terminate_destroy.argtypes = [c.c_void_p]
    mpv.mpv_terminate_destroy.restype = None
    handle = mpv.mpv_create()
    if not handle:
        raise RuntimeError('libmpv context creation failed')
    try:
        for option in (b'vo', b'ao'):
            if mpv.mpv_set_option_string(handle, option, b'null') < 0:
                raise RuntimeError('libmpv headless configuration failed')
        if mpv.mpv_initialize(handle) < 0:
            raise RuntimeError('libmpv initialization failed')
    finally:
        mpv.mpv_terminate_destroy(handle)
    print('Packaged native sessions decoded real frames; independent libmpv initialized.')


if __name__ == '__main__':
    main()
