#!/usr/bin/env python3
"""Hash user library files and record supplied licensing evidence; no downloads."""
import argparse
import hashlib
import json
from pathlib import Path


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def inventory(root, license_file, license_name, source=''):
    root = Path(root).resolve(strict=True)
    license_file = Path(license_file).resolve(strict=True)
    if not root.is_dir() or not license_file.is_file() or not license_name.strip():
        raise ValueError('Library directory and explicit license documentation/name required')
    files = sorted(p for p in root.rglob('*') if p.suffix.lower() in ('.sfz', '.wav', '.aif', '.aiff') and p.is_file() and not p.is_symlink())
    if not files or len(files) > 4096:
        raise ValueError('Expected 1-4096 SFZ/audio files')
    return {'schema_version': 1, 'license_name': license_name, 'source': source,
            'license_document_sha256': digest(license_file),
            'license_review': 'User supplied; not automatic legal approval',
            'coverage': 'Files under selected root only; external SFZ references are not audited',
            'files': [{'path': str(p.relative_to(root)), 'bytes': p.stat().st_size,
                       'sha256': digest(p)} for p in files]}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory')
    parser.add_argument('--license-file', required=True)
    parser.add_argument('--license-name', required=True)
    parser.add_argument('--source', default='')
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    try:
        result = inventory(args.directory, args.license_file, args.license_name, args.source)
        Path(args.output).write_text(json.dumps(result, indent=2) + '\n')
    except (OSError, ValueError) as error:
        parser.exit(1, str(error) + '\n')
