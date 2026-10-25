"""Verify and import a local folder of original data; no downloads or execution."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil

ROOT = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    args = parser.parse_args()
    source = args.source.resolve()
    manifest = json.loads((ROOT / 'assets/original-resources.json').read_text(encoding='utf8'))
    names = {p.name.upper(): p for p in source.iterdir() if p.is_file()}
    verified = []
    for row in manifest['files']:
        path = names.get(row['name'].upper())
        if not path or path.stat().st_size != row['size'] or hashlib.sha256(path.read_bytes()).hexdigest() != row['sha256']:
            raise SystemExit(f'Missing or different original resource: {row["name"]}')
        verified.append((path, row['name']))
    dest = ROOT / 'original-data'
    dest.mkdir(exist_ok=True)
    for path, name in verified:
        if path != (dest / name).resolve():
            shutil.copy2(path, dest / name)
    manifest['import_method'] = 'Verified local originals; no downloaded code executed'
    (dest / 'provenance.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf8')
    print(f'Imported {len(verified)} verified resources')


if __name__ == '__main__':
    main()
