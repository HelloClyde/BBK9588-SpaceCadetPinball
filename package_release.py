"""Create a complete deterministic install ZIP with verified original resources."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import zipfile
from resource_archive import validate_resource_pack

ROOT = Path(__file__).resolve().parent


def sha(data):
    return hashlib.sha256(data).hexdigest()


def write_zip(path, files):
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, content in sorted(files.items()):
            info = zipfile.ZipInfo(name, (2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, content)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--with-data', action='store_true', help=argparse.SUPPRESS)
    parser.parse_args()
    version = (ROOT / 'VERSION').read_text().strip()
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        raise SystemExit('VERSION must be MAJOR.MINOR.PATCH')
    binary = (ROOT / 'SpaceCadet9588.bda').read_bytes()
    metadata = json.loads((ROOT / 'build-native/build.json').read_text())
    if metadata['sha256'] != sha(binary):
        raise SystemExit('BDA differs from the current build record; rebuild before packaging')
    packed = (ROOT / 'assets/PINBALL.PAK').read_bytes()
    pack_metadata = validate_resource_pack(packed)
    target = ROOT / 'dist'
    target.mkdir(exist_ok=True)
    files = {'应用/程序/太空弹球.bda': binary}
    for name in ['LICENSE', 'SOURCE_NOTICE.md', 'INSTALL.md', 'licenses/Apache-2.0.txt', 'licenses/SDK-NOTICE.txt', 'native/engine/LICENSE']:
        files[name] = (ROOT / name).read_text(encoding='utf8').encode('utf8')
    files['应用/数据/游戏/Pinball/PINBALL.PAK'] = packed
    files['resource-provenance.json'] = (ROOT / 'assets/original-resources.json').read_text(encoding='utf8').encode('utf8')
    files['SHA256SUMS.txt'] = ''.join(f'{sha(data)}  {name}\n' for name, data in sorted(files.items())).encode('utf8')
    archive = target / f'SpaceCadet9588-{version}.zip'
    write_zip(archive, files)
    (target / 'SpaceCadet9588.bda').write_bytes(binary)
    metadata['version'] = version
    metadata['includes_original_game_data'] = True
    metadata['resource_pack'] = pack_metadata
    (target / 'BUILDINFO.json').write_text(json.dumps(metadata, indent=2)+'\n', encoding='utf8')
    outputs = ['SpaceCadet9588.bda', archive.name, 'BUILDINFO.json']
    (target / 'SHA256SUMS.txt').write_text(''.join(f'{sha((target / name).read_bytes())}  {name}\n' for name in outputs), encoding='utf8')
    print(f'Packaged {archive.name}; includes {pack_metadata["file_count"]} verified original resources')


if __name__ == '__main__':
    main()
