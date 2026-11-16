"""Step 2 in-VM run: identify the buddy AI update slot and prove the hold hook.

Run this inside the SP-Test VM only. It uses the same isolated-profile harness as the
step-1 probes (`console_exec.Game`, the shared launch lock) and loads Clyde's Fort with
mouse clicks. It then:

  1. `probe` scans the live objects.
  2. `aiscan` verifies the Companion_Basic / Companion_Follow / Companion_Command vtables
     and installs signature-neutral counting hooks on every non-destructor .text slot.
  3. `aistats` twice, one second apart, ranks the slots by calls/second and by whether
     their `this` is the live Companion_Follow state.
  4. Follow baseline: nudge the hero mover +4 X; the buddy must move.
  5. For up to three top candidates, `aihold <id>` and nudge the hero again. The first
     candidate that keeps the buddy within 0.05 of its anchor while the hero moves is
     the update; that is the slot named in the report.
  6. `aihold off` and nudge the hero once more; the buddy should follow again.
  7. The state class is logged on every sample: a Companion_Command / _Command_Hint state
     means the engine owns the buddy and the hold stub yields until the state resets.

Nothing is claimed by the script itself; the pass/fail fields in the JSON report are the
measurement. The mod's `coop_debug.log` holds the [HOOK]/[BUDDY] lines behind them.
"""
import ctypes
import json
import math
import os
import sys
import tempfile
import time
from ctypes import wintypes
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
BRIDGE = ROOT / 'tools' / 'bridge'
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(BRIDGE))

# The VM's Steam install. Point SP_GAME_DIR at an isolated copy if one is used.
os.environ.setdefault('SP_GAME_DIR',
                      r'C:\Program Files (x86)\Steam\steamapps\common\South Park - The Stick of Truth')
# The periodic scan reads a lot of memory during a level load; the explicit `probe` is the
# scan for a fully loaded level (the same switch step 1b used).
os.environ.setdefault('SP_COOP_SCAN', 'off')

import console_exec as ce                      # noqa: E402
import in_game                                 # noqa: E402
import launch_lock                             # noqa: E402
from bridge_client import Bridge               # noqa: E402
from capture_game_window import (              # noqa: E402
    capture_game_window, find_game_window, focus_window)

OWNER = 'step2-hook-vm'
USER = ctypes.windll.user32

LOAD_GAME = (0.844, 0.744)
SAVE_ROW = (0.437, 0.503)
LOAD_BUTTON = (0.738, 0.918)
DIALOG_CONFIRM = (0.430, 0.572)

_waited_for_menu = False


def wait_for_menu(pid, timeout=90.0, threshold=0.85):
    """Wait for the front-end frame; a click one second early is lost (fort_load2)."""
    deadline = time.time() + timeout
    probe = Path(tempfile.gettempdir()) / 'sp-step2-menu.png'
    while time.time() < deadline:
        info = capture_game_window(pid, probe)
        if info.get('captured'):
            grid = info.get('luma_grid') or []
            score = max([in_game.grid_correlation(grid, reference)
                         for reference in in_game.REFERENCES['menu']] or [0.0])
            if score >= threshold:
                return round(score, 4)
        time.sleep(1.5)
    return None


def hover_click(pid, fx, fy, hover=0.6, settle=1.2):
    global _waited_for_menu
    if not _waited_for_menu:
        wait_for_menu(pid)
        _waited_for_menu = True
    window = find_game_window(pid)
    if window is None:
        return {'clicked': False, 'reason': 'no window'}
    focus_window(window['hwnd'])
    point = wintypes.POINT(int(window['width'] * fx), int(window['height'] * fy))
    USER.ClientToScreen(window['hwnd'], ctypes.byref(point))
    USER.SetCursorPos(point.x, point.y)
    time.sleep(hover)
    USER.mouse_event(0x0002, 0, 0, 0, 0)
    time.sleep(0.1)
    USER.mouse_event(0x0004, 0, 0, 0, 0)
    time.sleep(settle)
    return {'clicked': True, 'screen': [point.x, point.y]}


def vec_from(obj):
    if not isinstance(obj, dict) or not obj:
        return None
    try:
        return [float(obj.get('x', 0.0)), float(obj.get('y', 0.0)),
                float(obj.get('z', 0.0))]
    except (TypeError, ValueError):
        return None


def positions(state):
    state = state or {}
    hero = vec_from(state.get('position_live')) or vec_from(state.get('position'))
    buddy_obj = state.get('buddy') or {}
    return (hero, vec_from(buddy_obj.get('position')),
            vec_from(buddy_obj.get('mover_position')),
            buddy_obj.get('state_class'))


def delta(a, b):
    if a is None or b is None:
        return None
    return math.sqrt(sum((a[i] - b[i]) ** 2 for i in range(3)))


def parse_stats(text):
    hooks = []
    state_line = None
    for line in (text or '').splitlines():
        if line.startswith('ID '):
            parts = line.split()
            item = {}
            index = 1
            while index + 1 < len(parts):
                item[parts[index]] = parts[index + 1]
                index += 2
            hooks.append(item)
        elif line.startswith('STATE '):
            state_line = line
    return hooks, state_line


def rank_candidates(hooks, limit=3):
    pool = []
    for hook in hooks:
        try:
            rate = float(hook.get('RATE', '0'))
            calls = int(hook.get('CALLS', '0'))
        except ValueError:
            continue
        if calls <= 0 or rate < 8.0:
            continue
        score = rate
        if 'Companion_Follow' in hook.get('CLASS', ''):
            score += 1000.0
        if hook.get('LABEL', '').startswith('Follow#'):
            score += 100.0
        if 'Companion_Basic' in hook.get('CLASS', ''):
            score += 10.0
        pool.append((score, hook))
    pool.sort(key=lambda pair: pair[0], reverse=True)
    return [hook for _score, hook in pool[:limit]]


def sample(bridge):
    state = bridge.state()
    hero, buddy, mover, state_class = positions(state)
    return {'state': state, 'hero': hero, 'buddy': buddy, 'mover': mover,
            'state_class': state_class}


def tpbody(bridge, x, y, z):
    return bridge.exec(f'tpbody {x} {y} {z}', timeout_ms=15000)


def main():
    # Hard stop: the owner's rule is that game runs happen only inside the SP-Test VM.
    # The explicit opt-in makes an accidental host launch impossible.
    if os.environ.get('SP_VM_OK') != '1':
        raise SystemExit('Refusing to launch outside the SP-Test VM: set SP_VM_OK=1 there.')
    report = {'owner': OWNER, 'game_dir': os.environ.get('SP_GAME_DIR')}
    if launch_lock.acquire(OWNER, minutes=40,
                           wait=float(os.environ.get('SP_LOCK_WAIT', '900')),
                           pid=os.getpid()) != 0:
        raise SystemExit('launch lock is held by another process')
    try:
        with ce.Game(duration=300) as game:
            report['run'] = str(game.run_dir)
            frames = game.run_dir / 'frames'
            frames.mkdir(parents=True, exist_ok=True)
            game.wait_for_ready(minimum=28, quiet=2.0, timeout=70)
            report['load_game'] = hover_click(game.pid, *LOAD_GAME)
            report['row'] = hover_click(game.pid, *SAVE_ROW)
            report['load'] = hover_click(game.pid, *LOAD_BUTTON)
            report['confirm'] = hover_click(game.pid, *DIALOG_CONFIRM)
            # About 45 s to load Clyde's Fort on the host; the VM can be slower.
            time.sleep(50)
            shot = capture_game_window(game.pid, frames / 'level.png')
            report['shot'] = {key: shot.get(key) for key in
                              ('captured', 'dhash', 'mean_luma')}
            with Bridge.connect('tcp://127.0.0.1:27990', wait_s=60) as bridge:
                report['scan_mod'] = bridge.exec('probe', timeout_ms=90000)
                time.sleep(0.5)
                report['scan_hooks'] = bridge.exec('aiscan', timeout_ms=180000)
                if not report['scan_hooks'].get('ok'):
                    report['error'] = 'aiscan failed: ' + str(report['scan_hooks'].get('output'))
                    return report
                time.sleep(1.5)
                first = bridge.exec('aistats', timeout_ms=30000)
                time.sleep(1.0)
                second = bridge.exec('aistats', timeout_ms=30000)
                report['stats_first'] = first.get('output')
                report['stats_second'] = second.get('output')
                hooks, state_line = parse_stats(second.get('output'))
                report['state_line'] = state_line
                report['hooks'] = hooks
                candidates = rank_candidates(hooks)
                report['candidates'] = candidates
                if not candidates:
                    report['error'] = 'no slot reached 8 calls/s with a live self'
                    return report

                # --- default FollowPlayer, before the hold -------------------------
                before = sample(bridge)
                tpbody(bridge, 4, 0, 0)
                time.sleep(2.5)
                after = sample(bridge)
                report['follow_before'] = before
                report['follow_after'] = after
                report['follow_hero_moved'] = delta(before['hero'], after['hero'])
                report['follow_buddy_moved'] = delta(before['buddy'], after['buddy'])
                report['follow_buddy_mover_moved'] = delta(before['mover'], after['mover'])
                tpbody(bridge, -4, 0, 0)
                time.sleep(1.5)

                # --- try the ranked slots; first real freeze wins ------------------
                attempts = []
                winner = None
                for candidate in candidates:
                    hold_id = candidate.get('ID')
                    hold = bridge.exec('aihold ' + hold_id, timeout_ms=60000)
                    if not hold.get('ok'):
                        attempts.append({'id': hold_id, 'hold': hold,
                                         'result': 'hold command failed'})
                        continue
                    time.sleep(0.4)
                    anchor = sample(bridge)
                    tpbody(bridge, 4, 0, 0)
                    time.sleep(2.5)
                    held = sample(bridge)
                    hero_moved = delta(anchor['hero'], held['hero'])
                    buddy_moved = delta(anchor['buddy'], held['buddy'])
                    mover_moved = delta(anchor['mover'], held['mover'])
                    record = {'id': hold_id, 'label': candidate.get('LABEL'),
                              'hold': hold, 'anchor': anchor, 'held': held,
                              'hero_moved': hero_moved, 'buddy_moved': buddy_moved,
                              'mover_moved': mover_moved}
                    attempts.append(record)
                    tpbody(bridge, -4, 0, 0)
                    time.sleep(0.8)
                    if (hero_moved is not None and hero_moved >= 2.0 and
                            buddy_moved is not None and buddy_moved <= 0.05):
                        winner = record
                        winner['hold'] = hold
                        break
                    bridge.exec('aihold off', timeout_ms=30000)
                    time.sleep(0.5)
                report['hold_attempts'] = attempts
                report['update'] = winner
                if winner is None:
                    report['error'] = ('none of the ranked slots froze the buddy; '
                                       'the slot list and rates are in the log')
                    return report

                # Keep the winner active for the command-state observation.
                time.sleep(0.5)
                held_stats = bridge.exec('aistats', timeout_ms=30000)
                report['hold_stats'] = held_stats.get('output')
                _hooks, report['hold_state_line'] = parse_stats(held_stats.get('output'))
                release = bridge.exec('aihold off', timeout_ms=30000)
                report['release_command'] = release
                time.sleep(0.5)

                # --- hand control back: the buddy should follow again -------------
                resumed_before = sample(bridge)
                tpbody(bridge, 3, 0, 0)
                time.sleep(2.0)
                resumed_after = sample(bridge)
                report['resume_before'] = resumed_before
                report['resume_after'] = resumed_after
                report['resume_hero_moved'] = delta(resumed_before['hero'],
                                                    resumed_after['hero'])
                report['resume_buddy_moved'] = delta(resumed_before['buddy'],
                                                     resumed_after['buddy'])
                tpbody(bridge, -3, 0, 0)
                time.sleep(1.0)
                report['state_final'] = sample(bridge)
                classes = []
                for item in (before, after, winner['anchor'], winner['held'],
                             resumed_before, resumed_after):
                    if item.get('state_class'):
                        classes.append(item['state_class'])
                if report.get('state_line'):
                    classes.append(report['state_line'])
                if report.get('hold_state_line'):
                    classes.append(report['hold_state_line'])
                report['state_classes_seen'] = sorted(set(classes))
                report['command_mode_observed'] = any('Command' in item for item in classes)
                report['alive'] = game.alive()
            report['validity'] = {key: game.result.get(key) for key in
                                  ('original_profile_unchanged', 'guard_integrity',
                                   'original_exe_hash_still_matches')}
    finally:
        launch_lock.release(OWNER)
        (HERE / 'step2-hook-report.json').write_text(
            json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8')
    return report


if __name__ == '__main__':
    result = main()
    print(json.dumps({
        'run': result.get('run'),
        'update': (result.get('update') or {}).get('id'),
        'update_label': (result.get('update') or {}).get('label'),
        'candidates': [c.get('ID') for c in result.get('candidates', [])],
        'follow_hero_moved': result.get('follow_hero_moved'),
        'follow_buddy_moved': result.get('follow_buddy_moved'),
        'hold_hero_moved': (result.get('update') or {}).get('hero_moved'),
        'hold_buddy_moved': (result.get('update') or {}).get('buddy_moved'),
        'resume_buddy_moved': result.get('resume_buddy_moved'),
        'command_mode_observed': result.get('command_mode_observed'),
        'state_classes_seen': result.get('state_classes_seen'),
        'alive': result.get('alive'),
        'validity': result.get('validity'),
        'error': result.get('error'),
    }, indent=2))
