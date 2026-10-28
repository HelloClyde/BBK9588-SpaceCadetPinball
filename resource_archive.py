"""Validate the shipped SCPAK archive against every pinned original resource."""
from pathlib import Path
import hashlib
import json
import struct

ROOT = Path(__file__).resolve().parent


def validate_resource_pack(payload, manifest=None):
    if manifest is None:
        manifest = json.loads((ROOT / 'assets/original-resources.json').read_text(encoding='utf8'))
    expected = manifest['resource_pack']
    digest = hashlib.sha256(payload).hexdigest()
    if len(payload) != expected['size'] or digest != expected['sha256']:
        raise ValueError('PINBALL.PAK size or SHA-256 differs from the pinned manifest')
    if len(payload) < 16:
        raise ValueError('Truncated resource archive')
    magic, count, reserved = struct.unpack_from('<8sII', payload)
    records = {row['name']: row for row in manifest['files']}
    header_size = 16 + count * 40
    if (magic != b'SCPAK01\0' or reserved != 0 or not 0 < count <= 128
            or count != len(records) or count != expected['file_count'] or header_size > len(payload)):
        raise ValueError('Invalid resource archive header or file count')
    seen, ranges = set(), []
    for index in range(count):
        raw_name, offset, size = struct.unpack_from('<32sII', payload, 16 + index * 40)
        if b'\0' not in raw_name:
            raise ValueError('Resource name is not terminated')
        name = raw_name.split(b'\0', 1)[0].decode('ascii')
        if (not name or name in ('.', '..') or '/' in name or '\\' in name
                or name.upper() in seen or name not in records):
            raise ValueError(f'Unexpected or duplicate resource name: {name}')
        seen.add(name.upper())
        if offset < header_size or offset % 4 or offset > len(payload) or size > len(payload) - offset:
            raise ValueError(f'Resource outside archive: {name}')
        row = records[name]
        if size != row['size'] or hashlib.sha256(payload[offset:offset + size]).hexdigest() != row['sha256']:
            raise ValueError(f'Original resource hash mismatch: {name}')
        ranges.append((offset, offset + size))
    ranges.sort()
    if any(left[1] > right[0] for left, right in zip(ranges, ranges[1:])):
        raise ValueError('Resource entries overlap')
    return {'name': expected['name'], 'size': len(payload), 'sha256': digest, 'file_count': count}


if __name__ == '__main__':
    print(json.dumps(validate_resource_pack((ROOT / 'assets/PINBALL.PAK').read_bytes()), indent=2))
