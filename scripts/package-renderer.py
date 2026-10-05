"""Package one separately built native FacetWire plugin without host code.

The target label is supplied by the trusted build operator. This is an
integrity/package tool, not a code-signing or runtime trust decision.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
import re
import zipfile


class PackageError(ValueError):
    pass


def package(manifest_path: Path, library_path: Path, license_path: Path,
            target: str, output_path: Path) -> dict:
    inputs = (manifest_path, library_path, license_path)
    if any(not path.is_absolute() or not path.is_file() or path.is_symlink()
           for path in inputs) or not output_path.is_absolute() or output_path.exists():
        raise PackageError('existing absolute regular inputs and unused absolute output required')
    if re.fullmatch(r'(windows|macos|linux)-(x86_64|arm64)|macos-universal', target) is None:
        raise PackageError('explicit supported desktop target required')
    suffix = '.dll' if target.startswith('windows-') else '.dylib' if target.startswith('macos-') else '.so'
    if library_path.suffix.lower() != suffix or '/' in library_path.name or '\\' in library_path.name:
        raise PackageError('library extension differs from the declared target')
    if any(path.stat().st_size == 0 for path in inputs) or library_path.stat().st_size > 64 * 1024 * 1024:
        raise PackageError('empty or oversized package input')
    try:
        source = json.loads(manifest_path.read_text(encoding='utf-8'))
    except (UnicodeError, ValueError) as exc:
        raise PackageError('UTF-8 source manifest required') from exc
    if (type(source) is not dict or source.get('format') != 'facetwire.plugin-manifest'
            or source.get('formatVersion') != '0.1' or type(source.get('plugin')) is not dict
            or type(source.get('capabilities')) is not list
            or source.get('permissions') != [] or source.get('dependencies') != []
            or type(source.get('artifacts')) is not list or len(source['artifacts']) != 1
            or type(source['artifacts'][0]) is not dict
            or source['artifacts'][0].get('profile') != 'static'):
        raise PackageError('one permission-free reference static renderer manifest required')
    library = library_path.read_bytes()
    license_bytes = license_path.read_bytes()
    location = 'lib/' + library_path.name
    manifest = dict(source, artifacts=[dict(target=target, profile='native-dynamic',
        path=location, querySymbol='facetwire_plugin_query',
        sha256=hashlib.sha256(library).hexdigest())])
    encoded = json.dumps(manifest, sort_keys=True, ensure_ascii=False,
        separators=(',', ':'), allow_nan=False).encode('utf-8')
    if len(encoded) > 1024 * 1024 or len(license_bytes) > 1024 * 1024:
        raise PackageError('manifest or license exceeds package limit')
    memory = io.BytesIO()
    with zipfile.ZipFile(memory, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for name, payload in (('LICENSE', license_bytes), ('facetwire.plugin.json', encoded),
                              (location, library)):
            info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, payload)
    with output_path.open('xb') as stream:
        stream.write(memory.getvalue())
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('manifest', 'library', 'license', 'target', 'output'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    package(Path(args.manifest), Path(args.library), Path(args.license),
            args.target, Path(args.output))


if __name__ == '__main__':
    main()
