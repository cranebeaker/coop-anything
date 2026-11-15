r"""A machine-checkable path from launch to a loaded level, and a detector that can fail.

One entry point:

    from in_game import start_run
    run = start_run()                                 # menu only
    run = start_run(save='Save Slot 001')             # -startsavegame
    run = start_run(level=r'world\levels\1_southpark\2_hub.opf')
    run['in_game'], run['level'], run['screenshot'], run['evidence']

Command line:

    py -3.14 -B in_game.py --once
    py -3.14 -B in_game.py --once --action 40:click:0.844,0.657     # click "Continue"
    py -3.14 -B in_game.py --once --startsavegame "Save Slot 001"
    py -3.14 -B in_game.py --replay <run dir>          # re-decide a stored run

Every launch happens on the copy of the install named by `SP_GAME_DIR`, under
`save_guard.js` + `guard_session.py`, with the original profile copied into the run folder
and redirected there, and — unless `--no-lock` — under `launch_lock.py`.

Why the verdict is trusted: `in_game` is a pure function of the evidence stored in the run
folder (`decide`), two independent signals must agree, and a black frame can never produce
`true` (that was the failure that started this gate). `--replay` prints the same verdict for
a stored run, so a reader can re-check an old run without launching anything.
"""
import argparse
import ctypes
import hashlib
import json
import os
import shutil
import sys
import time
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from capture_game_window import (capture_game_window, click_client, grid_correlation,
                                 hold_key, send_keys)
from guard_session import run_guarded
import launch_lock

GAME = Path(os.environ.get('SP_GAME_DIR', r'D:\coop-work\game-test'))
EXE = GAME / 'South Park - The Stick of Truth.exe'
EXPECTED_SHA256 = '54a83d055f97cad8beeb5aa027c96c0c2aa4a6c36394634270297dd47856a0d6'
ORIGINAL_PROFILE = Path.home() / 'Documents' / 'My Games' / 'South Park - The Stick of Truth'
WORKTREE = ROOT.parents[2]
RUN_ROOT = Path(os.environ.get('SP_RUN_ROOT', WORKTREE / 'local' / 'game-runs'))

# ---------------------------------------------------------------------------
# Engine memory, read-only. `world\levels\<...>.opf` is the path form every level in
# this build uses (local/level_names.txt). At the front end the *manifest* of level
# paths is already resident (24 of them, once each), so presence alone is not evidence
# of a loaded level — the detector counts occurrences per path and requires one path to
# be held repeatedly, which is what a loaded level does. `.amx` names show which scripts
# are resident: the front end holds none of the level scripts.
PROBE_NEEDLES = ['world\\levels\\', '.amx', '.bk2']

# Screen fingerprints: the 8x8 luma grid of real frames, as integers. Pixels never leave
# the machine — a run folder keeps its PNGs under local/, these numbers are just enough to
# tell a front-end screen from a level without one. Front-end frames correlate 1.00 with
# each other and 0.31-0.34 with a level frame (matrix in notes/03-gameplay-harness.md).
REFERENCES = {
    # front end: run in-game-39eaae... frame-048 / frame-060, run in-game-83f32f... frame-042
    'menu': [
        [7, 7, 12, 18, 21, 20, 17, 11, 7, 7, 16, 59, 57, 15, 10, 8, 7, 14, 73, 62, 63, 62, 7, 7,
         7, 11, 33, 49, 41, 25, 7, 7, 9, 12, 28, 56, 54, 21, 7, 7, 13, 14, 14, 10, 9, 8, 38, 23,
         13, 12, 14, 8, 7, 16, 50, 18, 9, 11, 16, 14, 13, 9, 13, 8],
        [5, 5, 9, 15, 19, 19, 15, 10, 5, 5, 11, 54, 54, 13, 9, 6, 5, 12, 68, 59, 61, 60, 5, 5,
         6, 8, 29, 45, 39, 22, 5, 5, 11, 10, 23, 53, 53, 19, 5, 5, 12, 12, 11, 10, 8, 6, 36, 21,
         7, 11, 12, 9, 9, 16, 48, 16, 5, 7, 11, 13, 13, 8, 11, 6]],
    # level: run in-game-83f32f... frame-056 / frame-084, run in-game-39eaae... frame-072
    'gameplay': [
        [177, 179, 179, 179, 180, 174, 171, 126, 158, 166, 166, 166, 166, 142, 119, 106,
         133, 166, 166, 166, 166, 140, 128, 103, 133, 143, 166, 163, 158, 136, 83, 90,
         134, 124, 110, 119, 102, 108, 120, 97, 111, 112, 86, 104, 81, 80, 99, 123,
         91, 93, 91, 17, 27, 69, 78, 103, 91, 74, 74, 16, 33, 91, 91, 79]],
}
FRONT_END_CORRELATION = 0.80


# Calibrated in notes/03-gameplay-harness.md against run folders. The level-path counts
# (a loaded level holds its path repeatedly) turned out NOT to separate front end from
# level: the level manifest already holds `2_hub` 9 times at the menu. They stay in the
# record as evidence, not as a decision input.
AMX_HEADER_PATTERN = '?? ?? ?? ?? e1 f1 0a 0a 14 00 0c 00 ??'
AMX_MIN_HEADERS = 1
WORKING_SET_LEVEL_MB = 470.0
WORKING_SET_GROWTH = 1.05

PROBE_JS = r"""
const MAX_SCAN_BYTES = 448 * 1024 * 1024;
const MAX_RANGE_BYTES = 32 * 1024 * 1024;

function hexOf(text) {
    let out = '';
    for (let i = 0; i < text.length; i++) out += text.charCodeAt(i).toString(16).padStart(2, '0');
    return out;
}

function printableRuns(pointer, size) {
    const runs = [];
    let text = '';
    for (let i = 0; i < size; i++) {
        let byte = 0;
        try { byte = pointer.add(i).readU8(); } catch (error) { break; }
        if (byte >= 32 && byte < 127) {
            text += String.fromCharCode(byte);
        } else {
            if (text.length >= 6) runs.push(text);
            text = '';
        }
    }
    if (text.length >= 6) runs.push(text);
    return runs;
}

rpc.exports = {
    probe(needles, amxPattern) {
        const patterns = needles.map(name => ({name, hex: hexOf(name)}));
        const summary = {};
        for (const pattern of patterns) summary[pattern.name] = {count: 0, values: {}};
        let scanned = 0;
        let ranges = 0;
        let amxHeaders = 0;
        let amxRanges = 0;
        const seen = new Set();
        for (const protection of ['rw-', 'r--']) {
            let found;
            try { found = Process.enumerateRanges({protection, coalesce: true}); } catch (error) { continue; }
            for (const range of found) {
                const key = range.base.toString() + ':' + range.size;
                if (seen.has(key)) continue;
                seen.add(key);
                if (range.size > MAX_RANGE_BYTES) continue;
                if (scanned + range.size > MAX_SCAN_BYTES) continue;
                scanned += range.size;
                ranges += 1;
                if (amxPattern) {
                    try {
                        const headers = Memory.scanSync(range.base, range.size, amxPattern);
                        if (headers.length) { amxHeaders += headers.length; amxRanges += 1; }
                    } catch (error) { /* unreadable range, ignore */ }
                }
                for (const pattern of patterns) {
                    let matches = [];
                    try { matches = Memory.scanSync(range.base, range.size, pattern.hex); }
                    catch (error) { continue; }
                    summary[pattern.name].count += matches.length;
                    const bucket = summary[pattern.name].values;
                    for (const match of matches) {
                        let runs = [];
                        try { runs = printableRuns(match.address.sub(64), 160); } catch (error) { continue; }
                        for (const run of runs) {
                            const lowered = run.toLowerCase();
                            const needle = pattern.name.toLowerCase();
                            const at = lowered.indexOf(needle);
                            if (at < 0) continue;
                            let value = run.slice(at);
                            if (value.length > 120) value = value.slice(0, 120);
                            bucket[value] = (bucket[value] || 0) + 1;
                            break;
                        }
                    }
                }
            }
        }
        return {scannedBytes: scanned, ranges, needles: summary,
                amxHeaders, amxRanges,
                modules: Process.enumerateModules().length, arch: Process.arch};
    }
};
"""


# ---------------------------------------------------------------------------
def snapshot(root):
    return {path.relative_to(root).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted(root.rglob('*')) if path.is_file()}


def inventory(root):
    return {path.relative_to(root).as_posix(): path.stat().st_size
            for path in sorted(root.rglob('*')) if path.is_file()}


class _MemoryCounters(ctypes.Structure):
    _fields_ = [('cb', ctypes.c_ulong), ('PageFaultCount', ctypes.c_ulong),
                ('PeakWorkingSetSize', ctypes.c_size_t), ('WorkingSetSize', ctypes.c_size_t),
                ('QuotaPeakPagedPoolUsage', ctypes.c_size_t),
                ('QuotaPagedPoolUsage', ctypes.c_size_t),
                ('QuotaPeakNonPagedPoolUsage', ctypes.c_size_t),
                ('QuotaNonPagedPoolUsage', ctypes.c_size_t), ('PagefileUsage', ctypes.c_size_t),
                ('PeakPagefileUsage', ctypes.c_size_t)]


def process_sample(pid):
    """CPU seconds burned and working set (MB): engine-side activity, no pixels involved."""
    kernel32 = ctypes.windll.kernel32
    psapi = ctypes.windll.psapi
    handle = kernel32.OpenProcess(0x1000, False, int(pid))     # QUERY_LIMITED_INFORMATION
    if not handle:
        return {'cpu_seconds': None, 'working_set_mb': None}
    try:
        creation, exit_time, kernel, user = (ctypes.c_ulonglong() for _ in range(4))
        cpu = None
        if kernel32.GetProcessTimes(handle, ctypes.byref(creation), ctypes.byref(exit_time),
                                    ctypes.byref(kernel), ctypes.byref(user)):
            cpu = round((kernel.value + user.value) / 10_000_000.0, 3)
        counters = _MemoryCounters()
        counters.cb = ctypes.sizeof(counters)
        working = None
        if psapi.GetProcessMemoryInfo(handle, ctypes.byref(counters), counters.cb):
            working = round(counters.WorkingSetSize / (1024 * 1024), 1)
        return {'cpu_seconds': cpu, 'working_set_mb': working}
    finally:
        kernel32.CloseHandle(handle)


def level_paths(probe):
    """Every `world\\levels\\...opf` the probe saw, with how many times it is held."""
    values = ((probe.get('needles') or {}).get('world\\levels\\') or {}).get('values') or {}
    paths = {}
    for text, count in values.items():
        lowered = text.lower()
        at = lowered.find('world\\levels\\')
        if at < 0:
            continue
        fragment = text[at:]
        end = fragment.lower().find('.opf')
        path = fragment[:end + 4] if end >= 0 else fragment.split()[0]
        path = path.strip().split(' |')[0]
        name = path.replace('/', '\\').split('\\')[-1]
        if not name or not name.lower().endswith('.opf'):
            continue
        entry = paths.setdefault(path, [name[:-4], 0])
        entry[1] += count
    return sorted(([name, path, count] for path, (name, count) in paths.items()),
                  key=lambda item: (-item[2], item[0]))


def script_names(probe):
    values = ((probe.get('needles') or {}).get('.amx') or {}).get('values') or {}
    names = {}
    for text, count in values.items():
        lowered = text.lower()
        at = lowered.find('.amx')
        end = at + 4
        start = text.rfind(' ', 0, at) + 1
        name = text[start:end].strip().replace('/', '\\').split('\\')[-1]
        if name and len(name) < 120:
            names[name] = names.get(name, 0) + count
    return sorted(([name, count] for name, count in names.items()), key=lambda item: -item[1])[:40]


def movie_names(probe):
    values = ((probe.get('needles') or {}).get('.bk2') or {}).get('values') or {}
    names = {}
    for text, count in values.items():
        lowered = text.lower()
        at = lowered.find('.bk2')
        end = at + 4
        start = text.rfind(' ', 0, at) + 1
        name = text[start:end].strip()
        if name:
            names[name] = names.get(name, 0) + count
    return sorted(([name, count] for name, count in names.items()), key=lambda item: -item[1])[:20]


def classify_frame(frame):
    """Front end or level? Compares the frame's coarse structure with the fingerprints.

    Returns 'front_end', 'level', 'black' (nothing readable) or 'unknown' (readable but
    unlike anything measured, e.g. a new level or a cutscene: the detector must not guess).
    """
    if not frame or not frame.get('captured'):
        return 'black'
    if frame.get('nonblack_ratio', 0) < 0.02:
        return 'black'
    grid = frame.get('luma_grid') or []
    front = round(max([grid_correlation(grid, reference) for reference in REFERENCES['menu']],
                      default=-1.0), 4)
    level = round(max([grid_correlation(grid, reference) for reference in REFERENCES['gameplay']],
                      default=-1.0), 4)
    frame['correlation'] = {'front_end': front, 'level': level}
    if front >= FRONT_END_CORRELATION and front > level:
        return 'front_end'
    if level >= FRONT_END_CORRELATION and level > front:
        return 'level'
    return 'unknown'


def decide(record):
    """The verdict. Pure function of the stored evidence: no game, no clock needed.

    Signal 1 (screen): a readable capture of the game window whose coarse structure is not
    the front end (`FRONT_END_CORRELATION`; measured front-end frames correlate 1.00 with
    each other and 0.31-0.34 with a level frame).
    Signal 2 (engine): the last memory probe's engine-side numbers - resident AMX script
    headers, the level paths it holds, the working set.

    `in_game` needs both to say "level". A black frame, an unrecognised screen or an engine
    that looks like a front end all give `false` with the reason in `warnings`.
    """
    return _decide(record.get('samples') or [], record.get('probes') or [],
                   record_level=(record.get('requested') or {}).get('level'),
                   record_level_path=(record.get('requested') or {}).get('level'))


def verdict_history(record):
    """`in_game` at every capture: the same decision rule, on the evidence known so far."""
    samples = record.get('samples') or []
    probes = record.get('probes') or []
    history = []
    for index, sample in enumerate(samples):
        known = [probe for probe in probes
                 if (probe.get('at') or 0) <= (sample.get('at') or 0) + 0.001]
        verdict = _decide(samples[:index + 1], known)
        history.append({'at': sample.get('at'), 'in_game': verdict['in_game'],
                        'screen_class': verdict['signals']['screen_class_last'],
                        'engine_looks_like_level': verdict['signals']['engine_looks_like_level'],
                        'reasons': verdict['warnings']})
    return history


def _decide(samples, probes, record_level=None, record_level_path=None):
    readable = [sample for sample in samples if (sample.get('capture') or {}).get('captured')]
    classes = [classify_frame(sample.get('capture')) for sample in samples]
    verdicts = []
    for probe in probes:
        paths = level_paths(probe)
        scripts = script_names(probe)
        top = paths[0] if paths else None
        verdicts.append({'at': probe.get('at'), 'paths': paths[:6], 'top': top,
                         'held_repeat': top[2] if top else 0,
                         'scripts_resident': len(scripts),
                         'amx_headers': probe.get('amxHeaders'),
                         'movies': movie_names(probe),
                         'error': probe.get('error')})
    last_probe = verdicts[-1] if verdicts else {}
    last_readable = readable[-1] if readable else None
    last_class = classify_frame(last_readable.get('capture')) if last_readable else 'black'
    working_set = last_readable.get('working_set_mb') if last_readable else None
    amx = last_probe.get('amx_headers') or 0
    # Engine side. Preferred test: the working set grew clearly after a frame the screen
    # signal called a front end (self-calibrating, same process). Fallbacks: resident AMX
    # script headers, then an absolute working-set floor measured on the test install.
    front_end_sets = [sample.get('working_set_mb') for sample in readable[:-1]
                      if sample.get('working_set_mb')
                      and classify_frame(sample.get('capture')) == 'front_end']
    engine_reason = None
    engine_level = False
    if last_class == 'front_end':
        engine_reason = 'screen is the front end'
    else:
        if amx >= AMX_MIN_HEADERS:
            engine_level, engine_reason = True, 'amx headers resident'
        if (front_end_sets and working_set
                and working_set >= max(front_end_sets) * WORKING_SET_GROWTH):
            engine_level, engine_reason = True, 'working set grew from front end'
        if not engine_level and (working_set or 0) >= WORKING_SET_LEVEL_MB:
            engine_level, engine_reason = True, 'working set above the front-end floor'
        if not engine_level:
            engine_reason = 'front end'
    screen_level = last_class in ('level', 'unknown') and last_class != 'front_end'
    warnings = []
    if not readable:
        warnings.append('no readable frame: every capture was black or the window was missing')
    elif last_class == 'front_end':
        warnings.append('last readable frame matches the front end fingerprints')
    if not engine_level:
        warnings.append('engine numbers look like a front end: %s AMX script headers, '
                        'working set %s MB' % (amx, working_set))
    if last_class == 'unknown':
        warnings.append('screen is readable but unlike a measured level; keeping the verdict '
                        'false unless the engine confirms')
    in_game = bool(readable) and screen_level and engine_level
    top = last_probe.get('top')
    return {
        'in_game': in_game,
        # Everything the verdict was computed from, so a later reader can re-decide it without
        # the game (also what `--replay` and any test harness should assert on).
        'evidence': {'samples': samples, 'probes': probes},
        # A level *name* is only claimed when the caller asked for one: the most-held path in
        # engine memory is a candidate, not proof, so it stays labelled as such.
        'level': (record_level or None),
        'level_candidate': top[0] if top else None,
        'level_path': (record_level_path or None),
        'signals': {
            'screen_readable': bool(readable),
            'screen_class_history': classes,
            'screen_class_last': last_class,
            'screen_correlation': (last_readable.get('capture') or {}).get('correlation')
            if last_readable else None,
            'working_set_mb': working_set,
            'amx_headers': amx,
            'engine_looks_like_level': engine_level,
            'engine_reason': engine_reason,
            'engine_checks_at': [verdict['at'] for verdict in verdicts],
            'engine': verdicts,
            'level_scripts_resident': last_probe.get('scripts_resident'),
        },
        'warnings': warnings,
    }



# ---------------------------------------------------------------------------
def _install_redirections(run):
    redirections = []
    for source, label in [
        (Path(r'C:\Program Files (x86)\Steam\logs'), 'steam-logs'),
        (Path.home() / 'AppData/Local/Microsoft/Windows/Caches', 'windows-cache'),
        (Path(r'C:\ProgramData\NVIDIA Corporation\Drs'), 'nvidia-drs'),
    ]:
        destination = run / 'runtime-files' / label
        destination.mkdir(parents=True, exist_ok=True)
        if label == 'nvidia-drs' and source.is_dir():
            for item in source.iterdir():
                if item.is_file() and item.stat().st_size < 20 * 1024 * 1024:
                    shutil.copy2(item, destination / item.name)
        redirections.append({'from': str(source), 'to': str(destination)})
    return redirections


def _parse_actions(values):
    """`40:click:0.844,0.657` / `50:keys:enter,enter` / `60:hold:w,2.0` -> events."""
    actions = []
    for value in values or []:
        seconds, kind, payload = value.split(':', 2)
        actions.append((float(seconds), kind.strip().lower(), payload))
    return actions


def start_run(save=None, level=None, duration=None, take_lock=True, owner='step02',
              actions=None, sample_times=None, probe_times=None, extra_args=None,
              notes=None, run_root=None):
    """Launch the test copy, watch it, and return the run record.

    `save`  -> `-startsavegame <name>` (the save game's name, e.g. "Save Slot 001")
    `level` -> `-startlevel <path> 0 0 0 0 0 0 0` (the engine's own command-line format)
    """
    if hashlib.sha256(EXE.read_bytes()).hexdigest() != EXPECTED_SHA256:
        raise SystemExit('Unexpected game executable; refusing launch')
    if not ORIGINAL_PROFILE.is_dir():
        raise SystemExit('Expected original profile not found; refusing launch')
    if ctypes.windll.user32.FindWindowW('South Park\u2122: The Stick of Truth\u2122', None):
        raise SystemExit('A South Park window already exists; refusing to interfere')
    lock_held = False
    if take_lock:
        if launch_lock.acquire(owner, minutes=45, wait=float(os.environ.get('SP_LOCK_WAIT', '1800')),
                               pid=os.getpid()) != 0:
            raise SystemExit('Another agent holds the launch lock; refusing to launch')
        lock_held = True
    try:
        return _run(save=save, level=level, duration=duration, actions=actions,
                    sample_times=sample_times, probe_times=probe_times,
                    extra_args=extra_args, notes=notes, run_root=run_root)
    finally:
        if lock_held:
            launch_lock.release(owner)


def _run(save, level, duration, actions, sample_times, probe_times, extra_args, notes, run_root):
    run = Path(run_root or RUN_ROOT) / ('in-game-' + uuid.uuid4().hex)
    run.mkdir(parents=True)
    backup = run / 'original-profile-backup'
    profile = run / 'test-profile'
    before = snapshot(ORIGINAL_PROFILE)
    shutil.copytree(ORIGINAL_PROFILE, backup)
    shutil.copytree(backup, profile)
    if snapshot(backup) != before or snapshot(ORIGINAL_PROFILE) != before:
        raise SystemExit('Profile changed while copying it into the run folder; refusing launch')
    config_backup = run / 'test-config-backup'
    config_backup.mkdir()
    for path in GAME.glob('*.ini'):
        shutil.copy2(path, config_backup / path.name)
    redirections = _install_redirections(run)
    temporary = run / 'runtime-files' / 'temp'
    temporary.mkdir(parents=True, exist_ok=True)

    launch_args = [str(EXE), '-window']
    if save:
        launch_args.extend(['-startsavegame', save])
    if level:
        launch_args.extend(['-startlevel', level, '0', '0', '0', '0', '0', '0', '0'])
    launch_args.extend(extra_args or [])

    config = {
        'originalSaves': str(ORIGINAL_PROFILE), 'testSaves': str(profile),
        'writeRoots': [str(GAME), str(run)], 'redirections': redirections,
        'nativeCandidates': [], 'traceNatives': [], 'scriptSignatures': [],
        'blockRemoteStorage': False,
        'logPaths': ['.sav', '.bk2', 'movies', 'console_scripts'],
    }
    samples = sorted(sample_times or [12, 24, 36, 48, 60, 72, 84])
    probes = sorted(probe_times or [samples[1], samples[-1]])
    events = [(float(at), 'capture', None) for at in samples]
    events += [(float(at), 'probe', None) for at in probes]
    events += [(float(seconds), kind, payload) for seconds, kind, payload in _parse_actions(actions)]
    events.sort(key=lambda item: item[0])
    duration = duration or (events[-1][0] + 8 if events else 20)
    record = {
        'run': str(run),
        'argv': launch_args,
        'started': time.strftime('%Y-%m-%dT%H:%M:%S'),
        'requested': {'save': save, 'level': level, 'notes': notes},
        'plan': {'samples': samples, 'probes': probes,
                 'actions': [[at, kind, payload] for at, kind, payload in events
                             if kind in ('click', 'keys')]},
        'samples': [], 'probes': [], 'actions': [], 'notes': notes,
    }
    game_before = inventory(GAME)
    profile_before = inventory(profile)
    started = time.time()

    def watch(pid, guard_script):
        """Runs after resume: frames, engine memory and clicked actions on one clock."""
        device = _frida_device()
        session = None
        probe_script = None
        try:
            session = device.attach(pid)
            probe_script = session.create_script(PROBE_JS, name='in-game-probe')
            probe_script.load()
            for target, kind, payload in events:
                wait = target - (time.time() - started)
                if wait > 0:
                    time.sleep(wait)
                if kind == 'capture':
                    frame_path = run / 'frames' / ('frame-%03d.png' % int(target))
                    capture = capture_game_window(pid, frame_path)
                    counters = process_sample(pid)
                    record['samples'].append({
                        'at': round(time.time() - started, 1),
                        'capture': {key: value for key, value in capture.items() if key != 'attempts'},
                        'capture_attempts': capture.get('attempts'),
                        **counters})
                elif kind == 'probe':
                    probe_started = time.time()
                    try:
                        probe = probe_script.exports_sync.probe(PROBE_NEEDLES, AMX_HEADER_PATTERN)
                    except Exception as error:            # noqa: BLE001 - reported, not raised
                        probe = {'error': repr(error)}
                    probe['seconds'] = round(time.time() - probe_started, 2)
                    probe['at'] = round(time.time() - started, 1)
                    record['probes'].append(probe)
                elif kind == 'click':
                    fraction_x, fraction_y = (float(part) for part in payload.split(','))
                    result = click_client(pid, fraction_x, fraction_y)
                    record['actions'].append({'at': round(time.time() - started, 1),
                                              'kind': kind, 'payload': payload,
                                              'result': result})
                elif kind == 'keys':
                    result = send_keys(pid, [part.strip() for part in payload.split(',')])
                    record['actions'].append({'at': round(time.time() - started, 1),
                                              'kind': kind, 'payload': payload,
                                              'result': result})
                elif kind == 'hold':
                    key, seconds = (part.strip() for part in payload.split(','))
                    result = hold_key(pid, key, float(seconds))
                    record['actions'].append({'at': round(time.time() - started, 1),
                                              'kind': kind, 'payload': payload,
                                              'result': result})
        finally:
            if probe_script is not None:
                try:
                    probe_script.unload()
                except Exception:                        # noqa: BLE001
                    pass
            if session is not None:
                try:
                    session.detach()
                except Exception:                        # noqa: BLE001
                    pass

    result = {}
    try:
        result = run_guarded(launch_args, config, duration=2, cwd=GAME,
                             environment={'SteamAppId': '213670', 'SteamGameId': '213670',
                                          'TEMP': str(temporary), 'TMP': str(temporary)},
                             during=watch)
    except Exception as error:                            # noqa: BLE001 - a failed run is data
        record['launcher_error'] = repr(error)
    finally:
        after = snapshot(ORIGINAL_PROFILE)
        game_after = inventory(GAME)
        profile_after = inventory(profile)
        record['pid'] = result.get('pid')
        record['guard'] = {
            'resumed': result.get('resumed'),
            'exited_before_deadline': result.get('exited_before_deadline'),
            'detach_reason': result.get('detach_reason'),
            'crash': result.get('crash'),
            'guard_telemetry_counters': (result.get('telemetry') or {}).get('counters'),
            'output': (result.get('output') or [])[:20],
            'errors': result.get('errors'),
            'events': [event for event in (result.get('events') or [])
                       if event.get('kind') in ('redirect', 'blocked-write', 'blocked-write-open',
                                                'path-open', 'guard-error')][:80],
        }
        record['profile'] = {
            'new_saves_in_test_profile': sorted(
                name for name in profile_after
                if name not in profile_before and name.lower().endswith(('.sav', '.opt'))),
            'test_profile_files': len(profile_after),
            'new_game_files': sorted(name for name in game_after if name not in game_before),
        }
        record['validity'] = {
            'original_profile_unchanged': before == after,
            'guard_integrity': not result.get('errors') and not any(
                event.get('kind') == 'guard-error' for event in (result.get('events') or [])),
            'original_exe_hash_still_matches':
                hashlib.sha256(EXE.read_bytes()).hexdigest() == EXPECTED_SHA256,
        }
        record['validity']['run_valid'] = all(record['validity'].values())
        record['stdout_state_lines'] = [line[:200] for line in
                                        (line.get('text', '') for line in
                                         record['guard']['output'])
                                        if 'GS_' in line or 'level' in line.lower()][:20]
        record['movies_opened'] = sorted({event.get('path', '') for event in record['guard']['events']
                                          if event.get('kind') == 'path-open'
                                          and '.bk2' in str(event.get('path', '')).lower()})
        record.update(decide(record))
        record['verdict_history'] = verdict_history(record)
        if (run / 'frames').is_dir():
            record['frames'] = sorted(str(path.relative_to(run)).replace('\\', '/')
                                      for path in (run / 'frames').glob('*.png'))
        else:
            record['frames'] = []
        readable = [sample for sample in record['samples']
                    if (sample.get('capture') or {}).get('captured')]
        record['screenshot'] = readable[-1]['capture'].get('png') if readable else None
        (run / 'result.json').write_text(json.dumps(record, indent=2, default=str),
                                         encoding='utf-8')
    return record


_FRIDA_DEVICE = None


def _frida_device():
    global _FRIDA_DEVICE
    if _FRIDA_DEVICE is None:
        import frida
        _FRIDA_DEVICE = frida.get_local_device()
    return _FRIDA_DEVICE


# ---------------------------------------------------------------------------
def replay(run_dir):
    """Re-decide a stored run with the current `decide`, and re-classify its frames."""
    run = Path(run_dir)
    record = json.loads((run / 'result.json').read_text(encoding='utf-8'))
    record.update(decide(record))
    record['verdict_history'] = verdict_history(record)
    for sample in record['samples']:
        sample['capture_class'] = classify_frame(sample.get('capture'))
    return record


def summarise(record):
    return {
        'run': record.get('run'),
        'pid': record.get('pid'),
        'in_game': record.get('in_game'),
        'level': record.get('level'),
        'level_path': record.get('level_path'),
        'screenshot': record.get('screenshot'),
        'requested': record.get('requested'),
        'level_candidate': record.get('level_candidate'),
        'signals': record.get('signals'),
        'warnings': record.get('warnings'),
        'validity': record.get('validity'),
        'profile': record.get('profile'),
        'movies_opened': record.get('movies_opened'),
        'actions': record.get('actions'),
        'verdict_history': record.get('verdict_history'),
        'samples': [{'at': sample.get('at'),
                     'captured': (sample.get('capture') or {}).get('captured'),
                     'method': (sample.get('capture') or {}).get('method'),
                     'nonblack_ratio': (sample.get('capture') or {}).get('nonblack_ratio'),
                     'screen_class': classify_frame(sample.get('capture')),
                     'cpu_seconds': sample.get('cpu_seconds'),
                     'working_set_mb': sample.get('working_set_mb')}
                    for sample in record.get('samples', [])],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--once', action='store_true', help='one launch, print a JSON summary')
    parser.add_argument('--replay', metavar='RUN_DIR', help='re-decide a stored run folder')
    parser.add_argument('--startsavegame', metavar='NAME')
    parser.add_argument('--startlevel', metavar='PATH')
    parser.add_argument('--duration', type=float, default=None)
    parser.add_argument('--sample-times', default='12,24,36,48,60,72,84')
    parser.add_argument('--probe-times', default=None,
                        help='when to read engine memory, default first and last sample')
    parser.add_argument('--action', action='append', default=[],
                        help='SECONDS:click:X,Y or SECONDS:keys:enter,space')
    parser.add_argument('--extra-arg', action='append', default=[],
                        help='extra raw launch argument (repeatable)')
    parser.add_argument('--notes', default=None)
    parser.add_argument('--no-lock', action='store_true')
    parser.add_argument('--owner', default=os.environ.get('SP_AGENT', 'step02'))
    arguments = parser.parse_args()
    if arguments.replay:
        print(json.dumps(summarise(replay(arguments.replay)), indent=2))
        return
    if not arguments.once:
        parser.error('choose --once or --replay')
    record = start_run(save=arguments.startsavegame, level=arguments.startlevel,
                       duration=arguments.duration,
                       sample_times=[float(value) for value in arguments.sample_times.split(',')],
                       probe_times=[float(value) for value in arguments.probe_times.split(',')]
                       if arguments.probe_times else None,
                       actions=arguments.action, extra_args=arguments.extra_arg,
                       notes=arguments.notes, take_lock=not arguments.no_lock,
                       owner=arguments.owner)
    print(json.dumps(summarise(record), indent=2))


if __name__ == '__main__':
    main()
