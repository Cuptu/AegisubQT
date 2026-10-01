#!/usr/bin/env python3
"""Require each packaged consumer to resolve FFmpeg from one local SDK closure."""
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys


FAMILY = re.compile(r'^(?:lib)?(avcodec|avformat|avutil|avfilter|avdevice|swscale|swresample)[-.]')


def pe_imports(binary):
    data = binary.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[pe:pe+4] != b'PE\0\0':
        raise RuntimeError(f'Invalid PE binary: {binary}')
    sections, optional_size = struct.unpack_from('<H12xH', data, pe + 6)
    optional = pe + 24
    magic = struct.unpack_from('<H', data, optional)[0]
    directory = optional + (112 if magic == 0x20b else 96)
    import_rva = struct.unpack_from('<I', data, directory + 8)[0]
    def offset(rva):
        for i in range(sections):
            entry = optional + optional_size + 40*i
            virtual_size, address, raw_size, raw = struct.unpack_from('<IIII', data, entry + 8)
            if address <= rva < address + max(virtual_size, raw_size):
                return raw + rva - address
        raise RuntimeError(f'Unmapped PE RVA in {binary}')
    if not import_rva:
        return []
    cursor = offset(import_rva)
    names = []
    while any(data[cursor:cursor+20]):
        name_rva = struct.unpack_from('<I', data, cursor + 12)[0]
        names.append(data[offset(name_rva):].split(b'\0', 1)[0].decode('ascii'))
        cursor += 20
    return names


def run(*command):
    return subprocess.check_output(command, text=True, stderr=subprocess.STDOUT)


def main():
    runtime = Path(sys.argv[1]).resolve()
    manifest = json.loads((runtime / 'astracore-runtime.json').read_text())
    roots = [runtime / value for value in manifest['components'].values()]
    versions = {}
    def record(name, resolved):
        match = FAMILY.match(name.lower())
        if not match:
            return
        resolved = resolved.resolve()
        if resolved.parent != runtime or not resolved.is_file():
            raise RuntimeError(f'FFmpeg dependency outside payload: {name} -> {resolved}')
        versions.setdefault(match[1], set()).add(resolved)
    if sys.platform == 'win32':
        files = {file.name.lower(): file for file in runtime.iterdir() if file.is_file()}
        pending, seen = roots[:], set()
        while pending:
            binary = pending.pop()
            if binary in seen:
                continue
            seen.add(binary)
            for name in pe_imports(binary):
                local = files.get(name.lower())
                if local:
                    record(name, local)
                    pending.append(local)
                elif FAMILY.match(name.lower()):
                    raise RuntimeError(f'Missing local FFmpeg dependency: {binary.name} -> {name}')
    elif sys.platform == 'darwin':
        for binary in runtime.iterdir():
            if not binary.is_file() or binary.is_symlink() or 'Mach-O' not in run('/usr/bin/file', '-b', str(binary)):
                continue
            identities = run('/usr/bin/otool', '-D', str(binary)).splitlines()[1:]
            identity = identities[0].strip() if identities else None
            for line in run('/usr/bin/otool', '-L', str(binary)).splitlines()[1:]:
                dep = re.sub(r'\s+\(compatibility version.*$', '', line.strip())
                if dep == identity or dep.startswith(('/usr/lib/', '/System/Library/')):
                    continue
                if not dep.startswith('@loader_path/'):
                    raise RuntimeError(f'External or unresolved dependency: {binary.name} -> {dep}')
                local = binary.parent / dep[len('@loader_path/'):]
                if not local.is_file() or local.resolve().parent != runtime:
                    raise RuntimeError(f'Missing local dependency: {binary.name} -> {dep}')
                record(local.name, local)
    else:
        for binary in roots:
            output = run('ldd', str(binary))
            if 'not found' in output:
                raise RuntimeError(f'Unresolved dependency in {binary}:\n{output}')
            for line in output.splitlines():
                match = re.match(r'\s*(\S+)\s+=>\s+(/.+?)\s+\(0x', line)
                if match:
                    record(match[1], Path(match[2]))
    for family, paths in versions.items():
        if len(paths) != 1:
            raise RuntimeError(f'Consumers use multiple {family} libraries: {paths}')
    if not {'avcodec', 'avformat', 'avutil', 'swscale', 'swresample'}.issubset(versions):
        raise RuntimeError(f'Incomplete FFmpeg dependency audit: {sorted(versions)}')
    print('Consumers share packaged FFmpeg libraries:', ', '.join(sorted(versions)))


if __name__ == '__main__':
    main()
