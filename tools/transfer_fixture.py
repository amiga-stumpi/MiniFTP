#!/usr/bin/env python3
"""Create reproducible FTP benchmark files or verify a returned copy."""
import argparse
import hashlib
import json
from pathlib import Path


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(65536), b''):
            h.update(chunk)
    return h.hexdigest()


def create(destination, large_kib):
    destination.mkdir(parents=True, exist_ok=False)
    data = destination / 'data'
    data.mkdir()
    sizes = {'large.bin': large_kib * 1024, 'empty.bin': 0}
    sizes.update({f'small/f{i:03}.bin': 1024 for i in range(200)})
    sizes.update({'nested/one.bin': 8193, 'nested/sub/two.bin': 16385,
                  'nested/sub/deep/three.bin': 32769})
    manifest = {}
    for name, size in sizes.items():
        path = data / name
        path.parent.mkdir(parents=True, exist_ok=True)
        block = hashlib.sha256(name.encode('ascii')).digest() * 256
        with path.open('wb') as stream:
            remaining = size
            while remaining:
                part = block[:min(remaining, len(block))]
                stream.write(part)
                remaining -= len(part)
        manifest[name] = {'size': size, 'sha256': digest(path)}
    (destination / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Created {len(manifest)} files in {data}')


def verify(fixture, returned):
    if not returned.is_dir():
        raise SystemExit(f'Not a directory: {returned}')
    manifest = json.loads((fixture / 'manifest.json').read_text())
    errors = []
    for name, expected in manifest.items():
        path = returned / name
        if not path.is_file():
            errors.append(f'Missing: {name}')
        elif path.stat().st_size != expected['size'] or digest(path) != expected['sha256']:
            errors.append(f'Mismatch: {name}')
    actual = {p.relative_to(returned).as_posix() for p in returned.rglob('*') if p.is_file()}
    errors.extend(f'Unexpected: {name}' for name in sorted(actual - manifest.keys()))
    if errors:
        raise SystemExit('\n'.join(errors))
    print(f'PASS: {len(manifest)} files match sizes and SHA-256 hashes')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    creator = commands.add_parser('create')
    creator.add_argument('destination', type=Path)
    creator.add_argument('--large-kib', type=int, default=1024)
    checker = commands.add_parser('verify')
    checker.add_argument('fixture', type=Path)
    checker.add_argument('returned_data', type=Path)
    args = parser.parse_args()
    if args.command == 'create':
        if args.large_kib < 1:
            parser.error('--large-kib must be positive')
        create(args.destination, args.large_kib)
    else:
        verify(args.fixture, args.returned_data)
