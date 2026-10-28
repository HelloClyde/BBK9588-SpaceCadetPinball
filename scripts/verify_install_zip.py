"""Verify a downloaded release is a complete install, including original assets."""
from pathlib import Path
import hashlib
import json
import sys
import zipfile

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT))
from resource_archive import validate_resource_pack


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1 else PROJECT / 'dist')
    metadata = json.loads((root / 'BUILDINFO.json').read_text(encoding='utf8'))
    version = (PROJECT / 'VERSION').read_text().strip()
    if metadata['version'] != version or metadata.get('includes_original_game_data') is not True:
        raise ValueError('Release metadata must describe a full-data package for this version')
    binary = (root / 'SpaceCadet9588.bda').read_bytes()
    if hashlib.sha256(binary).hexdigest() != metadata['sha256']:
        raise ValueError('Standalone BDA differs from build metadata')
    with zipfile.ZipFile(root / f'SpaceCadet9588-{version}.zip') as archive:
        names = archive.namelist()
        if len(names) != len(set(names)):
            raise ValueError('ZIP contains duplicate paths')
        for name in names:
            if name.startswith(('/', '\\')) or '..' in name.split('/') or '\\' in name:
                raise ValueError(f'Invalid install path: {name}')
            if name.upper().endswith(('.SAV', '.EXE', '.BIN')):
                raise ValueError(f'Unexpected personal save, program or firmware: {name}')
        if archive.read('应用/程序/太空弹球.bda') != binary:
            raise ValueError('Installed BDA differs from standalone release file')
        packed = archive.read('应用/数据/游戏/Pinball/PINBALL.PAK')
        provenance = json.loads(archive.read('resource-provenance.json'))
        pack_metadata = validate_resource_pack(packed)
        if validate_resource_pack(packed, provenance) != pack_metadata or metadata['resource_pack'] != pack_metadata:
            raise ValueError('Resource metadata differs from the verified archive')
        checks = {}
        for line in archive.read('SHA256SUMS.txt').decode('utf8').splitlines():
            digest, name = line.split('  ', 1)
            if name in checks or hashlib.sha256(archive.read(name)).hexdigest() != digest:
                raise ValueError(f'Invalid ZIP checksum: {name}')
            checks[name] = digest
        if set(checks) != set(names) - {'SHA256SUMS.txt'}:
            raise ValueError('ZIP checksum table does not cover every install file')
    print(f'Complete install verified: BDA + PINBALL.PAK ({pack_metadata["file_count"]} original resources)')


if __name__ == '__main__':
    main()
