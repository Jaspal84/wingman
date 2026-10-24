#!/usr/bin/env python3
"""AUv2 metadata inspection, user-level installation and actual-ID validation."""
import argparse
import json
import os
from pathlib import Path
import platform
import plistlib
import shutil
import subprocess
import sys
import tempfile
import uuid


def metadata(bundle):
    bundle = Path(bundle).resolve(strict=True)
    if bundle.suffix != '.component':
        raise ValueError('Expected an AU .component bundle')
    with (bundle / 'Contents/Info.plist').open('rb') as stream:
        plist = plistlib.load(stream)
    entries = plist.get('AudioComponents', [])
    if len(entries) != 1:
        raise ValueError('Expected exactly one AudioComponents entry')
    component = entries[0]
    for field in ('type', 'subtype', 'manufacturer'):
        value = component.get(field)
        if not isinstance(value, str) or len(value) != 4 or not value.isascii():
            raise ValueError('Invalid FourCC: ' + field)
    if not component.get('factoryFunction'):
        raise ValueError('Missing AU factoryFunction')
    name = plist.get('CFBundleExecutable', '')
    if not name or Path(name).name != name:
        raise ValueError('Invalid CFBundleExecutable')
    executable = bundle / 'Contents/MacOS' / name
    if not executable.is_file():
        raise ValueError('Compiled AU executable is missing')
    if not plist.get('CFBundleIdentifier'):
        raise ValueError('Missing CFBundleIdentifier')
    return {'bundle': str(bundle), 'executable': str(executable),
            'bundle_id': plist['CFBundleIdentifier'],
            **{key: component[key] for key in ('type', 'subtype', 'manufacturer')}}


def run(args):
    print('+ ' + ' '.join(map(str, args)), flush=True)
    subprocess.run(args, check=True)


def validate(bundle):
    info = metadata(bundle)
    print(json.dumps(info, indent=2), flush=True)
    run(['/usr/bin/plutil', '-lint', str(Path(bundle) / 'Contents/Info.plist')])
    run(['/usr/bin/codesign', '--verify', '--deep', '--strict', '--verbose=2', info['bundle']])
    run(['/usr/bin/lipo', '-archs', info['executable']])
    # Do not guess category/codes or parse stale CMake source values.
    run(['/usr/bin/auval', '-v', info['type'], info['subtype'], info['manufacturer']])


def install(bundle, adhoc=False):
    source = Path(metadata(bundle)['bundle'])
    root = Path.home() / 'Library/Audio/Plug-Ins/Components'
    root.mkdir(parents=True, exist_ok=True)
    destination = root / source.name
    if source == destination:
        raise ValueError('Source already points to the installed component')
    for existing in root.glob('*.component'):
        if existing == destination:
            continue
        try:
            other = metadata(existing)
        except (OSError, ValueError, plistlib.InvalidFileException):
            continue
        if other['bundle_id'] == metadata(source)['bundle_id']:
            raise ValueError('Conflicting AU identity at ' + str(existing) + '. Move it to a backup outside Components before installing.')
    # Stage on the destination filesystem; preserve an existing installation.
    with tempfile.TemporaryDirectory(prefix='.wingman-install-', dir=root) as stage:
        staged = Path(stage) / source.name
        shutil.copytree(source, staged, symlinks=True)
        if adhoc:
            run(['/usr/bin/codesign', '--force', '--deep', '--sign', '-', str(staged)])
        run(['/usr/bin/codesign', '--verify', '--deep', '--strict', '--verbose=2', str(staged)])
        backup = root / (destination.name + '.backup-' + uuid.uuid4().hex)
        existed = destination.exists()
        if existed:
            destination.rename(backup)
            print('Previous installation retained at ' + str(backup), flush=True)
        try:
            staged.rename(destination)
        except OSError:
            if existed:
                backup.rename(destination)
            raise
    print('Installed ' + str(destination), flush=True)
    print('Close/reopen the DAW, then run validate-au.sh on this installed bundle. '
          'If discovery is stale, log out/in; this tool does not erase AU caches.', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('inspect', 'install', 'validate'))
    parser.add_argument('bundle', type=Path)
    parser.add_argument('--adhoc', action='store_true', help='Ad-hoc sign the staged local-development copy; not distribution signing')
    args = parser.parse_args()
    try:
        if args.action == 'inspect':
            print(json.dumps(metadata(args.bundle), indent=2))
        else:
            if platform.system() != 'Darwin':
                raise RuntimeError('Installation and auval require macOS; only metadata inspection works here')
            if args.action == 'install':
                install(args.bundle, args.adhoc)
            else:
                validate(args.bundle)
        return 0
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError, plistlib.InvalidFileException) as error:
        print(str(error), file=sys.stderr)
        if args.action == 'validate':
            print('For discovery errors: inspect the installed plist, check native architecture/signature, '
                  'run auval -a, restart the DAW and inspect Logic Plug-in Manager. '
                  'An AU build alone does not establish registration.', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
