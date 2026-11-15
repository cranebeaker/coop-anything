"""Machine-wide launch lock for the South Park co-op test install.

One game process set at a time, across agents, branches and worktrees. The lock
lives outside the repository so every worktree sees the same lock, and it is a
lease: the holder says how long it needs and can refresh it.

    py launch_lock.py acquire --owner step01-net --minutes 45 [--wait 1800]
    py launch_lock.py refresh --owner step01-net --minutes 30
    py launch_lock.py release --owner step01-net
    py launch_lock.py status
"""
import argparse
import json
import os
import sys
import time
from pathlib import Path

DEFAULT_ROOT = Path(os.environ.get('LOCALAPPDATA', Path.home())) / 'south-park-coop'
LOCK = Path(os.environ.get('SP_LAUNCH_LOCK', DEFAULT_ROOT / 'launch.lock'))


def read_lock():
    try:
        return json.loads(LOCK.read_text(encoding='utf-8'))
    except (OSError, ValueError):
        return None


def process_alive(pid):
    if not pid:
        return False
    if os.name == 'nt':
        import ctypes
        handle = ctypes.windll.kernel32.OpenProcess(0x1000, False, int(pid))
        if not handle:
            return False
        ctypes.windll.kernel32.CloseHandle(handle)
        return True
    try:
        os.kill(int(pid), 0)
        return True
    except OSError:
        return False


def current_lease():
    data = read_lock()
    if data is None:
        return None
    if float(data.get('expires', 0)) <= time.time():
        return None
    return data


def write_lease(owner, minutes, pid=None, extra=None):
    LOCK.parent.mkdir(parents=True, exist_ok=True)
    payload = {'owner': owner, 'pid': pid or os.getpid(), 'since': time.time(),
               'expires': time.time() + minutes * 60,
               'host': os.environ.get('COMPUTERNAME', '')}
    payload.update(extra or {})
    LOCK.write_text(json.dumps(payload, indent=2), encoding='utf-8')
    return payload


def acquire(owner, minutes, wait, pid=None):
    deadline = time.time() + wait
    LOCK.parent.mkdir(parents=True, exist_ok=True)
    while True:
        try:
            handle = os.open(LOCK, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
            os.close(handle)
            payload = write_lease(owner, minutes, pid)
            print(json.dumps({'acquired': True, **payload}, indent=2))
            return 0
        except FileExistsError:
            held = current_lease()
            if held is None:
                try:
                    LOCK.unlink()
                except OSError:
                    pass
                continue
            if held.get('owner') == owner:
                print(json.dumps({'acquired': True, 'already_held': True, **held}, indent=2))
                return 0
            left = max(0, int(float(held['expires']) - time.time()))
            if time.time() >= deadline:
                print(json.dumps({'acquired': False, 'holder': held, 'seconds_left': left,
                                  'message': 'another agent holds the launch lock'}, indent=2))
                return 2
            print(f'waiting for {held.get("owner")} ({left}s left)', file=sys.stderr)
            time.sleep(5)


def release(owner):
    held = read_lock()
    if held is None:
        try:
            LOCK.unlink()
        except OSError:
            pass
        print(json.dumps({'released': False, 'reason': 'no active lease'}))
        return 0
    if held.get('owner') != owner:
        print(json.dumps({'released': False, 'reason': 'held by another owner', 'holder': held}))
        return 2
    LOCK.unlink(missing_ok=True)
    print(json.dumps({'released': True, 'owner': owner}))
    return 0


def status():
    held = read_lock()
    if held is None:
        print(json.dumps({'held': False, 'path': str(LOCK)}, indent=2))
        return 0
    left = int(float(held['expires']) - time.time())
    print(json.dumps({'held': left > 0, 'seconds_left': left,
                      'recorded_process_alive': process_alive(held.get('pid')),
                      'path': str(LOCK), **held}, indent=2))
    return 0


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['acquire', 'release', 'refresh', 'status'])
    parser.add_argument('--owner', default=os.environ.get('SP_AGENT', 'unknown-agent'))
    parser.add_argument('--minutes', type=float, default=45)
    parser.add_argument('--wait', type=float, default=0)
    parser.add_argument('--pid', type=int, default=None)
    arguments = parser.parse_args()
    if arguments.command == 'acquire':
        raise SystemExit(acquire(arguments.owner, arguments.minutes, arguments.wait, arguments.pid))
    if arguments.command == 'refresh':
        held = current_lease()
        if held is None or held.get('owner') != arguments.owner:
            print(json.dumps({'refreshed': False, 'holder': held}))
            raise SystemExit(2)
        print(json.dumps({'refreshed': True, **write_lease(arguments.owner, arguments.minutes,
                                                           held.get('pid'), {'since': held.get('since')})}))
        raise SystemExit(0)
    if arguments.command == 'release':
        raise SystemExit(release(arguments.owner))
    raise SystemExit(status())
