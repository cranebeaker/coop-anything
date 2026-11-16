r"""Read-only runtime object model for South Park: The Stick of Truth.

Owner: step 4 of the co-op project (see ``games/south-park-the-stick-of-truth/PARALLEL.md``).
This is the runtime half of Gate 3. It answers three questions and writes nothing into the
game process:

1. which classes from ``tools/rtti_index.json`` have live instances (a dword equal to
   ``module base + vtable RVA`` in the process's own writable memory), at the main menu and
   in a loaded level;
2. where the player body is, decided by **behaviour** (a float/double triple that moves when
   real key input makes the character walk, and stays still when it does not), not by name;
3. how many live instances the winning classes have in a normal single-player level.

Mechanism
---------
The static index (step 4, static half) gives 929 verified x86 vtables as RVAs. An object of
one of those classes begins with a pointer to its vtable, so the *value* ``base + rva``
appears in memory once per live instance. ``.text`` is Steam-CEG encrypted at rest, so no
code is read or disassembled here: everything is data.

One pass over committed read/write memory, aligned to 4 bytes, checks every dword against the
whole index at once (a ``Set`` of target values inside the Frida payload), so scanning all 929
classes costs the same as scanning one. Hits inside the module image are counted separately
(RTTI structures, COLs and multiple-inheritance vtable entries live there); only hits outside
the image can be heap objects.

The process is launched exactly like steps 2 and 3 launch it: the isolated copy named by
``SP_GAME_DIR``, spawned suspended under Steam's app id, ``tools/save_guard.js`` loaded and the
original profile copied into the run folder and redirected there, then resumed. This module
does not implement a launcher of its own: it imports ``in_game``/``guard_session``/
``capture_game_window``/``launch_lock`` from the step 2 and 3 tools and reuses
``guard_session.run_guarded`` with its ``during`` hook. Reuse, not a copy - see "Tools
location" below.

Nothing is ever written: no ``Memory.write*``, no ``NativeFunction``, no ``Interceptor``, no
console calls. The only interaction with the game is synthesized keyboard/mouse input, the
same path steps 2 and 3 use, plus read-only ``Memory.read*``/``Memory.scanSync``.

Which vtable to look for
-----------------------
A live object rarely uses the vtable of the class the index names for a subsystem. The class
that describes the player, ``OEI::Game_Component::Component_Player``, has a vtable that points
at nothing in memory: the object is a ``DS_Component_Player``, and the vtable in its first
dword is the ``DS_`` one. Counting only base-class vtables therefore reports zero live
instances of ``Component_Actor`` while 35 ``NY_Component_Actor`` objects are running (measured
in ``local/game-runs/object-model-029777b9``).

So candidates come from a class *or any class whose RTTI base chain contains it*
(``load_families``). Inside a family the rarest class is tried first, because "the player's own
component" is usually a single instance while the shared base is everything else.

Behaviour test
--------------
Candidate objects are the outside-image hits. Around each candidate the payload reads
``-0x400 .. +0x400`` as 32-bit floats *and* as 64-bit doubles (one read, both widths), then:

* snapshot 1, wait 1.0 s, snapshot 2 - the character is not moving: any triple that changes
  here is animation/timer noise and is rejected;
* hold ``w`` 1.2 s, snapshot 3; hold ``s``, snapshot 4 (opposite direction: the same triple
  should move back); hold ``a``, snapshot 5; hold ``d``, snapshot 6.

A triple (three consecutive values) that is still when idle and moves in all four walks, with
``w`` and ``s`` opposed (negative dot product), is a position. Frames are saved before and
after each hold so a human can confirm the character moved in the same run.

Tools location
--------------
``in_game.py`` (step 2), ``console_exec.py``/``save_guard.js`` (step 3) and
``capture_game_window.py``/``guard_session.py``/``launch_lock.py`` (shared) live in their own
step worktrees while several agents work in parallel. Point ``SP_TOOLS_DIR`` (or
``--tools-dir``) at the folder that has them; by default this module's own folder is tried
first, which is what a merged branch looks like. Only reads happen in that folder.

Usage
-----
    py -3.14 -B tools/object_model.py selftest          # offline: index + analysis
    py -3.14 -B tools/object_model.py selftest --live    # also exercise the payload, no game
    py -3.14 -B tools/object_model.py scan               # menu scan + level scan + walk
    py -3.14 -B tools/object_model.py scan --skip-walk   # instance counts only
    py -3.14 -B tools/object_model.py watch --class Component_Player --hold 2.5
    py -3.14 -B tools/object_model.py watch --run <run folder>

``scan`` is one guarded launch series and takes about three minutes. ``watch`` re-launches and
validates only the given classes' instances (longer holds), and ``watch --run`` re-analyses a
stored run folder offline - no launch, no lock.

Outputs: a run folder under ``<worktree>/local/game-runs/object-model-<hex>/`` holding
``result.json`` (module identification, per-class counts, candidate windows, movement
analysis, guard validity) and ``frames/*.png``. Addresses are stored as ``module+offset``;
the module's slide relative to its ``0x400000`` image base is stored as a number, never as an
absolute address.

stdout is forced to ASCII: the owner's console code page is cp1251.
"""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
import shutil
import sys
import time
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]


def _tool_dir(name='in_game.py', override=None):
    """First folder that holds ``name``: ``--tools-dir``/``SP_TOOLS_DIR``, then this one."""
    candidates = []
    if override:
        candidates.append(Path(override))
    if os.environ.get('SP_TOOLS_DIR'):
        candidates.append(Path(os.environ['SP_TOOLS_DIR']))
    candidates.append(ROOT)
    for candidate in candidates:
        if (candidate / name).is_file():
            return candidate
    raise SystemExit('cannot find %s; pass --tools-dir or set SP_TOOLS_DIR (tried: %s)'
                     % (name, ', '.join(str(path) for path in candidates)))


def _argv_tools_dir():
    """`--tools-dir X` / `--tools-dir=X` from the raw argv, before argparse exists."""
    for index, item in enumerate(sys.argv):
        if item == '--tools-dir' and index + 1 < len(sys.argv):
            return sys.argv[index + 1]
        if item.startswith('--tools-dir='):
            return item.split('=', 1)[1]
    return None


TOOLS = _tool_dir(override=_argv_tools_dir())
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))
for library in (TOOLS / 'runtime-libs', REPO / 'local' / 'runtime-libs'):
    if library.is_dir() and str(library) not in sys.path:
        sys.path.insert(0, str(library))

try:
    import frida
except ModuleNotFoundError as error:  # pragma: no cover - environment dependent
    raise SystemExit('frida is required (pinned wheel in <repo>/local/runtime-libs): ' + repr(error))

import in_game                                   # step 2: guarded harness, probe, verdict
import launch_lock                               # machine-wide launch lock
from capture_game_window import (capture_game_window, click_client, hold_key, send_keys,
                                 find_game_window, focus_window, api)
from guard_session import run_guarded            # the only launcher used here

GAME = in_game.GAME
EXE = in_game.EXE
EXPECTED_SHA256 = in_game.EXPECTED_SHA256
ORIGINAL_PROFILE = in_game.ORIGINAL_PROFILE
WINDOW_TITLE = 'South Park\u2122: The Stick of Truth\u2122'

RUN_ROOT = Path(os.environ.get('SP_RUN_ROOT', REPO / 'local' / 'game-runs'))
INDEX = ROOT / 'rtti_index.json'

# Click sequences for the game's own load list (Russian UI, 1280x720, fractions of the
# client area): "Загрузить игру" in the menu, the Clyde's Fort row, the bottom "Загрузить
# игру", then the overwrite-autosaves dialog's confirm button.
LOAD_PRESETS = {
    'fort': [(0.844, 0.744), (0.437, 0.503), (0.738, 0.918), (0.430, 0.572)],
}

# The image the index was parsed from: PE32, SizeOfImage 0x1e5b000, machine 0x14c.
IMAGE_SIZE = 0x1e5b000
IMAGE_BASE = 0x400000

# Vtable RVAs from notes/04-object-model.md (static half). Kept here so the tool still works
# if the JSON is missing, and so selftest can catch a changed index.
FOCUS = [
    ('Component_Player', 0x7af064),
    ('Component_RPG_Player', 0x7a1c0c),
    ('Component_Party_Member', 0x7b89ec),
    ('Component_Remote_Camera', 0x7b8434),
    ('Component_Actor', 0x7a0474),
    ('Component_Level_State', 0x7b8314),
    ('Component_Network', 0x7b2f3c),
    ('Component_Game_State', 0x7ae86c),
    ('Host_Object_Controller<Player_Object_Net_Data>', 0x7f7330),
    ('Ghost_Object_Controller<Player_Object_Net_Data>', 0x7f7100),
]

RADIUS = 0x400              # scan window around a candidate, both directions
IDLE_SECONDS = 1.0          # the still-pair that rejects animation noise
CHUNK = 4 * 1024 * 1024     # memory read granularity
# Series 1 skipped a single 141 MB `rw-` range because of this cap, and with it every focus
# class (the actor/component pool is one big allocation). Chunked reads mean a big range costs
# time, not memory, so the cap now sits far above anything this process holds.
RANGE_CAP = 512 * 1024 * 1024
SCAN_BUDGET = 1536 * 1024 * 1024

# How many outside-image hits are read as float windows, per class. Actor-heavy classes get
# more because the player body is one actor among many; `perClassFor` in the scan call must be
# at least as large, or the sample list is shorter than this and the cap never applies.
CANDIDATE_CAPS = {
    'Component_Player': 8,
    'Component_RPG_Player': 8,
    'Component_Party_Member': 8,
    'Component_Remote_Camera': 4,
    'Component_Actor': 48,
    'Component_Level_State': 3,
    'Component_Game_State': 3,
    'Component_Network': 4,
    'Host_Object_Controller<Player_Object_Net_Data>': 4,
    'Ghost_Object_Controller<Player_Object_Net_Data>': 4,
}
DEFAULT_CAP = 3
MAX_CANDIDATES = 112
SAMPLES_PER_CLASS = 64      # carried out of the payload, for the candidate choice above

SESSION_JS = r"""
const config = __CONFIG__;

let main = null;
let moduleReport = null;

function hex32(value) {
    return '0x' + (value >>> 0).toString(16);
}

function resolveMainModule() {
    const report = {candidates: [], chosen: null, reason: '', sizeOfImageMatches: null,
                    mainModuleName: null, mainModuleMatches: false};
    let modules = [];
    try { modules = Process.enumerateModules(); } catch (error) {
        report.reason = 'enumerateModules failed: ' + error;
        return report;
    }
    try { report.mainModuleName = Process.mainModule.name; } catch (error) { }
    const named = [];
    const pattern = new RegExp(config.moduleName || 'South Park', 'i');
    for (const module of modules) {
        if (!pattern.test(module.name)) continue;
        const entry = {name: module.name, size: module.size, verifiedPe: false};
        try {
            const lfanew = module.base.add(0x3c).readU32();
            entry.peOffset = lfanew;
            entry.verifiedPe = module.base.add(lfanew).readU32() === 0x4550;
            entry.machine = module.base.add(lfanew + 4).readU16();
            entry.sizeOfImage = module.base.add(lfanew + 0x50).readU32();
            entry.imageBase = module.base.add(
                lfanew + 0x18 + (entry.machine === 0x14c ? 0x1c : 0x18)).readU32();
        } catch (error) {
            entry.error = String(error);
        }
        report.candidates.push(entry);
        if (entry.verifiedPe && (config.machine === 0 || entry.machine === config.machine)) {
            named.push(entry);
        }
    }
    const exact = config.imageSize > 0
        ? named.filter(entry => entry.sizeOfImage === config.imageSize)
        : named;
    if (exact.length > 0) {
        report.chosen = exact[0];
        report.sizeOfImageMatches = true;
        report.ambiguous = exact.length;
    } else if (named.length === 1) {
        // PE32 by the right name and the only one: use it, but say the size did not match.
        report.chosen = named[0];
        report.sizeOfImageMatches = false;
    } else if (named.length === 0) {
        report.reason = 'no verified image matching /' + (config.moduleName || 'South Park') +
                        '/ among ' + report.candidates.length + ' name matches';
    } else {
        report.reason = named.length + ' candidate images and none with the indexed ' +
                        'SizeOfImage';
    }
    report.mainModuleMatches = report.chosen !== null &&
                               report.mainModuleName === report.chosen.name;
    return report;
}

if (!Module.loaded) {
    // A suspended spawn cannot read anything useful; the payload always loads post-resume.
}

moduleReport = resolveMainModule();
if (moduleReport.chosen !== null) {
    main = Process.enumerateModules().filter(
        module => module.name === moduleReport.chosen.name)[0];
}

function moduleInfo() {
    if (main === null) return {error: moduleReport.reason, report: moduleReport};
    const base = main.base;
    const slide = base.toInt32() >>> 0;
    return {
        name: main.name,
        size: main.size,
        path: main.path ? main.path.split('\\').pop() : null,
        verifiedPe: moduleReport.chosen.verifiedPe,
        machine: hex32(moduleReport.chosen.machine),
        sizeOfImage: hex32(moduleReport.chosen.sizeOfImage),
        imageBase: hex32(moduleReport.chosen.imageBase),
        atImageBase: (slide - (config.imageBase >>> 0)) === 0,
        slide: (slide - (config.imageBase >>> 0)) >>> 0,
        arch: Process.arch,
        pointerSize: Process.pointerSize
    };
}

function offsetOf(address) {
    return (address.sub(main.base).toInt32() >>> 0);
}

function scan(list, options) {
    if (main === null) return {error: moduleReport.reason};
    const perClass = options.perClassFor || 4;
    const maxMillis = options.maxMillis || 180000;
    const values = [];
    const labels = [];
    for (const item of list) {
        const label = item[0];
        const rva = item[1];
        const value = (main.base.add(rva).toInt32() >>> 0);
        values.push(value);
        labels.push(label);
    }
    const index = new Map();
    for (let i = 0; i < values.length; i++) {
        const value = values[i];
        if (index.has(value)) index.get(value).push(i);
        else index.set(value, [i]);
    }
    const counts = new Array(labels.length).fill(0);
    const inImage = new Array(labels.length).fill(0);
    const samples = labels.map(() => []);
    const started = Date.now();
    let rangesSeen = 0;
    let rangesUsed = 0;
    let rangesSkipped = 0;
    let scanned = 0;
    let skippedBytes = 0;
    let readErrors = 0;
    let truncated = false;
    let found;
    try { found = Process.enumerateRanges({protection: 'rw-', coalesce: true}); }
    catch (error) { return {error: 'enumerateRanges failed: ' + error}; }
    for (const range of found) {
        if (Date.now() - started > maxMillis) { truncated = true; break; }
        rangesSeen += 1;
        const size = range.size;
        if (size > config.rangeCap) { rangesSkipped += 1; skippedBytes += size; continue; }
        if (scanned + size > config.budget) { rangesSkipped += 1; skippedBytes += size; continue; }
        scanned += size;
        rangesUsed += 1;
        const base = range.base;
        for (let off = 0; off < size; off += config.chunk) {
            const length = Math.min(config.chunk, size - off) & ~3;
            if (length <= 0) continue;
            let buffer;
            try { buffer = base.add(off).readByteArray(length); }
            catch (error) { readErrors += 1; continue; }
            const words = new Uint32Array(buffer, 0, length >>> 2);
            for (let i = 0; i < words.length; i++) {
                const value = words[i];
                if (!index.has(value)) continue;
                for (const slot of index.get(value)) {
                    counts[slot] += 1;
                    const address = base.add(off + (i << 2));
                    const offset = offsetOf(address);
                    if (offset < main.size) {
                        inImage[slot] += 1;
                    } else if (samples[slot].length < perClass) {
                        samples[slot].push('0x' + offset.toString(16));
                    }
                }
            }
        }
    }
    const out = {labels: [], milliseconds: Date.now() - started,
                 scannedBytes: scanned, rangesSeen, rangesUsed, rangesSkipped, skippedBytes,
                 readErrors, truncated};
    for (let i = 0; i < labels.length; i++) {
        out.labels.push({label: labels[i], vtableRva: '0x' + (list[i][1]).toString(16),
                         count: counts[i], inImage: inImage[i],
                         outside: counts[i] - inImage[i], samples: samples[i]});
    }
    return out;
}

function readFloats(base, from, to) {
    const out = [];
    for (let at = from; at < to; at += 4) {
        let value = null;
        try {
            const raw = base.add(at).readFloat();
            value = (raw === raw && raw !== Infinity && raw !== -Infinity)
                ? Math.round(raw * 10000) / 10000 : null;
        } catch (error) { value = null; }
        out.push(value);
    }
    return out;
}

function readDoubles(base, from, to) {
    const out = [];
    for (let at = from; at < to; at += 8) {
        let value = null;
        try {
            const raw = base.add(at).readDouble();
            value = (raw === raw && raw !== Infinity && raw !== -Infinity)
                ? Math.round(raw * 10000) / 10000 : null;
        } catch (error) { value = null; }
        out.push(value);
    }
    return out;
}

function windows(offsets, radius) {
    if (main === null) return {error: moduleReport.reason};
    const out = {};
    for (const slot of offsets) {
        const address = addressOf(slot);
        if (address === null) { out[slot] = {error: 'unknown handle'}; continue; }
        out[slot] = {f: readFloats(address, -radius, radius),
                     d: readDoubles(address, -radius, radius)};
    }
    return {radius: radius, windows: out};
}

// Heap addresses are not expressible as module+offset when they sit below the image, so a
// chased pointer travels as a handle ('p12') and the payload keeps the real address. Nothing
// is written; the handle is just a lookup key.
const handles = [];

function addressOf(token) {
    if (typeof token === 'number') return main.base.add(token >>> 0);
    if (/^p\d+$/.test(token)) {
        const address = handles[parseInt(token.slice(1), 10)];
        return address === undefined ? null : address;
    }
    return main.base.add(parseInt(token.replace(/^0x/, ''), 16));
}

function pointerTargets(tokens, perToken) {
    if (main === null) return {error: moduleReport.reason};
    const baseValue = (main.base.toInt32() >>> 0);
    const out = [];
    for (const token of tokens) {
        const address = addressOf(token);
        if (address === null) continue;
        let taken = 0;
        for (let index = 0; index < 24 && taken < perToken; index++) {
            let value;
            try { value = address.add(index * 4).readU32(); } catch (error) { break; }
            if (value < 0x10000 || (value & 3) !== 0) continue;
            const relative = (value - baseValue) >>> 0;
            if (relative < main.size) continue;              // into the image: not an object
            let range = null;
            try { range = Process.findRangeByAddress(ptr(value)); } catch (error) { continue; }
            if (range === null || range.protection.indexOf('w') < 0) continue;
            if (handles.length >= 256) break;
            handles.push(ptr(value));
            out.push({token: 'p' + (handles.length - 1),
                      offset: relative > 0x7fffffff ? null : '0x' + relative.toString(16),
                      from: token, field: '0x' + (index * 4).toString(16)});
            taken += 1;
        }
    }
    return {targets: out, handles: handles.length};
}

function dwords(offset, count) {
    if (main === null) return {error: moduleReport.reason};
    const address = addressOf(offset);
    if (address === null) return {error: 'unknown handle'};
    const base = address;
    const fields = [];
    for (let i = 0; i < count; i++) {
        const at = i * 4;
        let value = null, asRva = null;
        try {
            value = base.add(at).readU32();
            const relative = (value - (main.base.toInt32() >>> 0)) >>> 0;
            if (relative < main.size) asRva = '0x' + relative.toString(16);
        } catch (error) { value = null; }
        fields.push({at: '0x' + at.toString(16), value: value === null ? null : hex32(value),
                     pointsIntoImageAt: asRva});
    }
    return {offset: typeof offset === 'string' ? offset : '0x' + offset.toString(16),
            fields: fields};
}

rpc.exports = {
    info: moduleInfo,
    scan: scan,
    windows: windows,
    pointers: pointerTargets,
    dwords: dwords
};
"""


# ---------------------------------------------------------------------------
# Index
# ---------------------------------------------------------------------------
def _index_key(classes, name):
    """The key in ``rtti_index.json`` for a focus name, template arguments and all."""
    if 'OEI::Game_Component::' + name in classes:
        return 'OEI::Game_Component::' + name
    if name in classes:
        return name
    leaf = name.split('<')[0]
    for key in classes:
        tail = key.split('::')[-1]
        if tail == name or tail.split('<')[0] == leaf:
            return key
    return None


def load_families(path=INDEX):
    """Focus name -> every index key whose RTTI base chain contains that focus class.

    This is what makes a scan find live objects at all: the vtable stored in an object is the
    leaf class's, never its base's (see "Which vtable to look for" above). Only the static index
    is read; nothing here touches the game.
    """
    data = json.loads(Path(path).read_text(encoding='utf-8'))
    classes = data['classes']
    parents = {name: [base['name'] for base in entry.get('base_classes') or []]
               for name, entry in classes.items()}

    def ancestors(name, seen=None):
        seen = set() if seen is None else seen
        if name in seen:
            return seen
        seen.add(name)
        for parent in parents.get(name, []):
            ancestors(parent, seen)
        return seen

    families = {}
    for name, _ in FOCUS:
        key = _index_key(classes, name)
        if key is None:
            families[name] = []
            continue
        families[name] = [other for other in classes
                          if other != key and key in ancestors(other)]
    return families


def load_index(path=INDEX):
    """The static index: label -> vtable RVA for every class that has one.

    Returns ``(focus, everything)`` where each is a list of ``(label, rva)``. Focus labels are
    prefixed ``focus:`` so a scan can report them separately without a second pass. The bare
    class name is the key in ``rtti_index.json``.
    """
    data = json.loads(Path(path).read_text(encoding='utf-8'))
    classes = data['classes']
    focus = []
    for name, rva in FOCUS:
        entry = classes.get('OEI::Game_Component::' + name) if '<' not in name else None
        if entry is None:
            for key, value in classes.items():
                if key == name or key.endswith('::' + name):
                    entry = value
                    break
        observed = [int(item, 16) for item in (entry or {}).get('vtable_rvas', [])]
        if observed and observed[0] != rva:
            raise SystemExit('index disagrees with the pinned RVA for %s: %s'
                             % (name, [hex(item) for item in observed]))
        focus.append(('focus:' + name, rva))
    everything = []
    for key, entry in classes.items():
        for item in entry.get('vtable_rvas', []):
            everything.append((key, int(item, 16)))
    return focus, everything


# ---------------------------------------------------------------------------
# Pure analysis (no game): movement
# ---------------------------------------------------------------------------
NOISE_MAX = 0.02        # a float that moves more than this while idle is not a position
MOVE_MIN = 0.15         # ... and this much to count as walking
COORD_LIMIT = 20000.0   # a level coordinate is nowhere near this


def _delta(a, b):
    out = []
    for left, right in zip(a, b):
        if left is None or right is None:
            out.append(None)
        else:
            out.append(right - left)
    return out


def _norm(vector):
    if not vector or any(item is None for item in vector):
        return None
    return sum(item * item for item in vector) ** 0.5


def _dot(left, right):
    if not left or not right:
        return None
    if any(item is None for item in left) or any(item is None for item in right):
        return None
    return sum(a * b for a, b in zip(left, right))


def _plausible(values):
    for value in values:
        if value is None:
            return False
        if abs(value) > COORD_LIMIT:
            return False
    # A triple that is exactly 0,0,0 at every snapshot is a null/gap, not a position.
    if all(abs(value) < 1e-9 for value in values):
        return False
    return True


def movement_analysis(snapshots, radius=RADIUS, kinds=('f', 'd'), limit=12):
    """Find float/double triples that behave like a position.

    ``snapshots`` maps a name to ``{offset: {'f': [...], 'd': [...]}}`` as returned by the
    payload; the names must be ``idle_before, idle_after, w, s, a, d``. The idle pair is the
    noise baseline. A triple is a candidate when it is still when idle and its three values
    all move in the four walks, with ``w`` and ``s`` opposed (the character walked back).

    Returns a list of candidate dicts sorted by score, best first. Pure function: the
    selftest builds snapshots by hand.
    """
    order = ['idle_before', 'idle_after', 'w', 's', 'a', 'd']
    for name in order:
        if name not in snapshots:
            return []
    offsets = [key for key in snapshots['idle_before'] if key in snapshots['w']]
    results = []
    for kind in kinds:
        step = 4 if kind == 'f' else 8
        for offset in offsets:
            series = {}
            missing = False
            for name in order:
                values = (snapshots[name].get(offset) or {}).get(kind)
                if not values:
                    missing = True
                    break
                series[name] = values
            if missing:
                continue
            length = min(len(values) for values in series.values())
            for index in range(0, max(0, length - 2)):
                window = {name: series[name][index:index + 3] for name in order}
                noise = _delta(window['idle_before'], window['idle_after'])
                noise_norm = _norm(noise)
                if noise_norm is None or noise_norm > NOISE_MAX:
                    continue
                moves = {}
                good = True
                for axis, previous in (('w', 'idle_after'), ('s', 'w'), ('a', 's'), ('d', 'a')):
                    vector = _delta(window[previous], window[axis])
                    size = _norm(vector)
                    if size is None or size < MOVE_MIN:
                        good = False
                        break
                    moves[axis] = {'delta': [None if item is None else round(item, 4)
                                             for item in vector],
                                   'size': round(size, 4)}
                if not good:
                    continue
                if not all(_plausible(window[name]) for name in order):
                    continue
                forward = moves['w']['delta']
                back = moves['s']['delta']
                opposition = _dot(forward, back)
                if opposition is None or opposition >= 0:
                    # `s` did not walk back along the same direction: not a translation.
                    continue
                cosine = opposition / (moves['w']['size'] * moves['s']['size'])
                sides = min(moves['a']['size'], moves['d']['size'])
                score = min(moves['w']['size'], moves['s']['size']) * (1.0 - cosine) * (
                    1.0 + sides / max(moves['w']['size'], 1e-6))
                window_offset = (index * step) - radius
                results.append({
                    'kind': 'float32' if kind == 'f' else 'float64',
                    'name': None,
                    'candidate': offset,
                    'offset_from_candidate': window_offset,
                    'score': round(score, 3),
                    'opposition_cosine': round(cosine, 3),
                    'idle_delta': round(noise_norm, 6),
                    'values': {name: window[name] for name in order},
                    'moves': moves,
                })
    results.sort(key=lambda item: -item['score'])
    return results[:limit]


def big_movers(before, after, radius=RADIUS, kinds=('f',), limit=8, minimum=0.5):
    """Anything that changed at all between two windows - the "did input reach the game" check.

    Weaker than `movement_analysis` (no idle baseline, no opposition test), so it is only
    reported when the strict test found nothing.
    """
    results = []
    for kind in kinds:
        step = 4 if kind == 'f' else 8
        for candidate, window in before.items():
            other = after.get(candidate) or {}
            left = window.get(kind)
            right = other.get(kind)
            if not left or not right:
                continue
            for index in range(0, max(0, min(len(left), len(right)) - 2)):
                triple_before = left[index:index + 3]
                triple_after = right[index:index + 3]
                if any(item is None for item in triple_before + triple_after):
                    continue
                size = _norm(_delta(triple_before, triple_after))
                if size is None or size < minimum:
                    continue
                results.append({'candidate': candidate, 'kind': kind,
                                'offset_from_candidate': index * step - radius,
                                'delta': round(size, 3),
                                'before': triple_before, 'after': triple_after})
    results.sort(key=lambda item: -item['delta'])
    return results[:limit]


def screen_changes(record):
    """dhash equality between the walk frames: proof of whether the screen moved at all."""
    frames = {sample['tag']: sample['capture'] for sample in record.get('samples') or []
              if sample.get('capture', {}).get('captured')}
    pairs = []
    tags = ['before-walk', 'after-w', 'after-s', 'after-a', 'after-d']
    for previous, current in zip(tags, tags[1:]):
        if previous in frames and current in frames:
            pairs.append({'from': previous, 'to': current,
                          'same_dhash': frames[previous].get('dhash') == frames[current].get('dhash'),
                          'dhash': [frames[previous].get('dhash'), frames[current].get('dhash')],
                          'mean_luma': [frames[previous].get('mean_luma'),
                                        frames[current].get('mean_luma')]})
    return pairs


def summarise_analysis(results, top=6):
    """A short printable form of `movement_analysis` results."""
    lines = []
    for entry in results[:top]:
        values = entry['values']
        lines.append('%s at candidate%s %+d  score %.3f  cos(W,S)=%.2f' % (
            entry['kind'], entry['candidate'], entry['offset_from_candidate'],
            entry['score'], entry['opposition_cosine']))
        lines.append('    idle   %s  ->  %s' % (values['idle_before'], values['idle_after']))
        lines.append('    W      %s  (delta %s)' % (values['w'], entry['moves']['w']['delta']))
        lines.append('    S      %s  (delta %s)' % (values['s'], entry['moves']['s']['delta']))
        lines.append('    A      %s  (delta %s)' % (values['a'], entry['moves']['a']['delta']))
        lines.append('    D      %s  (delta %s)' % (values['d'], entry['moves']['d']['delta']))
    return '\n'.join(lines)


# ---------------------------------------------------------------------------
# Launch series (guard + lock + session)
# ---------------------------------------------------------------------------
def preflight():
    """The three checks steps 2 and 3 make before every launch."""
    if not EXE.is_file():
        raise SystemExit('game copy not found: ' + str(EXE))
    if hashlib.sha256(EXE.read_bytes()).hexdigest() != EXPECTED_SHA256:
        raise SystemExit('unexpected game executable; refusing launch')
    if not ORIGINAL_PROFILE.is_dir():
        raise SystemExit('expected original profile not found; refusing launch')
    if ctypes.windll.user32.FindWindowW(WINDOW_TITLE, None):
        raise SystemExit('a South Park window already exists; refusing to interfere')


def _configure_payload(radius=RADIUS, chunk=CHUNK, range_cap=RANGE_CAP, budget=SCAN_BUDGET,
                       module_name='South Park', image_size=IMAGE_SIZE, machine=0x14c):
    return {
        'imageSize': image_size, 'imageBase': IMAGE_BASE, 'radius': radius,
        'chunk': chunk, 'rangeCap': range_cap, 'budget': budget,
        'moduleName': module_name, 'machine': machine,
    }


def _capture(pid, run, tag, record):
    path = run / 'frames' / ('frame-%s.png' % tag)
    frame = capture_game_window(pid, path)
    record.setdefault('samples', []).append({
        'tag': tag,
        'at': round(time.time() - record['started_wall'], 1),
        'capture': frame,
        'screen_class': in_game.classify_frame(frame),
        **in_game.process_sample(pid)})
    return frame


def _probe(script, record, tag):
    try:
        probe = script.exports_sync.probe(in_game.PROBE_NEEDLES, in_game.AMX_HEADER_PATTERN)
    except Exception as error:                      # noqa: BLE001 - reported, not raised
        probe = {'error': repr(error)}
    probe['tag'] = tag
    probe['at'] = round(time.time() - record['started_wall'], 1)
    record.setdefault('probes', []).append(probe)
    return probe


def _choose_candidates(scan, wanted=None, families=None):
    """Outside-image hits for each wanted class, its rarest-derived family first.

    ``families`` comes from :func:`load_families`. Members are ordered by how many live
    instances the scan saw, fewest first: a subsystem's own object is usually a single
    instance, the shared base class is the hundreds. Per-class cap applies to the whole
    family, and ``focus:<name>`` (the base vtable itself) is kept first for the record.
    Returns ``(chosen, classes)`` where ``classes`` maps a token to the class that matched it.
    """
    wanted = wanted or [name for name, _ in FOCUS]
    families = families if families is not None else {}
    by_label = {entry['label']: entry for entry in (scan.get('labels') or [])}
    chosen, classes = [], {}
    for name in wanted:
        labels = ['focus:' + name] + [member for member in families.get(name) or []]
        present = [by_label[label] for label in labels if label in by_label]
        # Keep the focus label first, then family members with fewest instances first.
        present[1:] = sorted(present[1:], key=lambda entry: (entry['outside'], entry['label']))
        cap = CANDIDATE_CAPS.get(name, DEFAULT_CAP)
        taken = 0
        for entry in present:
            for offset in entry['samples']:
                if taken >= cap or len(chosen) >= MAX_CANDIDATES:
                    break
                if offset not in chosen:
                    chosen.append(offset)
                    classes[offset] = entry['label']
                taken += 1
            if taken >= cap or len(chosen) >= MAX_CANDIDATES:
                break
    return chosen, classes


def _describe(record, token):
    """Map a watched token back to what it is: an offset from module base, or a chased pointer."""
    classes = record.get('candidate_classes') or {}
    for entry in record.get('chased') or []:
        if entry['token'] == token:
            return {'chased_from': entry['from'], 'chased_field': entry['field'],
                    'offset': entry.get('offset'), 'class': classes.get(entry['from'])}
    return {'offset': token if str(token).startswith('0x') else None,
            'class': classes.get(token)}


def _click_load_save(pid, plan, record):
    """Load a save from the game's own load list: menu, row, load, confirm.

    Each step needs a hover before the click: the menu ignores a click that arrives in the
    same frame as the cursor move. Fractions come from the Russian UI at 1280x720 and are
    recorded in the run so a different layout can be corrected in one place.
    """
    steps = plan.get('load_save') or []
    window = find_game_window(pid)
    if window is None:
        record['load_error'] = 'no game window'
        return False
    user = api().user
    clicks = []
    for fx, fy in steps:
        focus_window(window['hwnd'])
        point = wintypes.POINT(int(window['width'] * fx), int(window['height'] * fy))
        user.ClientToScreen(window['hwnd'], ctypes.byref(point))
        user.SetCursorPos(point.x, point.y)
        time.sleep(0.6)
        user.mouse_event(0x0002, 0, 0, 0, 0)
        time.sleep(0.1)
        user.mouse_event(0x0004, 0, 0, 0, 0)
        time.sleep(1.2)
        _capture(pid, Path(record['run']), 'load-step-%d' % (len(clicks) + 1), record)
        clicks.append({'fraction': [fx, fy], 'screen': [point.x, point.y]})
    record['load_clicks'] = clicks
    return True


def _wait_for_level(pid, run, record, plan):
    """Enter a level: by clicking Continue, or by loading a save chosen from the list."""
    if plan.get('load_save'):
        if not _click_load_save(pid, plan, record):
            return False
        # The load is deterministic when the four clicks land: list, confirm, black, world.
        # Wait it out rather than guessing from frame fingerprints (the world frame is not
        # in the front-end reference set, and a still-visible list must not be mistaken for it).
        time.sleep(plan.get('load_wait', 55.0))
        frame = _capture(pid, run, 'level', record)
        return bool(frame.get('captured'))
    record['click'] = click_client(pid, 0.844, 0.657)
    deadline = time.time() + plan['level_timeout']
    while time.time() < deadline:
        time.sleep(plan['level_poll'])
        frame = _capture(pid, run, 'loading-%d' % int(time.time() - record['started_wall']),
                         record)
        if not frame.get('captured') or frame.get('nonblack_ratio', 0) < 0.02:
            continue
        grid = frame.get('luma_grid') or []
        level = max([in_game.grid_correlation(grid, reference)
                     for reference in in_game.REFERENCES['gameplay']] or [-1.0])
        if level >= in_game.FRONT_END_CORRELATION:
            record['level_correlation'] = round(level, 4)
            return True
    return False


def session_body(pid, guard_script, plan, record):
    """Runs after resume. Everything read-only; called from `run_guarded(during=...)`."""
    device = frida.get_local_device()
    session = None
    probe_script = None
    om_script = None
    run = Path(record['run'])
    try:
        session = device.attach(pid)
        probe_script = session.create_script(in_game.PROBE_JS, name='object-model-probe')
        probe_script.load()
        om_script = session.create_script(
            SESSION_JS.replace('__CONFIG__', json.dumps(plan['payload'])),
            name='object-model')
        om_script.on('message', lambda message, data: record.setdefault(
            'payload_errors', []).append(str(message)[:600]))
        om_script.load()
        api = om_script.exports_sync

        info = api.info()
        record['module'] = info
        if 'error' in info:
            record['error'] = 'module not identified: ' + str(info.get('error'))
            return record

        # 1. front end: the character does not exist yet, so the scan is the "menu" baseline.
        wait = plan['warmup'] - (time.time() - record['started_wall'])
        if wait > 0:
            time.sleep(wait)
        _capture(pid, run, 'menu', record)
        _probe(probe_script, record, 'menu')
        record['menu_scan'] = api.scan(plan['all_targets'], {'perClassFor': 4})
        record['menu_scan_at'] = round(time.time() - record['started_wall'], 1)

        if plan['mode'] == 'scan' and not plan['skip_level']:
            # 2. click "Continue" (step 2's fraction) and wait for a level frame.
            if not _wait_for_level(pid, run, record, plan):
                record['level_seen'] = False
                record['error'] = 'never saw a level frame; level scan skipped'
                return record
            record['level_seen'] = True
            time.sleep(plan['settle'])
            _capture(pid, run, 'level', record)
            _probe(probe_script, record, 'level')
            record['level_scan'] = api.scan(plan['all_targets'],
                                            {'perClassFor': SAMPLES_PER_CLASS})
            record['level_scan_at'] = round(time.time() - record['started_wall'], 1)
        elif plan['mode'] == 'watch':
            # `watch` starts from the same click but scans only the requested classes.
            record['level_seen'] = _wait_for_level(pid, run, record, plan)
            time.sleep(plan['settle'])
            record['level_scan'] = api.scan(plan['all_targets'],
                                            {'perClassFor': SAMPLES_PER_CLASS})
            record['level_scan_at'] = round(time.time() - record['started_wall'], 1)

        # 3. behaviour: the candidate objects' neighbourhood, before and after real input.
        if plan['mode'] == 'watch' or not plan['skip_walk']:
            candidates, candidate_classes = _choose_candidates(
                record.get('level_scan') or {}, wanted=plan['wanted'],
                families=load_families())
            record['candidates'] = candidates
            record['candidate_classes'] = candidate_classes
            if not candidates:
                record['error'] = 'no outside-image instance to watch; walk test skipped'
                return record
            # The body may be the owning object, not the component itself: follow one pointer
            # out of each of the first few candidates and watch those objects too.
            chased = api.pointers(candidates[:48], 2)
            record['chased'] = chased.get('targets') or []
            watched = candidates + [entry['token'] for entry in record['chased']]
            record['watched'] = watched
            snapshots = {}
            # One space press: leaves an intro dialogue if there is one, and is the cheapest
            # evidence that key input arrives at the game at all (see the frame dhash).
            record.setdefault('input', []).append(
                {'axis': 'space', **send_keys(pid, ['space'])})
            time.sleep(plan['settle'])
            _capture(pid, run, 'before-walk', record)
            snapshots['idle_before'] = api.windows(watched, plan['radius'])['windows']
            time.sleep(plan['idle'])
            snapshots['idle_after'] = api.windows(watched, plan['radius'])['windows']
            for axis, seconds in (('w', plan['hold']), ('s', plan['hold']),
                                  ('a', plan['hold']), ('d', plan['hold'])):
                record.setdefault('input', []).append(
                    {'axis': axis, **hold_key(pid, axis, seconds)})
                snapshots[axis] = api.windows(watched, plan['radius'])['windows']
                _capture(pid, run, 'after-' + axis, record)
            record['snapshots'] = snapshots
            analysis = movement_analysis(snapshots, radius=plan['radius'])
            record['movement'] = analysis
            if not analysis and plan.get('retry', True):
                # Nothing moved. Longer hold, and record whether anything moved at all:
                # a moving camera with a still object set would put the body outside the
                # window; a still screen means the input never reached a controllable body.
                record.setdefault('input', []).append(
                    {'axis': 'w-retry', **hold_key(pid, 'w', 3.0)})
                record['retry_window'] = api.windows(watched, plan['radius'])['windows']
                _capture(pid, run, 'after-retry', record)
                record['retry_movers'] = big_movers(snapshots['d'], record['retry_window'],
                                                    radius=plan['radius'], limit=8)
            record['screen_changes'] = screen_changes(record)
            if analysis:
                best = analysis[0]
                record['player_candidate'] = {
                    'candidate': best['candidate'],
                    'kind': best['kind'],
                    'offset_from_candidate': best['offset_from_candidate'],
                    'score': best['score'],
                    'values': best['values'],
                    **_describe(record, best['candidate']),
                }
                record['winner_dwords'] = api.dwords(best['candidate'], 24)
        return record
    except Exception as error:                      # noqa: BLE001 - a failed run is data
        record['error'] = repr(error)
        return record
    finally:
        for script in (om_script, probe_script):
            if script is not None:
                try:
                    script.unload()
                except Exception:                   # noqa: BLE001
                    pass
        if session is not None:
            try:
                session.detach()
            except Exception:                       # noqa: BLE001
                pass


def select_targets(wanted=None):
    """``(focus, everything)`` vtable targets for a series, after the ``--class`` filter.

    Everything is kept unscoped for ``scan`` because a full index costs one pass either way.
    For ``--class`` the filter keeps the class's derived leaves (``load_families``): the base
    vtable alone has no live instance, so scanning only it would always find zero.
    """
    focus, everything = load_index()
    families = load_families()
    if not wanted:
        return focus, everything
    wanted_names = set(wanted)
    unknown = sorted(name for name in wanted_names if name not in families)
    if unknown:
        raise SystemExit('not a focus class: ' + ', '.join(unknown))
    focus = [(label, rva) for label, rva in focus
             if label.split(':', 1)[1] in wanted_names]
    everything = [(label, rva) for label, rva in everything
                  if any(label == name or label.endswith('::' + name)
                         or label in (families.get(name) or [])
                         for name in wanted_names)]
    if not focus and not everything:
        raise SystemExit('no vtable RVA for ' + ', '.join(sorted(wanted_names)))
    return focus, everything


def run_series(mode='scan', wanted=None, skip_walk=False, skip_level=False, warmup=32.0,
               settle=3.0, hold=1.2, idle=IDLE_SECONDS, radius=RADIUS, owner='step04-objects',
               minutes=45, run_root=None, load_save=None, load_wait=35.0):
    """One guarded launch series: lock, isolated copy, guard, session, run folder."""
    preflight()
    focus, everything = select_targets(wanted)
    all_targets = [list(item) for item in focus] if mode != 'scan' else \
        [list(item) for item in focus + everything]

    if launch_lock.acquire(owner, minutes=minutes,
                           wait=float(os.environ.get('SP_LOCK_WAIT', '1800')),
                           pid=os.getpid()) != 0:
        raise SystemExit('another agent holds the launch lock; refusing to launch')
    try:
        run = Path(run_root or RUN_ROOT) / ('object-model-' + uuid.uuid4().hex[:8])
        run.mkdir(parents=True)
        (run / 'frames').mkdir()
        profile_before = in_game.snapshot(ORIGINAL_PROFILE)
        profile = run / 'test-profile'
        backup = run / 'original-profile-backup'
        shutil.copytree(ORIGINAL_PROFILE, backup)
        shutil.copytree(backup, profile)
        if (in_game.snapshot(backup) != profile_before
                or in_game.snapshot(ORIGINAL_PROFILE) != profile_before):
            raise SystemExit('profile changed while copying it into the run folder')
        temporary = run / 'runtime-files' / 'temp'
        temporary.mkdir(parents=True)
        config = {
            'originalSaves': str(ORIGINAL_PROFILE), 'testSaves': str(profile),
            'writeRoots': [str(GAME), str(run)],
            'redirections': in_game._install_redirections(run),
            'nativeCandidates': [], 'traceNatives': [], 'scriptSignatures': [],
            'blockRemoteStorage': False,
            'logPaths': ['.sav', '.bk2', 'movies', 'console_scripts'],
        }
        game_before = in_game.inventory(GAME)
        record = {
            'tool': 'object_model.py', 'mode': mode,
            'run': str(run), 'argv': [str(EXE), '-window'],
            'started_wall': time.time(),
            'started': time.strftime('%Y-%m-%dT%H:%M:%S'),
            'wanted': sorted(wanted) if wanted else None,
            'focus': [[label, hex(rva)] for label, rva in focus],
            'targets': len(all_targets),
            'plan': {'warmup': warmup, 'settle': settle, 'hold': hold, 'idle': idle,
                     'radius': radius, 'level_timeout': 90.0, 'level_poll': 4.0,
                     'skip_walk': skip_walk, 'skip_level': skip_level,
                     'load_save': load_save, 'load_wait': load_wait},
        }
        plan = {
            'mode': mode, 'wanted': wanted, 'skip_walk': skip_walk, 'skip_level': skip_level,
            'warmup': warmup, 'settle': settle, 'hold': hold, 'idle': idle, 'radius': radius,
            'level_timeout': 90.0, 'level_poll': 4.0, 'all_targets': all_targets,
            'load_save': load_save, 'load_wait': load_wait,
            'payload': _configure_payload(radius=radius),
        }
        result = {}
        try:
            result = run_guarded(
                [str(EXE), '-window'], config, duration=2, cwd=GAME,
                environment={'SteamAppId': '213670', 'SteamGameId': '213670',
                             'TEMP': str(temporary), 'TMP': str(temporary)},
                during=lambda pid, guard: session_body(pid, guard, plan, record))
        except Exception as error:                  # noqa: BLE001 - a failed run is data
            record['launcher_error'] = repr(error)
        finally:
            profile_after = in_game.snapshot(ORIGINAL_PROFILE)
            game_after = in_game.inventory(GAME)
            record['pid'] = result.get('pid')
            record['guard'] = {
                'resumed': result.get('resumed'),
                'exited_before_deadline': result.get('exited_before_deadline'),
                'detach_reason': result.get('detach_reason'),
                'crash': result.get('crash'),
                'errors': result.get('errors'),
                'events': [event for event in (result.get('events') or [])
                           if event.get('kind') in ('redirect', 'blocked-write',
                                                    'blocked-write-open', 'path-open',
                                                    'guard-error')][:80],
                'telemetry_counters': (result.get('telemetry') or {}).get('counters'),
            }
            record['new_game_files'] = sorted(name for name in game_after
                                              if name not in game_before)
            record['validity'] = {
                'original_profile_unchanged': profile_before == profile_after,
                'guard_integrity': not result.get('errors') and not any(
                    event.get('kind') == 'guard-error'
                    for event in (result.get('events') or [])),
                'original_exe_hash_still_matches':
                    hashlib.sha256(EXE.read_bytes()).hexdigest() == EXPECTED_SHA256,
            }
            record['validity']['run_valid'] = all(record['validity'].values())
            record['in_game'] = in_game.decide(record)
            record.pop('started_wall', None)
            (run / 'result.json').write_text(json.dumps(record, indent=2, default=str),
                                             encoding='utf-8')
        return record
    finally:
        launch_lock.release(owner)


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------
def class_table(record):
    """Per-class live instance counts at the menu and in the level, focus first.

    The focus name's own vtable is often never instantiated: the live objects use a derived
    class from the same family (`DS_Component_Player` for `Component_Player`). The cell sums
    the family and names the class that actually has the instances, so a 0 in the focus row
    can never again hide a live player object.
    """
    menu = {entry['label']: entry for entry in (record.get('menu_scan') or {}).get('labels', [])}
    level = {entry['label']: entry for entry in (record.get('level_scan') or {}).get('labels', [])}
    families = load_families()

    def cell(mapping, key):
        entry = mapping.get(key)
        if entry is None:
            return '-'
        outside = entry.get('outside', 0)
        in_image = entry.get('inImage', 0)
        contributors = []
        for member in families.get(key.split(':', 1)[1], []):
            other = mapping.get(member)
            if not other:
                continue
            outside += other.get('outside', 0)
            in_image += other.get('inImage', 0)
            if other.get('outside'):
                contributors.append((member, other['outside']))
        text = '%d (%d image)' % (outside, in_image)
        if contributors:
            best = max(contributors, key=lambda item: item[1])[0]
            text += ' ' + best.split('::')[-1]
        return text

    rows = []
    for label, rva in FOCUS:
        rows.append((label, hex(rva), cell(menu, 'focus:' + label), cell(level, 'focus:' + label)))
    return rows


def top_hits(record, scan='level_scan', count=20):
    """Biggest outside-image counts for the whole index, as a printable list."""
    labels = (record.get(scan) or {}).get('labels') or []
    ranked = sorted(labels, key=lambda entry: -entry['outside'])
    return [(entry['label'], entry['vtableRva'], entry['outside'], entry['inImage'])
            for entry in ranked if entry['outside'] > 0][:count]


def print_scan(record):
    module = record.get('module') or {}
    print('run      : %s' % record.get('run'))
    print('module   : %s size %s slide %s atImageBase %s arch %s' % (
        module.get('name'), module.get('sizeOfImage'), module.get('slide'),
        module.get('atImageBase'), module.get('arch')))
    validity = record.get('validity') or {}
    print('validity : profile=%s guard=%s exe=%s' % (
        validity.get('original_profile_unchanged'), validity.get('guard_integrity'),
        validity.get('original_exe_hash_still_matches')))
    print('in_game  : %s' % (record.get('in_game') or {}).get('in_game'))
    menu_scan = record.get('menu_scan') or {}
    level_scan = record.get('level_scan') or {}
    print('scan     : menu %s classes in %s ms over %s bytes (%s ranges); '
          'level %s classes in %s ms over %s bytes (%s ranges)' % (
              len(menu_scan.get('labels') or []), menu_scan.get('milliseconds'),
              menu_scan.get('scannedBytes'), menu_scan.get('rangesUsed'),
              len(level_scan.get('labels') or []), level_scan.get('milliseconds'),
              level_scan.get('scannedBytes'), level_scan.get('rangesUsed')))
    print('')
    print('%-46s %-10s %-16s %-16s' % ('class', 'vtable', 'menu outside(img)', 'level outside(img)'))
    for label, rva, menu, level in class_table(record):
        print('%-46s %-10s %-16s %-16s' % (label, rva, menu, level))
    print('')
    best = record.get('player_candidate')
    if best:
        print('player candidate: %s (%s) position at %+d from it, score %s' % (
            best.get('offset') or best.get('candidate'), best['kind'],
            best['offset_from_candidate'], best['score']))
        if best.get('class'):
            print('   (vtable is %s)' % best['class'])
        if best.get('chased_from'):
            print('   (reached by following field %s of %s)' % (
                best['chased_field'], best['chased_from']))
        print(summarise_analysis(record.get('movement') or [], top=3))
    else:
        print('player candidate: none')
        movers = record.get('retry_movers') or []
        print('3 s W retry: %d triple(s) moved by more than 0.5' % len(movers))
        for entry in movers[:4]:
            print('   %s %+d  %s -> %s' % (entry['candidate'], entry['offset_from_candidate'],
                                           entry['before'], entry['after']))
        print('warnings: %s' % (record.get('in_game') or {}).get('warnings'))
    watched = record.get('watched') or record.get('candidates') or []
    print('watched  : %d objects (%d of them chased pointers)' % (
        len(watched), len(record.get('chased') or [])))
    classes = record.get('candidate_classes') or {}
    counts = {}
    for token in record.get('candidates') or []:
        label = classes.get(token) or '?'
        counts[label] = counts.get(label, 0) + 1
    for label in sorted(counts):
        print('   %-3d %s' % (counts[label], label))
    for entry in record.get('screen_changes') or []:
        print('screen   : %s -> %s  same dhash=%s' % (
            entry['from'], entry['to'], entry['same_dhash']))


def replay(run_dir):
    """Re-print a stored run; no launch, no lock."""
    record = json.loads((Path(run_dir) / 'result.json').read_text(encoding='utf-8'))
    record.setdefault('started_wall', 0)
    print_scan(record)
    return 0


# ---------------------------------------------------------------------------
# Selftest
# ---------------------------------------------------------------------------
def _synthetic_snapshots():
    """A candidate whose floats 100..102 are a position, plus noise and a still object."""
    offsets = ['0x1000', '0x2000']
    radius = 256
    length_f = radius * 2 // 4
    length_d = radius * 2 // 8
    rng = 12345

    def noise():
        nonlocal rng
        rng = (1103515245 * rng + 12345) & 0x7fffffff
        return (rng % 1000) / 1000.0

    position = {'idle_before': (10.0, 20.0, 30.0), 'idle_after': (10.0, 20.0, 30.0),
                'w': (12.0, 20.0, 26.0), 's': (10.0, 20.0, 30.0),
                'a': (16.0, 20.0, 30.0), 'd': (12.0, 20.0, 26.0)}
    snapshots = {}
    for name, values in position.items():
        floats = [100.0 + noise() for _ in range(length_f)]
        floats[100], floats[101], floats[102] = values
        doubles = [200.0 + noise() for _ in range(length_d)]
        second = [5.0 + noise() for _ in range(length_f)]
        snapshots[name] = {offsets[0]: {'f': floats, 'd': doubles},
                           offsets[1]: {'f': second, 'd': [1.0] * length_d}}
    return snapshots, radius, offsets


def _refuses(call):
    """True when `call` raises SystemExit/ValueError, i.e. the tool refused bad input."""
    try:
        call()
    except (SystemExit, ValueError, KeyError):
        return True
    return False


def selftest():
    checks = []

    def check(name, condition, detail=''):
        checks.append((name, bool(condition), detail))

    focus, everything = load_index()
    check('index: 10 focus classes', len(focus) == 10, str(len(focus)))
    check('index: 929 vtables in the whole index', len(everything) == 929, str(len(everything)))
    for name, rva in FOCUS:
        check('index: %s rva matches' % name,
              ('focus:' + name, rva) in focus, hex(rva))

    snapshots, radius, offsets = _synthetic_snapshots()
    results = movement_analysis(snapshots, radius=radius)
    check('movement: a triple is found', len(results) >= 1, str(len(results)))
    if results:
        best = results[0]
        check('movement: the position triple wins', best['candidate'] == offsets[0]
              and best['offset_from_candidate'] == 100 * 4 - radius,
              '%s %+d' % (best['candidate'], best['offset_from_candidate']))
        check('movement: it is read as float32', best['kind'] == 'float32', best['kind'])
        check('movement: W and S oppose each other', best['opposition_cosine'] <= 0,
              str(best['opposition_cosine']))
    check('movement: the still object is rejected',
          all(entry['candidate'] != offsets[1] for entry in results))
    check('movement: no snapshot means no result', movement_analysis({}) == [])

    # A triple that changes while idle must not pass the noise gate.
    noisy = {}
    for name, values in snapshots.items():
        noisy[name] = {key: {'f': list(value['f']), 'd': list(value['d'])}
                       for key, value in values.items()}
    noisy['idle_after'][offsets[0]]['f'][100] += 5.0
    check('movement: idle motion disqualifies',
          all(entry['offset_from_candidate'] != 100 * 4 - radius
              for entry in movement_analysis(noisy, radius=radius)))

    # Reporting helpers on a synthetic record, no game involved.
    record = {'menu_scan': {'labels': [
        {'label': 'focus:Component_Player', 'vtableRva': '0x7af064', 'count': 2,
         'inImage': 1, 'outside': 1, 'samples': []}]},
        'level_scan': {'labels': [
            {'label': 'focus:Component_Player', 'vtableRva': '0x7af064', 'count': 9,
             'inImage': 1, 'outside': 8, 'samples': ['0x1000']},
            {'label': 'Some::Class', 'vtableRva': '0x700000', 'count': 5,
             'inImage': 5, 'outside': 0, 'samples': []}]},
        'module': {'name': 'South Park', 'sizeOfImage': '0x1e5b000', 'slide': 0,
                   'atImageBase': True, 'arch': 'ia32'},
        'validity': {'original_profile_unchanged': True, 'guard_integrity': True,
                     'original_exe_hash_still_matches': True}}
    rows = class_table(record)
    check('report: one row per focus class', len(rows) == len(FOCUS), str(len(rows)))
    check('report: counts are outside-image and in-image',
          rows[0][2] == '1 (1 image)' and rows[0][3] == '8 (1 image)', str(rows[0]))
    hits = top_hits(record)
    check('report: top hits skip classes with no outside hit',
          [item[0] for item in hits] == ['focus:Component_Player'], str(hits))
    check('report: the summary prints the triple',
          'idle' in summarise_analysis(results, top=1), '')
    chosen, chosen_classes = _choose_candidates(
        record['level_scan'], wanted=['Component_Player', 'Component_Actor'])
    check('candidates: chosen from outside-image samples', chosen == ['0x1000'], str(chosen))
    check('candidates: each token keeps its class',
          chosen_classes == {'0x1000': 'focus:Component_Player'}, str(chosen_classes))

    # The base vtable is dead; the live object carries a derived vtable. Candidates must come
    # from the family, and the rarest family member must be tried first.
    families = load_families()
    check('index: Component_Player has a derived family',
          'OEI::Game_Component::DS_Component_Player' in families.get('Component_Player', []),
          str(families.get('Component_Player')))
    check('index: Component_Actor has a derived family',
          'OEI::Game_Component::NY_Component_Actor' in families.get('Component_Actor', []),
          str(len(families.get('Component_Actor') or [])))
    check('index: a focus class is not in its own family',
          'OEI::Game_Component::Component_Actor' not in families.get('Component_Actor', []))
    check('index: every focus class has a family key', len(families) == len(FOCUS),
          str(len(families)))
    derived_scan = {'labels': [
        {'label': 'focus:Component_Actor', 'count': 0, 'inImage': 0, 'outside': 0,
         'samples': []},
        {'label': 'OEI::Game_Component::NY_Component_Actor', 'count': 35, 'inImage': 0,
         'outside': 35, 'samples': ['0x%x' % (0x2000 + 0x10 * i) for i in range(35)]},
        {'label': 'OEI::Game_Component::DS_Component_Player', 'count': 1, 'inImage': 0,
         'outside': 1, 'samples': ['0x3000']},
        {'label': 'OEI::Game_Component::DS_Component_Actor_Extra', 'count': 3, 'inImage': 0,
         'outside': 3, 'samples': ['0x4000', '0x4010', '0x4020']}]}
    derived = {'Component_Actor': ['OEI::Game_Component::NY_Component_Actor',
                                   'OEI::Game_Component::DS_Component_Actor_Extra']}
    actor_chosen, actor_classes = _choose_candidates(
        derived_scan, wanted=['Component_Actor'], families=derived)
    check('candidates: family hits are used when the base vtable has none',
          '0x3000' not in actor_chosen and actor_chosen and actor_classes[actor_chosen[0]],
          str(actor_chosen[:3]))
    check('candidates: rarest family member first',
          actor_classes[actor_chosen[0]] == 'OEI::Game_Component::DS_Component_Actor_Extra',
          str(actor_classes.get(actor_chosen[0])))
    check('candidates: the per-class cap covers the whole family',
          len(actor_chosen) == 38, str(len(actor_chosen)))
    wide = {'labels': [
        {'label': 'focus:Component_Actor', 'count': 0, 'inImage': 0, 'outside': 0,
         'samples': []},
        {'label': 'OEI::Game_Component::NY_Component_Actor', 'count': 80, 'inImage': 0,
         'outside': 80, 'samples': ['0x%x' % (0x5000 + 0x10 * i) for i in range(80)]}]}
    wide_chosen, _ = _choose_candidates(
        wide, wanted=['Component_Actor'],
        families={'Component_Actor': ['OEI::Game_Component::NY_Component_Actor']})
    check('candidates: cap stops the family at the focus cap',
          len(wide_chosen) == CANDIDATE_CAPS['Component_Actor'], str(len(wide_chosen)))

    # `watch --class X` must scan X's derived leaf, or it would scan a vtable nothing uses.
    watch_focus, watch_everything = select_targets(['Component_Player'])
    watch_labels = [label for label, _ in watch_everything]
    check('watch: the derived vtable is in the target list',
          'OEI::Game_Component::DS_Component_Player' in watch_labels, str(watch_labels))
    check('watch: the base vtable is still counted separately',
          [label for label, _ in watch_focus] == ['focus:Component_Player'],
          str(watch_focus))
    scan_focus, scan_everything = select_targets()
    check('scan: the whole index is a target list', len(scan_everything) == 929,
          str(len(scan_everything)))
    check('scan: an unknown class name is refused',
          _refuses(lambda: select_targets(['Component_Playr'])), '')

    failed = [item for item in checks if not item[1]]
    for name, passed, detail in checks:
        print('%s %s %s' % ('PASS' if passed else 'FAIL', name, detail))
    print('')
    print('%d/%d selftest checks passed' % (len(checks) - len(failed), len(checks)))
    return 0 if not failed else 1


def selftest_live():
    """Load the real payload into *this* Python process and call every RPC once.

    No game, no lock, no launch: the point is to prove that the payload parses in this
    Frida build, that the RPC names map as expected, and that the memory scan/window/dword
    paths run end to end on a real address space before a launch series is spent on them.
    Nothing is written anywhere; the scan runs against this interpreter's own memory.
    """
    checks = []

    def check(name, condition, detail=''):
        checks.append((name, bool(condition), detail))

    device = frida.get_local_device()
    session = device.attach(os.getpid())
    try:
        script = session.create_script(
            SESSION_JS.replace('__CONFIG__', json.dumps(
                _configure_payload(module_name='python', image_size=0, machine=0))),
            name='object-model-selftest')
        script.load()
        api = script.exports_sync
        info = api.info()
        check('live: module identified', not info.get('error'), str(info.get('name')))
        check('live: PE verified', info.get('verifiedPe') is True, str(info.get('machine')))
        scan = api.scan([['smoke', 0x1234], ['smoke2', 0x1238]],
                        {'perClassFor': 2, 'maxMillis': 20000})
        check('live: scan returned labels', len(scan.get('labels') or []) == 2,
              str(len(scan.get('labels') or [])))
        check('live: scan read memory', (scan.get('scannedBytes') or 0) > 0,
              str(scan.get('scannedBytes')))
        check('live: scan has no read errors', scan.get('readErrors') == 0,
              str(scan.get('readErrors')))
        windows = api.windows(['0x1000'], 16)
        check('live: windows shape', windows.get('radius') == 16
              and len((windows.get('windows') or {}).get('0x1000', {}).get('f') or []) == 8,
              str(len((windows.get('windows') or {}).get('0x1000', {}).get('f') or [])))
        fields = api.dwords(0x1000, 4).get('fields') or []
        check('live: dwords shape', len(fields) == 4, str(len(fields)))
        chased = api.pointers(['0x1000'], 1)
        check('live: pointer chase returns handles',
              'targets' in chased and 'handles' in chased, str(chased.get('handles')))
        script.unload()
    finally:
        session.detach()
    for name, passed, detail in checks:
        print('%s %s %s' % ('PASS' if passed else 'FAIL', name, detail))
    failed = [item for item in checks if not item[1]]
    print('')
    print('%d/%d live checks passed' % (len(checks) - len(failed), len(checks)))
    return 0 if not failed else 1


# ---------------------------------------------------------------------------
def main(argv=None):
    try:
        sys.stdout.reconfigure(encoding='ascii', errors='replace')
        sys.stderr.reconfigure(encoding='ascii', errors='replace')
    except Exception:                               # noqa: BLE001
        pass
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--tools-dir', default=None,
                        help='folder holding in_game.py/console_exec.py (step 2/3 worktrees)')
    sub = parser.add_subparsers(dest='command', required=True)

    scan = sub.add_parser('scan', help='one guarded launch: menu scan, level scan, walk test')
    scan.add_argument('--hold', type=float, default=1.2, help='seconds each direction is held')
    scan.add_argument('--warmup', type=float, default=32.0, help='seconds before the menu scan')
    scan.add_argument('--radius', type=int, default=RADIUS)
    scan.add_argument('--skip-walk', action='store_true', help='counts only, no input')
    scan.add_argument('--skip-level', action='store_true', help='menu scan only')
    scan.add_argument('--load-save', choices=['fort', 'none'], default='none',
                      help='enter a level by loading a save from the game list (fort: '
                           'a Clyde\'s Fort save, real gameplay) instead of clicking Continue')
    scan.add_argument('--owner', default='step04-objects')
    scan.add_argument('--run-root', default=None)

    watch = sub.add_parser('watch', help='validate given classes by longer holds, or replay')
    watch.add_argument('--class', dest='classes', action='append',
                       help='focus class name, repeatable (default: all focus classes)')
    watch.add_argument('--hold', type=float, default=2.5)
    watch.add_argument('--radius', type=int, default=RADIUS)
    watch.add_argument('--run', default=None, help='re-analyse a stored run folder, no launch')
    watch.add_argument('--owner', default='step04-objects')
    watch.add_argument('--run-root', default=None)

    selftest_parser = sub.add_parser('selftest',
                                     help='no game: index, formatting and movement analysis')
    selftest_parser.add_argument('--live', action='store_true',
                                 help='also load the payload into this Python process')

    args = parser.parse_args(argv)
    if args.command == 'selftest':
        offline = selftest()
        if args.live:
            return selftest_live() or offline
        return offline
    if args.command == 'watch' and args.run:
        return replay(args.run)
    if args.command == 'scan':
        record = run_series(mode='scan', skip_walk=args.skip_walk, skip_level=args.skip_level,
                            warmup=args.warmup, hold=args.hold, radius=args.radius,
                            owner=args.owner, run_root=args.run_root,
                            load_save=LOAD_PRESETS.get(args.load_save))
    else:
        record = run_series(mode='watch', wanted=args.classes, warmup=32.0, hold=args.hold,
                            radius=args.radius, owner=args.owner, run_root=args.run_root)
    print_scan(record)
    print('')
    print('top outside-image hits (%s):' % ('level_scan' if record.get('level_scan')
                                            else 'menu_scan'))
    for label, rva, outside, image in top_hits(
            record, scan='level_scan' if record.get('level_scan') else 'menu_scan'):
        print('  %-6d %-10s %s' % (outside, rva, label))
    print('')
    print('run folder: %s' % record.get('run'))
    return 0 if (record.get('validity') or {}).get('run_valid') else 2


if __name__ == '__main__':
    raise SystemExit(main())
