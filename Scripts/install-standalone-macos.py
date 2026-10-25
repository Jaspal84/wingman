#!/usr/bin/env python3
"""Install a local standalone bundle transactionally; never requires sudo by default."""
import argparse
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys
import tempfile


def inspect(bundle):
    bundle = Path(bundle).resolve(strict=True)
    if bundle.suffix != '.app':
        raise ValueError('Expected a compiled .app bundle')
    with (bundle / 'Contents/Info.plist').open('rb') as stream:
        info = plistlib.load(stream)
    name = info.get('CFBundleExecutable', '')
    if not name or Path(name).name != name:
        raise ValueError('Invalid standalone executable name')
    executable = bundle / 'Contents/MacOS' / name
    if not executable.is_file() or not os.access(executable, os.X_OK):
        raise ValueError('Standalone executable missing or not executable')
    return bundle, executable


def run(args):
    print('+', ' '.join(map(str, args)), flush=True)
    return subprocess.run(list(map(str, args)), check=True, capture_output=False)


def install(source, system=False, launch=False):
    if sys.platform != 'darwin':
        raise ValueError('Installation and signing require macOS')
    source, executable = inspect(source)
    run(['/usr/bin/lipo', '-verify_arch', 'arm64', executable])
    root = Path('/Applications') if system else Path.home() / 'Applications'
    root.mkdir(parents=True, exist_ok=True)
    destination = root / 'JASWINGMAN PRO.app'
    if source == destination:
        raise ValueError('Choose the build output, not the installed application')
    with tempfile.TemporaryDirectory(prefix='.jaswingman-install-', dir=root) as work:
        staged = Path(work) / destination.name
        run(['/usr/bin/ditto', source, staged])
        run(['/usr/bin/codesign', '--force', '--deep', '--sign', '-', staged])
        run(['/usr/bin/codesign', '--verify', '--deep', '--strict', staged])
        _, binary = inspect(staged)
        run(['/usr/bin/lipo', '-verify_arch', 'arm64', binary])
        backup = Path(work) / 'previous.app'
        if destination.exists():
            destination.rename(backup)
        try:
            staged.rename(destination)
            inspect(destination)
        except Exception:
            if destination.exists():
                shutil.rmtree(destination)
            if backup.exists():
                backup.rename(destination)
            raise
    print('Installed:', destination)
    if launch:
        run(['/usr/bin/open', destination])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bundle')
    parser.add_argument('--system', action='store_true', help='Use /Applications (requires write permission)')
    parser.add_argument('--launch', action='store_true')
    args = parser.parse_args()
    try:
        install(args.bundle, args.system, args.launch)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, 'Standalone installation failed: ' + str(error) + '\n')
