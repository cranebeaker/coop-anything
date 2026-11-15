"""Two game instances on one PC, both guarded, to test the client path.

Instance A: plain launch (candidate host). Instance B: `-multiple_instance` plus
`-connect <addr>` (candidate client). Both run under the save guard and the
read-only network probe, each with its own redirected profile.

    py -3.14 -B guard_multi.py --connect 127.0.0.1:2005
"""
import argparse
import hashlib
import json
import os
import shutil
import sys
import threading
import time
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / 'runtime-libs'))
import frida

GAME = Path(os.environ.get('SP_GAME_DIR', r'D:\coop-work\game-test'))
EXE = GAME / 'South Park - The Stick of Truth.exe'
EXPECTED = '54a83d055f97cad8beeb5aa027c96c0c2aa4a6c36394634270297dd47856a0d6'
ORIGINAL_PROFILE = Path.home() / 'Documents' / 'My Games' / 'South Park - The Stick of Truth'
DURATION = float(os.environ.get('SP_TWO_DURATION', '45'))


def snapshot(root):
    return {path.relative_to(root).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted(root.rglob('*')) if path.is_file()}


class Instance:
    def __init__(self, device, name, argv, run, redirections):
        self.name = name
        self.run = run
        self.events = []
        self.errors = []
        profile = run / 'test-profile'
        backup = run / 'original-profile-backup'
        shutil.copytree(ORIGINAL_PROFILE, backup)
        shutil.copytree(backup, profile)
        config = {'originalSaves': str(ORIGINAL_PROFILE), 'testSaves': str(profile),
                  'writeRoots': [str(GAME), str(run)], 'redirections': redirections,
                  'nativeCandidates': [], 'traceNatives': [], 'scriptSignatures': [],
                  'blockRemoteStorage': False, 'logPaths': []}
        temporary = run / 'runtime-files' / 'temp'
        temporary.mkdir(parents=True, exist_ok=True)
        environment = {'SteamAppId': '213670', 'SteamGameId': '213670',
                       'TEMP': str(temporary), 'TMP': str(temporary)}
        self.pid = device.spawn(argv, cwd=str(GAME), stdio='pipe', env=environment)
        self.session = device.attach(self.pid)
        self.session.on('detached', lambda reason, crash=None: self.events.append(
            {'kind': 'detached', 'reason': reason, 'crash': str(crash) if crash else None}))
        guard = self.session.create_script(
            (ROOT / 'save_guard.js').read_text(encoding='utf-8').replace('__CONFIG__', json.dumps(config)),
            name='guard-' + name)
        guard.on('message', self._message)
        guard.load()
        probe = self.session.create_script((ROOT / 'net_probe.js').read_text(encoding='utf-8'),
                                           name='net-' + name)
        probe.on('message', self._message)
        probe.load()
        self.guard = guard
        self.probe = probe

    def _message(self, message, data):
        if message['type'] == 'send':
            payload = dict(message['payload'])
            payload['instance'] = self.name
            self.events.append(payload)
        elif message['type'] == 'error':
            self.errors.append({'instance': self.name, 'error': message.get('description')})

    def resume(self):
        frida.get_local_device().resume(self.pid)

    def telemetry(self):
        output = {}
        for key, script in (('guard', self.guard), ('net', self.probe)):
            try:
                output[key] = script.exports_sync.telemetry()
            except Exception as error:  # noqa: BLE001 - report, do not fail the run
                output[key] = {'error': repr(error)}
        return output

    def stop(self):
        try:
            frida.get_local_device().kill(self.pid)
        except frida.ProcessNotFoundError:
            pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--connect', default='127.0.0.1:2005')
    arguments = parser.parse_args()
    if hashlib.sha256(EXE.read_bytes()).hexdigest() != EXPECTED:
        raise SystemExit('Unexpected game executable; refusing launch')
    if not ORIGINAL_PROFILE.is_dir():
        raise SystemExit('Original profile not found; refusing launch')
    # The pair run is still a launch: it must hold the machine-wide lease like every other one.
    import atexit

    import launch_lock

    owner = os.environ.get('SP_AGENT', 'guard-multi')
    if launch_lock.acquire(owner, minutes=20,
                           wait=float(os.environ.get('SP_LOCK_WAIT', '600')),
                           pid=os.getpid()) != 0:
        raise SystemExit('Another agent holds the launch lock; refusing to launch')
    atexit.register(lambda: launch_lock.release(owner))
    before = snapshot(ORIGINAL_PROFILE)
    run_root = ROOT / 'game-runs' / ('two-' + uuid.uuid4().hex)
    run_root.mkdir(parents=True)
    redirections = []
    for source, label in [
        (Path(r'C:\Program Files (x86)\Steam\logs'), 'steam-logs'),
        (Path.home() / 'AppData/Local/Microsoft/Windows/Caches', 'windows-cache'),
        (Path(r'C:\ProgramData\NVIDIA Corporation\Drs'), 'nvidia-drs')]:
        destination = run_root / 'runtime-files' / label
        destination.mkdir(parents=True, exist_ok=True)
        redirections.append({'from': str(source), 'to': str(destination)})
    device = frida.get_local_device()
    instances = []
    result = {'connect': arguments.connect, 'instances': {}}
    try:
        host = Instance(device, 'A', [str(EXE), '-window'], run_root / 'A', redirections)
        instances.append(host)
        client = Instance(device, 'B', [str(EXE), '-window', '-multiple_instance',
                                        '-connect', arguments.connect], run_root / 'B', redirections)
        instances.append(client)
        for instance in instances:
            instance.resume()
        time.sleep(DURATION)
        for instance in instances:
            result['instances'][instance.name] = {
                'pid': instance.pid, 'events': instance.events, 'errors': instance.errors,
                'telemetry': instance.telemetry()}
    finally:
        for instance in instances:
            instance.stop()
        after = snapshot(ORIGINAL_PROFILE)
        result['original_profile_unchanged'] = before == after
        result['original_exe_hash_still_matches'] = hashlib.sha256(EXE.read_bytes()).hexdigest() == EXPECTED
        result['report'] = str(run_root / 'result.json')
        (run_root / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    for name, data in result['instances'].items():
        net = (data.get('telemetry') or {}).get('net', {})
        print(name, 'pid', data['pid'], 'counters', (net or {}).get('counters'))
        print('   connects', (net or {}).get('details', {}).get('connect'),
              'binds', (net or {}).get('details', {}).get('bind'),
              'errors', data['errors'])
    print(json.dumps({k: v for k, v in result.items() if k != 'instances'}, indent=2))


if __name__ == '__main__':
    main()
