"""Guarded probe: does the shipped build start its Onyx network layer?

Read-only in spirit: it hooks Winsock and Steamworks exports, writes console
scripts only into the isolated test copy, and never opens the original profile
for writing (the guard redirects any such attempt into the run folder).
"""
import ctypes
import hashlib
import json
import os
import shutil
import sys
import uuid
from pathlib import Path

from guard_session import run_guarded
from capture_game_window import capture_game_window
import launch_lock

ROOT = Path(__file__).resolve().parent
GAME = Path(os.environ.get('SP_GAME_DIR', r'D:\coop-work\game-test'))
EXE = GAME / 'South Park - The Stick of Truth.exe'
EXPECTED = '54a83d055f97cad8beeb5aa027c96c0c2aa4a6c36394634270297dd47856a0d6'
ORIGINAL_PROFILE = Path.home() / 'Documents' / 'My Games' / 'South Park - The Stick of Truth'
CONSOLE_DIRS = [GAME / 'global' / 'console_scripts', GAME / 'data' / 'global' / 'console_scripts']
# The engine string table shows global/console_scripts plus "%s/%s", "%s.txt" and
# "unable to exec missing file %s"; the exact startup file name is unknown, so
# each candidate writes a differently named dump and the surviving dump names the
# script that actually ran.
CANDIDATES = ['platform_win.txt', 'init.txt', 'console.txt', 'autoexec.txt']
COMMANDS = ['con_echo_command 1', 'console_toggle', 'find multiplayer', 'find server',
            'find session', 'status', 'multiplayer_local_enabled 1',
            'multiplayer_remote_enabled 1']


def snapshot(root):
    return {path.relative_to(root).as_posix(): {'bytes': path.stat().st_size,
            'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
            for path in sorted(root.rglob('*')) if path.is_file()}


def inventory(root):
    return {path.relative_to(root).as_posix(): path.stat().st_size
            for path in sorted(root.rglob('*')) if path.is_file()}


def install_console_scripts():
    written = []
    for directory in CONSOLE_DIRS:
        directory.mkdir(parents=True, exist_ok=True)
        for name in CANDIDATES:
            marker = 'probe_' + Path(name).stem + '.txt'
            body = '\n'.join(COMMANDS + ['write_file ' + marker]) + '\n'
            (directory / name).write_text(body, encoding='ascii')
            written.append(str(directory / name))
    return written


if __name__ == '__main__':
    if hashlib.sha256(EXE.read_bytes()).hexdigest() != EXPECTED:
        raise SystemExit('Unexpected game executable; refusing launch')
    if not ORIGINAL_PROFILE.is_dir():
        raise SystemExit('Expected original profile not found; refusing launch')
    if ctypes.windll.user32.FindWindowW('South Park\u2122: The Stick of Truth\u2122', None):
        raise SystemExit('A South Park window already exists; refusing to interfere')
    owner = os.environ.get('SP_AGENT', 'run_net_probe')
    lock_held = False
    if '--no-lock' not in sys.argv:
        if launch_lock.acquire(owner, minutes=30, wait=float(os.environ.get('SP_LOCK_WAIT', '1800')),
                               pid=os.getpid()) != 0:
            raise SystemExit('Another agent holds the launch lock; refusing to launch')
        lock_held = True
    run = ROOT / 'game-runs' / uuid.uuid4().hex
    run.mkdir(parents=True)
    backup = run / 'original-profile-backup'
    profile = run / 'test-profile'
    before = snapshot(ORIGINAL_PROFILE)
    shutil.copytree(ORIGINAL_PROFILE, backup)
    shutil.copytree(backup, profile)
    if snapshot(backup) != before or snapshot(ORIGINAL_PROFILE) != before:
        raise SystemExit('Profile changed during backup; refusing launch')
    config_backup = run / 'test-config-backup'
    config_backup.mkdir()
    for path in GAME.glob('*.ini'):
        shutil.copy2(path, config_backup / path.name)
    written_scripts = install_console_scripts()
    launch_args = [str(EXE), '-window']
    if '--exec' in sys.argv:
        body = '\n'.join(COMMANDS + ['write_file probe_dump.txt']) + '\n'
        for directory in CONSOLE_DIRS:
            directory.mkdir(parents=True, exist_ok=True)
            (directory / 'probe_console.txt').write_text(body, encoding='ascii')
            written_scripts.append(str(directory / 'probe_console.txt'))
        launch_args.extend(['-exec', 'probe_console'])
    game_before = inventory(GAME)
    redirections = []
    for source, label in [
        (Path(r'C:\Program Files (x86)\Steam\logs'), 'steam-logs'),
        (Path.home() / 'AppData/Local/Microsoft/Windows/Caches', 'windows-cache'),
        (Path(r'C:\ProgramData\NVIDIA Corporation\Drs'), 'nvidia-drs'),
    ]:
        destination = run / 'runtime-files' / label
        destination.mkdir(parents=True)
        if label == 'nvidia-drs' and source.is_dir():
            for item in source.iterdir():
                if item.is_file() and item.stat().st_size < 20 * 1024 * 1024:
                    shutil.copy2(item, destination / item.name)
        redirections.append({'from': str(source), 'to': str(destination)})
    temporary = run / 'runtime-files' / 'temp'
    temporary.mkdir(parents=True)
    config = {'originalSaves': str(ORIGINAL_PROFILE), 'testSaves': str(profile),
              'writeRoots': [str(GAME), str(run)], 'redirections': redirections,
              'nativeCandidates': [], 'traceNatives': [], 'scriptSignatures': [],
              'blockRemoteStorage': False,
              'logPaths': ['console_scripts', 'probe_console']}
    result = {}
    try:
        result = run_guarded(launch_args, config, duration=50, cwd=GAME,
                             environment={'SteamAppId': '213670', 'SteamGameId': '213670',
                                          'TEMP': str(temporary), 'TMP': str(temporary)},
                             observer=lambda pid, script: capture_game_window(pid, run / 'game-window.png'),
                             observation_delay=0,
                             extra_scripts=['net_probe.js'])
    except Exception as exc:
        result['launcher_error'] = repr(exc)
    finally:
        result.pop('extra_scripts', None)
        after = snapshot(ORIGINAL_PROFILE)
        game_after = inventory(GAME)
        new_files = {path: size for path, size in game_after.items() if path not in game_before}
        dumps = {}
        for path in list(GAME.rglob('probe_*.txt')) + list(run.rglob('probe_*.txt')):
            dumps[str(path)] = path.read_text(encoding='utf-8', errors='replace')[:20000]
        result['console_scripts_written'] = written_scripts
        result['probe_dumps'] = dumps
        result['new_game_files'] = new_files
        result['guard_integrity'] = not result.get('errors') and not any(
            e.get('kind') == 'guard-error' for e in result.get('events', []))
        result['original_profile_unchanged'] = before == after
        result['original_exe_hash_still_matches'] = hashlib.sha256(EXE.read_bytes()).hexdigest() == EXPECTED
        (run / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
        if lock_held:
            launch_lock.release(owner)
    net = (result.get('extra_telemetry') or {}).get('net_probe.js', {})
    summary = {
        'run': str(run),
        'report': str(run / 'result.json'),
        'original_profile_unchanged': result.get('original_profile_unchanged'),
        'guard_integrity': result.get('guard_integrity'),
        'launcher_error': result.get('launcher_error'),
        'net_counter_keys': sorted((net.get('counters') or {}).keys()),
        'net_counters': net.get('counters'),
        'binds': (net.get('details') or {}).get('bind'),
        'connects': (net.get('details') or {}).get('connect'),
        'probe_dumps': sorted(result.get('probe_dumps') or {}),
        'new_game_files': result.get('new_game_files'),
        'observation': result.get('observation'),
    }
    print(json.dumps(summary, indent=2))
