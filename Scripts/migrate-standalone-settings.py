#!/usr/bin/env python3
"""Copy legacy JUCE standalone settings without overwriting either version."""
import argparse
from pathlib import Path
import shutil
import sys


def migrate(directory):
    root = Path(directory)
    destination = root / 'JASWINGMAN PRO.settings'
    if destination.exists():
        return 'Existing JASWINGMAN PRO settings preserved'
    for name in ('Wingman Pro.settings', 'Wingman.settings'):
        source = root / name
        if source.is_file():
            created = False
            try:
                with destination.open('xb') as output:
                    created = True
                    with source.open('rb') as original:
                        shutil.copyfileobj(original, output)
            except FileExistsError:
                return 'Existing JASWINGMAN PRO settings preserved'
            except BaseException:
                if created:
                    destination.unlink(missing_ok=True)
                raise
            return 'Copied legacy settings from ' + str(source)
    return 'No legacy standalone settings found; nothing changed'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path)
    args = parser.parse_args()
    if args.directory is None and sys.platform != 'darwin':
        parser.error('Default settings location requires macOS; use --directory for an explicit fixture')
    print(migrate(args.directory or Path.home() / 'Library/Application Support'))


if __name__ == '__main__':
    main()
