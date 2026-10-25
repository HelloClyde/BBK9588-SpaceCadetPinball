"""Fetch a pinned public web bundle and statically extract original resources."""
from pathlib import Path
import base64
import hashlib
import json
import re
import urllib.request

ROOT = Path(__file__).resolve().parent / 'original-data'
COMMIT = '684f0b57d0cc93d5a29329f0b59d9996c54f1553'
URL = f'https://raw.githubusercontent.com/lrusso/3DPinballSpaceCadet/{COMMIT}/3DPinballSpaceCadet.js'
EXPECTED_BUNDLE = 'bd27b7999795e8638e1024785f30f37fc3d06ecd5242f2e87b1699eddf6c6feb'
ROOT.mkdir(exist_ok=True)
bundle = ROOT / 'web-bundle.js'
if not bundle.exists():
    with urllib.request.urlopen(URL, timeout=90) as response:
        content = response.read()
    assert hashlib.sha256(content).hexdigest() == EXPECTED_BUNDLE
    bundle.write_bytes(content)
content = bundle.read_bytes()
assert hashlib.sha256(content).hexdigest() == EXPECTED_BUNDLE, 'Unexpected bundle hash'
source = content.decode('utf8')
encoded = re.search(r'var DATA_FILE = "data:application/octet-stream;base64,([^"]+)"', source).group(1)
payload = base64.b64decode(encoded, validate=True)
metadata = json.loads(re.search(r'(\{"files":\[.*?\],"remote_package_size":\d+,"package_uuid":"[^"]+"\})', source).group(1))
assert len(payload) == metadata['remote_package_size']
records = []
for entry in metadata['files']:
    name = Path(entry['filename']).name
    assert name and name not in ('.', '..')
    data = payload[entry['start']:entry['end']]
    assert len(data) == entry['end'] - entry['start']
    target = ROOT / name
    if target.exists():
        assert target.read_bytes() == data, f'Existing resource differs: {name}'
    else:
        target.write_bytes(data)
    records.append({'name': name, 'size': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
assert (ROOT / 'PINBALL.DAT').read_bytes().startswith(b'PARTOUT(4.0)RESOURCE')
provenance = {
    'source': URL, 'pinned_source': URL, 'source_commit': COMMIT,
    'project': 'https://github.com/lrusso/3DPinballSpaceCadet',
    'method': 'Static base64 data extraction; downloaded code is never executed',
    'bundle_sha256': EXPECTED_BUNDLE, 'package_uuid': metadata['package_uuid'],
    'verified_signature': 'PARTOUT(4.0)RESOURCE', 'files': records,
    'sound_files': sum(row['name'].upper().endswith('.WAV') for row in records),
}
(ROOT / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n', encoding='utf8')
print(f'Verified {len(records)} original resource files from {COMMIT}')
