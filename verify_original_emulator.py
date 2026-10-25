"""Verify the original Space Cadet engine and assets in a fresh isolated NAND.

Only public emulator input, screen and file-export endpoints are used at run
time. Host-side NAND deployment happens before starting this owned instance.
No existing emulator process or user NAND is modified or stopped.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import time
import urllib.error
import zipfile

from emulator_client import Frontend, pixel_changes

HERE = Path(__file__).resolve().parent
SDK = HERE / 'sdk'


def export_optional(web: Frontend, path: str):
    try:
        return web.request('/api/files/export', path=path)
    except (OSError, urllib.error.URLError, AssertionError):
        return None


def engine_log(web: Frontend, out: Path) -> str:
    content = export_optional(web, '/SCADLOG.TXT')
    if not content:
        return ''
    text = content.decode('utf8', errors='replace')
    (out / 'SCADLOG.TXT').write_text(text, encoding='utf8')
    return text


def held_response(web: Frontend, name: str, before: bytes, keys=(), touch=None,
                  box=None, minimum=10, timeout=4, minimum_hold=0):
    """Keep levels applied while waiting for a frame showing the actual action."""
    try:
        for key in keys:
            web.request('/api/key', True, code=key, down=1)
        if touch:
            web.request('/api/touch', True, x=touch[0], y=touch[1], down=1)
        if minimum_hold:
            time.sleep(minimum_hold)
        response = web.shot_changed(name, before, box, minimum, timeout)
        changes = pixel_changes(before, response, box)
        assert changes > minimum, f'{name}: no visible response ({changes} changed pixels)'
        return response, changes
    finally:
        for key in keys:
            web.request('/api/key', True, code=key, down=0)
        if touch:
            web.request('/api/touch', True, x=touch[0], y=touch[1], down=0)
        time.sleep(.25)


def wait_changed(web: Frontend, name: str, before: bytes, minimum: int, box=None, timeout=4):
    image = web.shot_changed(name, before, box, minimum, timeout)
    changes = pixel_changes(before, image, box)
    assert changes > minimum, f'{name}: unchanged ({changes} pixels)'
    return image, changes


def deploy_assets(python: Path, emulator: Path, env: dict, nand: Path, run: Path, data: Path, pack_only=False):
    """Use the emulator's validated offline remove-and-recreate writer."""
    helper = run / 'deploy_original_assets.py'
    helper.write_text('''from pathlib import Path
import hashlib, json, struct, sys
from emu.qemu.nand_fs import mutate_nand_files, replace_fat_file, fat_file_clusters
nand, data = map(Path, sys.argv[1:3])
manifest = json.loads((data / 'provenance.json').read_text(encoding='utf8'))['files']
expected = {}
for record in manifest:
    name = record['name']
    assert Path(name).name == name and '/' not in name and '\\\\' not in name
    source = data / name
    assert source.is_file() and source.stat().st_size == record['size']
    assert hashlib.sha256(source.read_bytes()).hexdigest() == record['sha256']
    expected['/Pinball/' + name] = record
assert len(expected) == 66
packed = (data / 'PINBALL.PAK').read_bytes()
magic, count, reserved = struct.unpack_from('<8sII', packed)
assert magic == b'SCPAK01\\0' and count == 66 and 16 + count * 40 <= len(packed)
found = set()
for index in range(count):
    name_bytes, offset, size = struct.unpack_from('<32sII', packed, 16 + index * 40)
    assert b'\\0' in name_bytes and offset >= 16 + count * 40 and offset % 4 == 0
    assert offset <= len(packed) and size <= len(packed) - offset
    name = name_bytes.split(b'\\0', 1)[0].decode('ascii')
    assert name not in found
    found.add(name)
    record = expected['/Pinball/' + name]
    assert size == record['size'] and hashlib.sha256(packed[offset:offset + size]).hexdigest() == record['sha256']
pack_record = {'name': 'PINBALL.PAK', 'size': len(packed), 'sha256': hashlib.sha256(packed).hexdigest()}
expected['/Pinball/PINBALL.PAK'] = pack_record
def digest(fs, name):
    with fs.openbin(name, 'r') as stream:
        return hashlib.sha256(stream.read()).hexdigest()
original = {}
def operation(fs):
    original['c200'] = digest(fs, '/系统/数据/C200.bin')
    fs.makedirs('/Pinball', recreate=True)
    if fs.isfile('/SCADLOG.TXT'):
        fs.remove('/SCADLOG.TXT')
    for target, record in expected.items():
        if sys.argv[3] == 'pack' and record['name'] != 'PINBALL.PAK':
            continue
        with (data / record['name']).open('rb') as incoming:
            replace_fat_file(fs, target, incoming)
def validator(fs):
    assert digest(fs, '/系统/数据/C200.bin') == original['c200']
    for target, record in expected.items():
        assert fs.getsize(target) == record['size']
        assert digest(fs, target) == record['sha256']
        fat_file_clusters(fs, target)
mutate_nand_files(nand, operation, validator=validator)
print(json.dumps({'ok': True, 'asset_count': len(expected) - 1, 'resource_pack': pack_record, 'c200_sha256': original['c200'], 'files': expected}, indent=2))
''', encoding='utf8')
    result = subprocess.run([str(python), str(helper), str(nand), str(data), 'pack' if pack_only else 'all'],
                            cwd=run, env=env, capture_output=True, text=True,
                            encoding='utf8', creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
    (run / 'assets-deploy.stderr.txt').write_text(result.stderr, encoding='utf8')
    assert result.returncode == 0, f'Asset deployment failed:\n{result.stderr}'
    (run / 'assets-deploy.json').write_text(result.stdout, encoding='utf8')
    return json.loads(result.stdout)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emulator-root', type=Path, required=True)
    parser.add_argument('--bda', type=Path, default=HERE / 'SpaceCadet9588.bda')
    parser.add_argument('--data', type=Path, default=HERE / 'original-data')
    parser.add_argument('--port', type=int, default=8022)
    parser.add_argument('--boot-delay', type=float, default=14)
    parser.add_argument('--load-timeout', type=float, default=180)
    parser.add_argument('--intro-wait', type=float, default=8, help='Original new-game introduction/ball feed delay')
    parser.add_argument('--require-ball-ready', action='store_true', help='Wait for the native BALL READY log before plunger testing')
    parser.add_argument('--play-seconds', type=int, default=18)
    parser.add_argument('--require-score', action='store_true', help='Require a nonzero persistent original-engine score')
    parser.add_argument('--prepare-only', action='store_true', help='Prepare isolated NAND but do not start an emulator')
    parser.add_argument('--reuse-fixture', type=Path, help='Reuse an already stopped NAND created by this script under qa/original/runtime')
    parser.add_argument('--leave-running', action='store_true')
    args = parser.parse_args()
    emulator, bda, data = args.emulator_root.resolve(), args.bda.resolve(), args.data.resolve()
    python, qemu = emulator / 'python/python.exe', emulator / 'bin/bbk9588-qemu-system-mipsel.exe'
    archive = emulator / 'bbk9588_nand-v1.2.0.zip'
    for required in (python, qemu, bda, archive, data / 'provenance.json'):
        assert required.is_file(), f'Missing {required}'
    with socket.socket() as guard:
        guard.bind(('127.0.0.1', args.port))
    out = HERE / 'qa/original'
    out.mkdir(parents=True, exist_ok=True)
    if args.reuse_fixture:
        nand = args.reuse_fixture.resolve()
        runtime_root = (out / 'runtime').resolve()
        assert runtime_root in nand.parents and nand.name == 'bbk9588_nand.bin', 'Reuse is restricted to this script\'s own isolated runtime'
        run = nand.parent
        assert nand.is_file() and (run / 'assets-deploy.json').is_file(), 'Unrecognized test fixture'
        assets = json.loads((run / 'assets-deploy.json').read_text(encoding='utf8'))
        assert assets.get('ok') and assets.get('asset_count') == 66
    else:
        run = out / 'runtime' / time.strftime('%Y%m%d-%H%M%S')
        run.mkdir(parents=True, exist_ok=False)
        nand = run / 'bbk9588_nand.bin'
        with zipfile.ZipFile(archive) as source:
            images = [item for item in source.infolist() if item.filename.lower().endswith('.bin')]
            assert len(images) == 1
            with source.open(images[0]) as incoming, nand.open('wb') as outgoing:
                shutil.copyfileobj(incoming, outgoing, 4 * 1024 * 1024)
    env = os.environ.copy()
    env['PYTHONNOUSERSITE'], env['PYTHONPATH'] = '1', str(emulator)
    flags = getattr(subprocess, 'CREATE_NO_WINDOW', 0)
    prepared = subprocess.run([str(python), str(SDK / 'scripts/replace_emulator_bda.py'),
                               '--emulator-root', str(emulator), '--nand', str(nand), '--bda', str(bda)],
                              env=env, cwd=run, capture_output=True, text=True, encoding='utf8',
                              creationflags=flags)
    (run / 'bda-deploy.stderr.txt').write_text(prepared.stderr, encoding='utf8')
    assert prepared.returncode == 0, f'BDA deployment failed:\n{prepared.stderr}'
    (run / 'bda-deploy.json').write_text(prepared.stdout, encoding='utf8')
    deployment = json.loads(prepared.stdout)
    assets = deploy_assets(python, emulator, env, nand, run, data, pack_only=bool(args.reuse_fixture))
    print(f'Prepared original-engine NAND: {nand}', flush=True)
    if args.prepare_only:
        return 0

    bootstrap = ('from pathlib import Path;import emu.web.frontend_state as state;'
                 f'state.BUILD=Path({str(run / "build")!r});'
                 'from emu.web.frontend import main;raise SystemExit(main())')
    log = (run / 'frontend.log').open('w', encoding='utf8')
    proc = subprocess.Popen([str(python), '-u', '-c', bootstrap,
                             '--boot-mode', 'nand', '--qemu', str(qemu), '--nand-image', str(nand),
                             '--host', '127.0.0.1', '--port', str(args.port), '--quiet'],
                            cwd=run, env=env, stdout=log, stderr=subprocess.STDOUT, creationflags=flags)
    web = Frontend(args.port, out)
    report = {'result': 'FAIL', 'bda_sha256': deployment['sha256'],
              'bda_size': deployment['size'], 'asset_count': assets['asset_count'], 'resource_pack': assets['resource_pack'],
              'nand': str(nand), 'frontend_pid': proc.pid, 'base': web.base, 'checks': {}, 'saved_scores': []}
    try:
        deadline = time.monotonic() + 60
        while True:
            if proc.poll() is not None:
                raise RuntimeError(f'Frontend exited; inspect {run / "frontend.log"}')
            try:
                status = web.request('/api/status', timeout=2)
                if status.get('running'):
                    break
            except (OSError, urllib.error.URLError):
                pass
            assert time.monotonic() < deadline, 'Emulator startup timeout'
            time.sleep(.5)
        assert Path(status['nand_image']).resolve() == nand
        time.sleep(args.boot_delay)
        menu = web.shot('00-firmware-menu')
        web.touch(198, 84)
        time.sleep(2)
        web.touch(120, 100)
        launched_at = time.monotonic()
        deadline = time.monotonic() + args.load_timeout
        last_log, last_progress, diagnose_due = '', launched_at, launched_at + 20
        while True:
            text = engine_log(web, out)
            web.shot('01-loading')
            assert 'PANIC:' not in text, f'Original engine panic:\n{text}'
            if text != last_log:
                last_log, last_progress = text, time.monotonic()
            if 'READY TABLE' in text:
                break
            if time.monotonic() >= diagnose_due and time.monotonic() - last_progress >= 15:
                # Full status asks the existing frontend HMP connection for a
                # read-only register snapshot. No register/memory writes, hooks
                # or engine calls are injected by this test.
                samples = []
                for _ in range(2):
                    sample = web.request('/api/status', detail='full')['qemu'].get('register_sample') or {}
                    samples.append(sample)
                    time.sleep(.6)
                report.setdefault('load_register_samples', []).extend(samples)
                (out / 'registers-loading.json').write_text(json.dumps(samples, indent=2), encoding='utf8')
                a, b = samples
                cp0 = b.get('cp0') or {}
                epc = cp0.get('epc')
                native_epc = isinstance(epc, str) and 0x81C00020 <= int(epc, 16) < 0x82000000
                if native_epc and cp0.get('exl') and cp0.get('exception_code') in (4, 5, 6, 7, 10) and (a.get('cp0') or {}).get('epc') == epc:
                    raise AssertionError(f'Persistent guest exception {cp0.get("exception")} at original-engine EPC {epc}; last load stage:\n{text}')
                diagnose_due = time.monotonic() + 30
            assert time.monotonic() < deadline, f'Original table did not load; log:\n{text}'
            assert proc.poll() is None, 'Frontend exited during original resource loading'
            time.sleep(1)
        report['engine_ready_log'] = text
        report['load_wall_seconds'] = round(time.monotonic() - launched_at, 3)
        time.sleep(.5)
        ready = web.shot('02-original-table')
        changes = pixel_changes(menu, ready)
        report['checks']['loaded_table_changed_pixels'] = changes
        assert changes > 15000, 'Original table is not visibly rendered'
        print('Original table ready', flush=True)
        # Original Space Cadet has a launch sequence before feeding its first
        # ball. A plunger pulse during that introduction cannot launch a ball.
        if args.require_ball_ready:
            deadline = time.monotonic() + 120
            while True:
                text = engine_log(web, out)
                assert 'PANIC:' not in text, f'Engine panic before ball feed:\n{text}'
                if 'BALL_READY' in text or 'BALL READY' in text:
                    report['ball_ready_log'] = text
                    report['ball_ready_wall_seconds'] = round(time.monotonic() - launched_at, 3)
                    break
                assert time.monotonic() < deadline, f'Original first-ball feed timed out:\n{text}'
                time.sleep(1)
        else:
            time.sleep(args.intro_wait)
        ready = web.shot('02-original-table-ready-ball')

        # Bumpers and lights may animate independently, so isolate the lower bats.
        flippers, changes = held_response(web, '03-physical-flippers', ready, keys=(6, 7),
                                          box=(20, 225, 220, 286), minimum=20)
        report['checks']['physical_flippers_changed_pixels'] = changes
        time.sleep(.3)
        before_touch = web.shot('04-before-touch')
        touched, changes = held_response(web, '05-touch-flipper', before_touch, touch=(35, 303),
                                        box=(20, 225, 120, 286), minimum=8)
        report['checks']['touch_flipper_changed_pixels'] = changes

        before_plunger = web.shot('06-before-plunger')
        charged, changes = held_response(web, '07-plunger-held', before_plunger, keys=(10,),
                                        box=(20, 18, 238, 286), minimum=5, timeout=5, minimum_hold=3)
        report['checks']['plunger_changed_pixels'] = changes
        time.sleep(.8)
        launched, changes = wait_changed(web, '08-original-gameplay', charged, 8,
                                        box=(20, 18, 238, 286), timeout=6)
        report['checks']['launched_ball_changed_pixels'] = changes

        # User-required portrait layout: the original sidebar appears only in a
        # modal scoreboard and all original table motion pauses while it is open.
        web.key(4, .25)
        popup, changes = wait_changed(web, '09-scoreboard-popup', launched, 1500, timeout=5)
        report['checks']['scoreboard_popup_changed_pixels'] = changes
        time.sleep(.6)
        stable = web.shot('10-scoreboard-stable')
        report['checks']['paused_popup_changed_pixels'] = pixel_changes(popup, stable)
        assert report['checks']['paused_popup_changed_pixels'] == 0, 'Scoreboard did not pause the original engine'
        web.key(10, .25)
        resumed, changes = wait_changed(web, '11-popup-closed', stable, 1500, timeout=5)
        report['checks']['popup_close_changed_pixels'] = changes

        # Top-strip touch opens the same modal; Escape closes it, leaving the game.
        web.touch(120, 5, .25)
        touch_popup, changes = wait_changed(web, '12-touch-scoreboard', resumed, 1500, timeout=5)
        report['checks']['touch_scoreboard_changed_pixels'] = changes
        web.key(9, .25)
        after_popup, changes = wait_changed(web, '13-escape-closes-popup', touch_popup, 1500, timeout=5)
        report['checks']['escape_closes_popup_changed_pixels'] = changes

        # Actual public-input gameplay, without writing engine state or memory.
        play_start_wall = time.monotonic()
        play_start_status = web.request('/api/status')
        for second in range(args.play_seconds):
            side, other = (6, 7) if second % 2 == 0 else (7, 6)
            web.request('/api/key', True, code=other, down=0)
            web.request('/api/key', True, code=side, down=1)
            if second % 5 == 0:
                web.key(10, .75)
            else:
                time.sleep(.85)
            if second == 4:
                web.key(5, .15)
        web.request('/api/key', True, code=6, down=0)
        web.request('/api/key', True, code=7, down=0)
        play_end_wall = time.monotonic()
        play_end_status = web.request('/api/status')
        seconds = play_end_wall - play_start_wall
        start_qemu, end_qemu = play_start_status.get('qemu', {}), play_end_status.get('qemu', {})
        report['gameplay_performance'] = {
            'wall_seconds': round(seconds, 3),
            'lcd_changed_frames': end_qemu.get('frame_chardev_count', 0) - start_qemu.get('frame_chardev_count', 0),
            'lcd_changed_frame_fps': round((end_qemu.get('frame_chardev_count', 0) - start_qemu.get('frame_chardev_count', 0)) / seconds, 3),
            'audio_packets': end_qemu.get('audio_stream_packet_count', 0) - start_qemu.get('audio_stream_packet_count', 0),
            'audio_bytes': end_qemu.get('audio_stream_bytes', 0) - start_qemu.get('audio_stream_bytes', 0),
            'last_audio_stream_error': end_qemu.get('last_audio_stream_error'),
            'last_qemu_error': end_qemu.get('last_error'),
            'guest_ips_at_end': (end_qemu.get('performance') or {}).get('guest_ips'),
        }
        gameplay = web.shot('14-original-gameplay-scored')
        web.key(4, .25)
        final_popup, _ = wait_changed(web, '15-final-scoreboard', gameplay, 1500, timeout=5)
        web.key(10, .25)
        before_exit, _ = wait_changed(web, '16-before-exit', final_popup, 1500, timeout=5)
        web.request('/api/key', True, code=9, down=1)
        time.sleep(.35)
        held = web.shot('17-escape-held')
        # A live table may continue moving while Esc is held; it must retain the
        # native top/bottom UI until release rather than returning early.
        report['checks']['escape_held_ui_changed_pixels'] = pixel_changes(before_exit, held, (0, 294, 240, 320))
        assert report['checks']['escape_held_ui_changed_pixels'] == 0, 'Returned to firmware before Esc release'
        web.request('/api/key', True, code=9, down=0)
        exited, changes = wait_changed(web, '18-exit-to-firmware', before_exit, 15000, timeout=8)
        report['checks']['exit_changed_pixels'] = changes
        text = engine_log(web, out)
        report['engine_final_log'] = text
        assert 'PANIC:' not in text, f'Engine panic during play/cleanup:\n{text}'

        for slot in range(2):
            record = export_optional(web, f'/Pinball/SCORE{slot}.SAV')
            if not record:
                report['saved_scores'].append({'slot': slot, 'present': False})
                continue
            (out / f'SCORE{slot}.SAV').write_bytes(record)
            assert len(record) == 20, f'Bad saved original score length {len(record)}'
            magic, version, best, sequence, checksum = struct.unpack('<5I', record)
            expected = 2166136261
            for value in (magic, version, best, sequence):
                expected = ((expected ^ value) * 16777619) & 0xffffffff
            assert magic == 0x53435031 and version == 1 and expected == checksum
            report['saved_scores'].append({'slot': slot, 'present': True, 'best': best, 'sequence': sequence})
        if args.require_score:
            assert any(item.get('best', 0) > 0 for item in report['saved_scores']), 'Gameplay did not earn a persistent nonzero score'
        report['status'] = web.request('/api/status')
        assert report['status'].get('running'), 'Firmware unexpectedly stopped'
        report['result'] = 'PASS'
        print(json.dumps({k: v for k, v in report.items() if k != 'status'}, ensure_ascii=False, indent=2), flush=True)
        return 0
    except Exception as exc:
        report['error'] = f'{type(exc).__name__}: {exc}'
        try:
            web.shot('failure')
            report['engine_final_log'] = engine_log(web, out)
            report['status'] = web.request('/api/status')
        except Exception:
            pass
        raise
    finally:
        (out / 'emulator-result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
        if not args.leave_running:
            try:
                web.request('/api/shutdown', True, timeout=5)
                proc.wait(timeout=20)
            except Exception:
                proc.terminate()
                try:
                    proc.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    proc.kill()
        log.close()


if __name__ == '__main__':
    raise SystemExit(main())
