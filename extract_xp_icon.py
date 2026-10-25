"""Extract icon resources from the hash-verified XP executable; never executes it.

Host-only utility requiring pefile and Pillow. The game build consumes the
resulting PNG/ICO, and does not need these libraries.
"""
from pathlib import Path
import argparse
import hashlib
import io
import json
import struct
import pefile
from PIL import Image

ROOT = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('exe', type=Path)
parser.add_argument('--output', type=Path, default=ROOT / 'original-data/icon-source')
args = parser.parse_args()
SOURCE = args.output
SOURCE.mkdir(parents=True, exist_ok=True)
exe = args.exe.read_bytes()
assert hashlib.sha1(exe).hexdigest() == '2a5b525e0f631bb6107639e2a69df15986fb0d05'
pe = pefile.PE(data=exe)
icons, groups = {}, []
for resource_type in pe.DIRECTORY_ENTRY_RESOURCE.entries:
    if resource_type.id not in (3, 14):
        continue
    for resource in resource_type.directory.entries:
        for language in resource.directory.entries:
            record = language.data.struct
            data = pe.get_data(record.OffsetToData, record.Size)
            if resource_type.id == 3:
                icons[(resource.id, language.id)] = data
            else:
                groups.append((str(resource.name) if resource.name is not None else resource.id, language.id, data))

result = []
for group_id, language, group in groups:
    reserved, icon_type, count = struct.unpack_from('<HHH', group)
    assert reserved == 0 and icon_type == 1
    headers, images, frames = [], [], []
    offset = 6 + count * 16
    for index in range(count):
        width, height, colors, reserved, planes, bits, size, resource_id = struct.unpack_from('<BBBBHHIH', group, 6 + index * 14)
        image = icons[(resource_id, language)]
        assert len(image) == size
        headers.append(struct.pack('<BBBBHHII', width, height, colors, reserved, planes, bits, size, offset))
        images.append(image)
        frames.append({'width': width or 256, 'height': height or 256, 'bpp': bits,
                       'resource_id': resource_id, 'payload_sha256': hashlib.sha256(image).hexdigest()})
        offset += size
    content = struct.pack('<HHH', 0, 1, count) + b''.join(headers) + b''.join(images)
    target = SOURCE / f'xp-group-{group_id}-{language}.ico'
    target.write_bytes(content)
    image = Image.open(io.BytesIO(content))
    # Explicitly choose maximum size and color depth, rather than the first
    # same-size 4-bit entry that Pillow may otherwise select.
    chosen = max(frames, key=lambda row: (row['width'] * row['height'], row['bpp']))
    size = (chosen['width'], chosen['height'])
    rgba = image.ico.getimage(size, chosen['bpp']).convert('RGBA')
    png = SOURCE / f'xp-group-{group_id}-{language}.png'
    rgba.save(png)
    result.append({'group_id': group_id, 'language': language, 'count': count,
                   'ico': target.name, 'png': png.name, 'frames': frames,
                   'ico_sha256': hashlib.sha256(content).hexdigest(),
                   'chosen_width': size[0], 'chosen_height': size[1], 'chosen_bpp': chosen['bpp']})
(SOURCE / 'xp-icons.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')
print(json.dumps(result, indent=2))
