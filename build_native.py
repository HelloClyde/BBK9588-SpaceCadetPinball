"""Build the original Space Cadet engine for the freestanding BBK 9588 ABI."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import sys

HERE = Path(__file__).resolve().parent
SDK_COMMIT = 'f2fcbe020e7e94014c33d7e5c593802e4b1e3034'
SDK = HERE / 'sdk'
NATIVE = HERE / 'native'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', default=os.environ.get('BDA_TOOLCHAIN_PREFIX'))
    parser.add_argument('--compile-only', action='store_true', help='Compile every translation unit without linking')
    args = parser.parse_args()
    actual_sdk = subprocess.check_output(['git', '-C', str(SDK), 'rev-parse', 'HEAD'], text=True).strip()
    if actual_sdk != SDK_COMMIT:
        raise SystemExit(f'Expected SDK {SDK_COMMIT}, got {actual_sdk}')
    sys.path.insert(0, str(SDK))
    from bda_packer.build import ENTRY_OFFSET, ENTRY_VA, ICON_START, ICON_SIZES, build_icons, find_tool
    from bda_packer.header import BdaHeaderFields, write_header
    from bda_packer.validate import validate_bda
    toolchain = SDK / '.toolchain'
    if not (toolchain / 'bin').is_dir():
        versioned = toolchain / 'g++-mipsel-none-elf-15.2.0'
        if versioned.is_dir():
            toolchain = versioned
    prefix = args.prefix or str(toolchain / 'bin/mipsel-none-elf-')
    cc, cxx = find_tool(prefix, 'gcc'), find_tool(prefix, 'g++')
    objcopy = find_tool(prefix, 'objcopy')
    out = HERE / 'build-native'
    out.mkdir(exist_ok=True)
    linker = out / 'space-cadet.ld'
    linker.write_text(f'''ENTRY(bda_start)
SECTIONS {{
 . = 0x{ENTRY_VA:x};
 .text : {{ KEEP(*(.text.bda_start)) *(.text.bda_main) *(.text*) }}
 .rodata : {{ *(.rodata*) }}
 .data : {{ *(.data*) *(.sdata*) }}
 . = ALIGN(16);
 .init_array : {{ __init_array_start = .; KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*))) KEEP(*(.init_array)) __init_array_end = .; }}
 .ctors : {{ __ctors_start = .; KEEP(*(SORT(.ctors.*))) KEEP(*(.ctors)) __ctors_end = .; }}
 . = ALIGN(16);
 __bss_start = .;
 .bss (NOLOAD) : {{ *(.bss*) *(.sbss*) *(COMMON) . = ALIGN(16); }}
 __bss_end = .;
 /DISCARD/ : {{ *(.MIPS.abiflags) *(.reginfo) *(.comment) *(.eh_frame*) }}
}}
''', encoding='ascii')
    flags = ['-EL', '-march=mips32', '-msoft-float', '-mno-abicalls', '-G0', '-fno-pic',
             '-Os', '-ffreestanding', '-fno-builtin', '-ffunction-sections', '-fdata-sections',
             '-Wall', '-Wextra', '-Wno-misleading-indentation', '-DNDEBUG', '-DSCP_BBK9588=1',
             '-I', str(NATIVE / 'include'), '-I', str(SDK / 'sdk/include'),
             '-I', str(NATIVE), '-I', str(NATIVE / 'engine')]
    # All retained simulation/rules/resource units are compiled. Desktop-only
    # adapters are supplied by native/*.cpp instead of linking SDL or ImGui.
    excluded = {'SpaceCadetPinball', 'gdrv', 'winmain', 'options', 'fullscrn', 'Sound', 'midi',
                'DebugOverlay', 'font_selection', 'high_score', 'EmbeddedData'}
    engine = sorted(p for p in (NATIVE / 'engine').glob('*.cpp')
                    if p.stem not in excluded and not p.stem.startswith(('imgui', 'imstb')))
    if not engine:
        raise RuntimeError('Stage the original engine sources in native/engine first')
    adapters = sorted(p for p in NATIVE.glob('*.cpp') if not p.stem.startswith('test_'))
    runtime = sorted(p for p in NATIVE.glob('*.c') if not p.stem.startswith('test_'))
    sources = engine + adapters + runtime + [NATIVE / 'startup.S']
    objects = []
    errors = []
    for index, source in enumerate(sources):
        obj = out / f'{index:02d}-{source.stem}.o'
        is_cpp = source.suffix == '.cpp'
        extra = ['-std=c++11', '-fno-exceptions', '-fno-rtti', '-fno-threadsafe-statics'] if is_cpp else ['-std=c11'] if source.suffix == '.c' else []
        result = subprocess.run([cxx if is_cpp else cc, *flags, *extra, '-c', str(source), '-o', str(obj)])
        if result.returncode:
            errors.append(str(source.relative_to(HERE)))
        objects.append(str(obj))
    if errors:
        raise SystemExit('Compilation failed: ' + ', '.join(errors))
    if args.compile_only:
        print(f'Compiled {len(objects)} original engine/runtime units')
        return
    elf, raw = out / 'SpaceCadet9588.elf', out / 'SpaceCadet9588.bin'
    subprocess.run([cxx, *flags, '-nostdlib', '-Wl,--gc-sections', '-Wl,--build-id=none',
                    f'-Wl,-T,{linker}', f'-Wl,-Map,{out / "SpaceCadet9588.map"}',
                    *objects, '-lgcc', '-o', str(elf)], check=True)
    # This bare-metal GCC emits legacy .ctors. Do not let section GC silently
    # remove the global component-tag constructors: their vtables stay zero
    # until first use, which otherwise faults after the entire table loads.
    nm = find_tool(prefix, 'nm')
    symbols = {}
    for line in subprocess.check_output([nm, '-n', str(elf)], text=True).splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2] in ('__init_array_start', '__init_array_end', '__ctors_start', '__ctors_end'):
            symbols[parts[2]] = int(parts[0], 16)
    constructor_bytes = (symbols.get('__init_array_end', 0)-symbols.get('__init_array_start', 0)
                         + symbols.get('__ctors_end', 0)-symbols.get('__ctors_start', 0))
    if constructor_bytes <= 0 or constructor_bytes % 4:
        raise RuntimeError('C++ global constructor table is missing or malformed')
    subprocess.run([objcopy, '-O', 'binary', str(elf), str(raw)], check=True)
    payload = bytearray(ENTRY_OFFSET)
    payload[ICON_START:ENTRY_OFFSET] = build_icons(HERE / 'original_icon.png', (0, 0, 0))
    payload.extend(raw.read_bytes())
    payload.extend(bytes((-len(payload)) % 4))
    write_header(payload, BdaHeaderFields(category=4, file_size_minus_4=len(payload)-4,
                 entry_offset=ENTRY_OFFSET, icon_start=ICON_START,
                 icon0_size=ICON_SIZES[0], icon1_size=ICON_SIZES[1],
                 icon2_size=ICON_SIZES[2], icon3_size=ICON_SIZES[3]), '太空弹球')
    bda = HERE / 'SpaceCadet9588.bda'
    bda.write_bytes(payload)
    report = validate_bda(bda)
    if not report['ok']:
        raise RuntimeError(report['errors'])
    metadata = {'upstream_commit': 'cb9b7b886244a27773f66b0b19fdc2998392565e',
                'bda': bda.name, 'size': len(payload), 'sha256': hashlib.sha256(payload).hexdigest(),
                'simulation': 'original SpaceCadetPinball engine', 'units': len(objects),
                'global_constructor_entries': constructor_bytes // 4,
                'architecture': 'MIPS32 little-endian software floating point',
                'sdk_commit': actual_sdk,
                'engine_payload_sha256': hashlib.sha256(payload[ENTRY_OFFSET:]).hexdigest(),
                'toolchain': subprocess.check_output([cc, '--version'], text=True).splitlines()[0]}
    (out / 'build.json').write_text(json.dumps(metadata, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(metadata, ensure_ascii=True, indent=2))


if __name__ == '__main__':
    main()
