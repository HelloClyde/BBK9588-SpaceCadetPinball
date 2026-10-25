"""Verify the exact release files downloaded from a CI artifact."""
from pathlib import Path
import hashlib
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else 'dist').resolve()
for line in (root / 'SHA256SUMS.txt').read_text(encoding='utf8').splitlines():
    digest, name = line.split('  ', 1)
    path = (root / name).resolve()
    if path.parent != root or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
        raise SystemExit(f'Checksum mismatch: {name}')
    print(f'OK {name}')
