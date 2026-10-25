"""Bundle verified originals for one NAND read; bytes inside remain unchanged."""
from pathlib import Path
import hashlib
import json
import struct

root = Path(__file__).resolve().parent
directory = root / 'original-data'
provenance = json.loads((directory / 'provenance.json').read_text(encoding='utf8'))
records = sorted(provenance['files'], key=lambda row: row['name'].upper())
header_size = 16 + 40 * len(records)
payload = bytearray(header_size)
struct.pack_into('<8sII', payload, 0, b'SCPAK01\0', len(records), 0)
for i, record in enumerate(records):
    path = directory / record['name']
    assert path.resolve().parent == directory.resolve()
    content = path.read_bytes()
    assert len(content) == record['size'] and hashlib.sha256(content).hexdigest() == record['sha256']
    name = record['name'].encode('ascii')
    assert len(name) < 32
    payload.extend(bytes((-len(payload)) % 4))
    offset = len(payload)
    payload.extend(content)
    struct.pack_into('<32sII', payload, 16 + i * 40, name, offset, len(content))
(directory / 'PINBALL.PAK').write_bytes(payload)
print(f'PINBALL.PAK: {len(records)} original files, {len(payload)} bytes; SHA256 {hashlib.sha256(payload).hexdigest()}')
