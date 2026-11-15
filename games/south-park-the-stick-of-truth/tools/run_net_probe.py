"""Gate 0 runner: does the shipped Onyx netcode have a host path, a client path, and a second instance?

Every launch is guarded, under the machine-wide launch lock (taken here, released in finally):

    py -3.14 -B run_net_probe.py --mode single          # one instance, menu only
    py -3.14 -B run_net_probe.py --mode pair            # A plain, B with -multiple_instance

Read-only with respect to the game: the save guard redirects the profile into the run folder,
the net probe only observes. There is no -connect here: the command-line route is dead in this
build (see NOTES.md). A client needs a console `connect` or a direct call, which is not on this branch.

Run folders go to local/game-runs/<uuid>/ (git-ignored). A run is valid only if
original_profile_unchanged, guard_integrity and original_exe_hash_still_matches are all true.
"""
import argparse
import ctypes
import hashlib
import json
import os
import sys
import time
import uuid
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
try:
    import frida  # noqa: F401 - guard_multi imports it too; PYTHONPATH must point at the runtime libs
    from guard_multi import EXE, EXPECTED, GAME, ORIGINAL_PROFILE, Instance, snapshot
except ModuleNotFoundError as error:
    raise SystemExit('Missing frida or guard module (' + str(error) + '). '
                     'Set PYTHONPATH to the folder that holds the pinned frida wheel.')
from capture_game_window import capture_game_window
import launch_lock

RUNS = ROOT.parents[2] / 'local' / 'game-runs'
OWNER = os.environ.get('SP_AGENT', 'step01')
WINDOW_TITLE = 'South Park\u2122: The Stick of Truth\u2122'
NET_KEYS = ['wsaStartup', 'socket', 'bind', 'listen', 'accept', 'connect', 'wsaConnect', 'sendto',
            'send', 'recv', 'closesocket']


def inventory(root):
    return {path.relative_to(root).as_posix(): path.stat().st_size
            for path in sorted(root.rglob('*')) if path.is_file()}


def summarise(instance_result):
    telemetry = instance_result.get('telemetry') or {}
    net = telemetry.get('net') or {}
    counters = net.get('counters') or {}
    steam = {key: value for key, value in counters.items()
             if key.startswith('Steam') or key.startswith('SteamAPI') or key.startswith('SteamGameServer')}
    return {
        'alive_at_end': instance_result['alive_at_end'],
        'net': {key: counters.get(key, 0) for key in NET_KEYS},
        'steam_counters': steam,
        'connect': (net.get('details') or {}).get('connect', []),
        'wsaConnect': (net.get('details') or {}).get('wsaConnect', []),
        'bind': (net.get('details') or {}).get('bind', []),
        'listen': (net.get('details') or {}).get('listen', []),
        'steam_details': (net.get('details') or {}).get('steam', []),
        'interfaces': net.get('interfaces', []),
        'guard_error': (telemetry.get('guard') or {}).get('error'),
        'net_error': (telemetry.get('net') or {}).get('error'),
    }


def entry_guard_ok(entry):
    events = entry.get('events') or []
    guard_telemetry = (entry.get('telemetry') or {}).get('guard') or {}
    return (not entry['errors'] and 'error' not in guard_telemetry
            and any(event.get('kind') == 'guard-ready' for event in events)
            and not any(event.get('kind') == 'guard-error' for event in events))


def run(mode, seconds):
    if hashlib.sha256(EXE.read_bytes()).hexdigest() != EXPECTED:
        raise SystemExit('Unexpected game executable; refusing launch')
    if not ORIGINAL_PROFILE.is_dir():
        raise SystemExit('Original profile not found; refusing launch')
    if ctypes.windll.user32.FindWindowW(WINDOW_TITLE, None):
        raise SystemExit('A South Park window already exists; refusing to interfere')
    run_root = RUNS / uuid.uuid4().hex
    run_root.mkdir(parents=True)
    before = snapshot(ORIGINAL_PROFILE)
    game_before = inventory(GAME)
    redirections = []
    for source, label in [
        (Path(r'C:\Program Files (x86)\Steam\logs'), 'steam-logs'),
        (Path.home() / 'AppData/Local/Microsoft/Windows/Caches', 'windows-cache'),
        (Path(r'C:\ProgramData\NVIDIA Corporation\Drs'), 'nvidia-drs')]:
        destination = run_root / 'runtime-files' / label
        destination.mkdir(parents=True, exist_ok=True)
        redirections.append({'from': str(source), 'to': str(destination)})
    result = {'mode': mode, 'seconds': seconds, 'run': str(run_root), 'owner': OWNER,
              'instances': {}, 'errors': []}
    instances = []
    try:
        device = frida.get_local_device()
        instances.append(Instance(device, 'A', [str(EXE), '-window'], run_root / 'A', redirections))
        if mode == 'pair':
            instances.append(Instance(device, 'B', [str(EXE), '-window', '-multiple_instance'],
                                      run_root / 'B', redirections))
        for instance in instances:
            instance.resume()
        time.sleep(seconds)
        for instance in instances:
            entry = {'pid': instance.pid,
                     'alive_at_end': not any(event.get('kind') == 'detached' for event in instance.events),
                     'events': instance.events[:200], 'errors': instance.errors,
                     'telemetry': instance.telemetry()}
            try:
                capture_game_window(instance.pid, run_root / ('game-window-' + instance.name + '.png'))
                entry['screenshot'] = 'game-window-' + instance.name + '.png'
            except Exception as error:  # noqa: BLE001 - the screenshot is evidence, not a gate
                entry['screenshot_error'] = repr(error)
            result['instances'][instance.name] = entry
    except Exception as error:  # noqa: BLE001 - recorded in result.json, the run is then invalid
        result['errors'].append(repr(error))
    finally:
        for instance in instances:
            try:
                device.kill(instance.pid)
            except Exception:  # noqa: BLE001 - already exited or not ours to keep
                pass
        after = snapshot(ORIGINAL_PROFILE)
        game_after = inventory(GAME)
        result['new_game_files'] = {path: size for path, size in game_after.items()
                                    if path not in game_before or game_before[path] != size}
        result['original_profile_unchanged'] = before == after
        result['original_exe_hash_still_matches'] = hashlib.sha256(EXE.read_bytes()).hexdigest() == EXPECTED
        result['guard_integrity'] = bool(result['instances']) and not result['errors'] and all(
            entry_guard_ok(entry) for entry in result['instances'].values())
        result['summary'] = {name: summarise(entry) for name, entry in result['instances'].items()}
        result['event_kinds'] = {name: dict(Counter(event.get('kind') for event in entry.get('events') or []))
                                 for name, entry in result['instances'].items()}
        (run_root / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--mode', choices=['single', 'pair'], default='single')
    parser.add_argument('--seconds', type=float, default=80)
    parser.add_argument('--lock-wait', type=float, default=float(os.environ.get('SP_LOCK_WAIT', '1800')))
    arguments = parser.parse_args()
    if launch_lock.acquire(OWNER, minutes=45, wait=arguments.lock_wait, pid=os.getpid()) != 0:
        raise SystemExit('Another agent holds the launch lock; refusing to launch')
    try:
        outcome = run(arguments.mode, arguments.seconds)
    finally:
        launch_lock.release(OWNER)
    print(json.dumps({'run': outcome['run'], 'report': str(Path(outcome['run']) / 'result.json'),
                      'original_profile_unchanged': outcome['original_profile_unchanged'],
                      'guard_integrity': outcome['guard_integrity'],
                      'original_exe_hash_still_matches': outcome['original_exe_hash_still_matches'],
                      'errors': outcome['errors'],
                      'summary': outcome['summary']}, indent=2, default=str))
