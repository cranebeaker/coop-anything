"""Execute console commands inside South Park: The Stick of Truth and read the result.

Owner: step 3 of the co-op project (see ``games/south-park-the-stick-of-truth/PARALLEL.md``).
This module is the Gate 1 deliverable: one call that executes a console command in the
guarded, isolated test copy and returns its output plus machine-checkable evidence.

Mechanism (found 2026-11-15, evidence in ``notes/02-console-and-devtools.md``)
------------------------------------------------------------------------------
The build ships the whole Onyx console, but every external trigger is dead: ``-exec``
and ``data/global/console_scripts/*.txt`` are inert at startup, ``-connect`` is a dead
command-line switch, and the on-screen console needs a keypress into a D3D window.
What *is* alive is the console inside the process. ``.text`` is Steam-CEG encrypted at
rest, so the static image shows no code references; the live process is decrypted, and
a scan of the live ``.text`` for the address of a known string finds the code that uses
it. That gave three addresses (RVAs, base ``0x400000``):

``0x107c60``  console print, ``void print(const char* fmt, ...)`` (cdecl, varargs).
              Hooking it turns console output into data.
``0x107f50``  the console line executor: the engine's own "here is a line the user
              typed" entry point. It splits on newline/``;`` (quote-aware), echoes the
              line when ``con_echo_command`` is on, and dispatches name + arguments to
              the registered command. It takes the string in **eax**, so it is called
              through a 12-byte stub this module builds in the target process.
``0x51b6a0``  ``Console::add_command(name, flags, help, handler)``, a thiscall on a
              statically allocated command object; every command object sits in
              ``.data`` at a 24-byte stride around ``0xf35000``, so the command table
              (name, help, flags, handler) can be read straight out of the process.

So: launch the isolated copy under the shared save guard, hook ``0x107c60``, call
``0x107f50`` with the command text, and return what the engine printed.

Usage
-----
    py -3.14 -B tools/console_exec.py run "find multiplayer" "status"
    py -3.14 -B tools/console_exec.py probe          # write a structural report

    from console_exec import run_commands
    result = run_commands(['status'])   # -> {ok, output, evidence}

stdout is forced to ASCII, because the owner's console code page is cp1251.
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
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT.parents[2] / 'local' / 'runtime-libs'))
try:
    import frida
except ModuleNotFoundError as error:  # pragma: no cover - environment dependent
    raise SystemExit('frida is required (pinned wheel in <repo>/local/runtime-libs): ' + repr(error))

GAME = Path(os.environ.get('SP_GAME_DIR', r'D:\coop-work\game-test'))
EXE = GAME / 'South Park - The Stick of Truth.exe'
EXPECTED_EXE_SHA256 = '54a83d055f97cad8beeb5aa027c96c0c2aa4a6c36394634270297dd47856a0d6'
ORIGINAL_PROFILE = Path.home() / 'Documents' / 'My Games' / 'South Park - The Stick of Truth'
WINDOW_TITLE = 'South Park\u2122: The Stick of Truth\u2122'
RUNS = Path(os.environ.get('SP_RUN_ROOT', ROOT.parents[2] / 'local' / 'game-runs'))

# RVAs of image strings, from the retail image (file offset - 0x400). All plain ASCII
# in .rdata; only the code that references them is encrypted at rest.
STRINGS = {
    'exec_name': 0x7524bc,            # "exec"
    'exec_usage': 0x752318,           # "usage: exec filename"
    'find_name': 0x7523dc,            # "find"
    'find_no_match': 0x7522fc,        # "find: no matches for '%s'"
    'write_file_name': 0x7524e8,      # "write_file"
    'output_prefix': 0x752394,        # "] %s"  (console echo prefix)
    'unknown_var': 0x752380,          # 'unknown var "%s"'
    'script_dir': 0x753438,           # "global/console_scripts"
    'con_echo_on': 0x7523b0,          # "con_echo_command 1"
    'gui_console_exec': 0x745840,     # "CONSOLE_EXEC"
    'gui_console_open': 0x7d76b8,     # "console_open"
    'cmd_console_toggle': 0x7d76dc,   # "console_toggle"
    'cmd_quit': 0x7c9924,             # "quit"
    'cmd_connect': 0x7f5b0c,          # "connect"
    'cmd_status': 0x7f5ba0,           # "status"
}

# Addresses found in the live process by probe() on 2026-11-15 (run b294e922...).
# NOTE: the registration thunks address their command objects with absolute VAs
# (`mov ecx, 0xf35a74`), so the constants below are VAs; everything passed to va() is
# an RVA. .data holds 1.3 MB of initialised data followed by ~19 MB of BSS, and the
# command objects are spread through it (exec at 0xf35a74 = rva 0xb35a74, console_open
# at 0x20a5c54 = rva 0x1ca5c54), so the scan covers the whole section.
CONSOLE_PRINT_RVA = 0x107c60      # void print(const char* fmt, ...)
CONSOLE_LINE_RVA = 0x107f50       # line executor, string arrives in eax
ADD_COMMAND_RVA = 0x51b6a0        # Console::add_command(name, flags, help, handler)
EXEC_HANDLER_RVA = 0x107e50       # handler for "exec": void f(int argc, char* argv)
DATA_SCAN_START = 0x8a5000        # rva of the .data section
DATA_SCAN_END = 0x1d4ac1c

TEXT_RVA = 0x1000
TEXT_SIZE = 0x7368d1

# ---------------------------------------------------------------------------
# Frida payload. Loaded before resume; arms hooks on request, never at load time,
# because .text is still encrypted while the process is suspended.
# ---------------------------------------------------------------------------
SESSION_JS = r"""
const config = __CONFIG__;
const main = Process.mainModule;
function va(rva) { return main.base.add(rva); }
function rvaOf(address) { return address.sub(main.base).toInt32() >>> 0; }
function valueHex(value) {
    const bytes = [];
    let v = value >>> 0;
    for (let i = 0; i < 4; i++) { bytes.push((v & 0xff).toString(16).padStart(2, '0')); v >>>= 8; }
    return bytes.join('');
}
function listInstructions(rva, count) {
    const out = [];
    let address = va(rva);
    for (let i = 0; i < (count || 40); i++) {
        let instruction;
        try { instruction = Instruction.parse(address); }
        catch (e) { out.push('<stop: ' + String(e) + '>'); break; }
        let text = instruction.toString();
        const immediates = text.match(/0x[0-9a-f]{5,8}/g) || [];
        for (const immediate of immediates) {
            try {
                const offset = (parseInt(immediate, 16) - main.base.toInt32()) >>> 0;
                if (offset >= main.size) continue;
                const string = va(offset).readCString(48);
                if (string !== null && string.length > 1) text += '   ; ' + JSON.stringify(string);
            } catch (ignored) {}
        }
        out.push(rvaOf(address).toString(16) + '  ' + text);
        address = instruction.next;
    }
    return out;
}

const state = {armed: false, lines: [], printHits: 0, notes: []};
const u32 = (address) => { try { return address.readU32(); } catch (e) { return null; } };

function note(text) { if (state.notes.length < 200) state.notes.push(String(text)); }

// --- printf reconstruction -------------------------------------------------
function renderFormat(fmt, values) {
    let out = '';
    let i = 0;
    let arg = 0;
    while (i < fmt.length) {
        const ch = fmt[i];
        if (ch !== '%') { out += ch; i++; continue; }
        let spec = '%';
        let j = i + 1;
        while (j < fmt.length && !/[diouxXfFeEgGscp%]/.test(fmt[j])) { spec += fmt[j]; j++; }
        if (j >= fmt.length) { out += fmt.slice(i); break; }
        const kind = fmt[j];
        spec += kind;
        i = j + 1;
        if (kind === '%') { out += '%'; continue; }
        const value = values[arg++];
        if (value === undefined || value === null) { out += '<missing>'; continue; }
        if (kind === 's') {
            try { out += value.isNull() ? '(null)' : value.readCString(1024); }
            catch (e) { out += '<unreadable:' + value + '>'; }
        } else if ('diouxX'.indexOf(kind) >= 0) {
            const n = value.toInt32();
            out += (kind === 'x' || kind === 'X') ? (n >>> 0).toString(16) : String(n);
        } else if ('fFeEgG'.indexOf(kind) >= 0) {
            out += '<' + spec + '>';
        } else if (kind === 'c') {
            out += String.fromCharCode(value.toInt32() & 0xff);
        } else {
            out += spec;
        }
    }
    return out;
}

function installPrintHook() {
    // 0x107c60 may be an incremental-link thunk (jmp <outside the image>) that the
    // engine installs at runtime, so hook the thunk and its immediate jmp target.
    const targets = [va(config.consolePrintRva)];
    try {
        const first = targets[0].readByteArray(5);
        const bytes = new Uint8Array(first);
        if (bytes[0] === 0xe9) {
            const relative = targets[0].add(1).readS32();
            targets.push(targets[0].add(5).add(relative));
        }
    } catch (e) {}
    state.printTargets = [];
    for (const target of targets) {
        try {
            Interceptor.attach(target, {
                onEnter(args) {
                    state.printHits++;
                    let fmt;
                    try { fmt = args[0].readCString(2048); } catch (e) { return; }
                    if (fmt === null) return;
                    const values = [args[1], args[2], args[3], args[4], args[5], args[6]];
                    state.lines.push(renderFormat(fmt, values));
                }
            });
            state.printTargets.push(target.toString());
        } catch (error) { note('print hook failed at ' + target + ': ' + error); }
    }
}

// --- calling the engine's line executor ------------------------------------
// 0x107f50 takes the line in eax. The stub also passes it on the stack, so it works
// whether the function is __usercall(eax) as observed or a plain stack-argument call.
function buildLineStub() {
    const mem = Memory.alloc(32);
    const target = va(config.consoleLineRva);
    const code = [
        0x8b, 0x44, 0x24, 0x04,             // mov eax, [esp + 4]
        0xff, 0x74, 0x24, 0x04,             // push dword [esp + 4]
        0xe8, 0x00, 0x00, 0x00, 0x00,       // call target
        0x83, 0xc4, 0x04,                   // add esp, 4
        0xc2, 0x04, 0x00                    // ret 4
    ];
    mem.writeByteArray(code);
    const nextInstruction = mem.add(13);            // end of the call instruction
    const relative = target.sub(nextInstruction).toInt32();
    mem.add(9).writeS32(relative);
    Memory.protect(mem, 32, 'rwx');
    return new NativeFunction(mem, 'void', ['pointer'], 'stdcall');
}

// --- running a command on the game's own thread ---------------------------
// Executing straight from the debugger thread works, but the console is normally driven
// from the frame loop, so execLineMain() queues the text and lets the message pump run
// it on an engine thread. progress() reports when the queue was drained.
function installPumpHook() {
    state.pending = [];
    state.processed = 0;
    state.busy = false;
    state.pumpHooks = [];
    for (const name of ['PeekMessageW', 'PeekMessageA', 'DispatchMessageW']) {
        let address;
        try { address = Process.getModuleByName('user32.dll').getExportByName(name); }
        catch (e) { note('pump export missing: ' + name); continue; }
        try {
            Interceptor.attach(address, {
                onEnter() {
                    if (state.busy || !state.lineStub || state.pending.length === 0) return;
                    state.busy = true;
                    try {
                        const text = state.pending.shift();
                        state.lineStub(Memory.allocUtf8String(text));
                        state.processed++;
                    } catch (error) { note('pump exec failed: ' + error); }
                    state.busy = false;
                }
            });
            state.pumpHooks.push(name);
        } catch (error) { note('pump hook failed on ' + name + ': ' + error); }
    }
}

// --- crash-tolerant network evidence --------------------------------------
// Net activity is reported with send() the moment it happens, so a call that kills the
// process still leaves the record on the Python side. Counters are also read from
// tools/net_probe.js on demand; these events are the fallback when it cannot be read.
function installNetHooks() {
    state.netHooked = [];
    const winsock = [['bind', 1], ['listen', 0], ['connect', 1], ['WSAConnect', 1]];
    let ws2 = null;
    try { ws2 = Process.getModuleByName('ws2_32.dll'); } catch (e) { note('ws2_32 not loaded'); }
    if (ws2 !== null) {
        for (const [name, addressIndex] of winsock) {
            let address = null;
            try { address = ws2.getExportByName(name); } catch (e) { continue; }
            try {
                Interceptor.attach(address, {
                    onEnter(args) {
                        const entry = {api: name};
                        if (addressIndex && args[addressIndex] && !args[addressIndex].isNull()) {
                            try {
                                const sockaddr = args[addressIndex];
                                const family = sockaddr.readU16();
                                entry.family = family;
                                if (family === 2) {
                                    entry.port = (sockaddr.add(2).readU8() << 8) | sockaddr.add(3).readU8();
                                    entry.address = [4, 5, 6, 7].map(i => sockaddr.add(i).readU8()).join('.');
                                }
                            } catch (error) { entry.parseError = String(error); }
                        }
                        send({kind: 'net', ...entry});
                    }
                });
                state.netHooked.push(name);
            } catch (error) { note('net hook failed ' + name + ': ' + error); }
        }
    }
    for (const name of ['SteamGameServer_Init', 'SteamMatchmaking', 'SteamGameServerNetworking']) {
        let address = null;
        try { address = Process.getModuleByName('steam_api.dll').getExportByName(name); } catch (e) { continue; }
        try {
            Interceptor.attach(address, { onEnter() { send({kind: 'net', api: name}); } });
            state.netHooked.push(name);
        } catch (error) { note('steam hook failed ' + name + ': ' + error); }
    }
}

rpc.exports = {
    arm() {
        if (state.armed) return {armed: true, printHits: state.printHits};
        installPrintHook();
        state.lineStub = buildLineStub();
        state.execHandler = new NativeFunction(va(config.execHandlerRva), 'void', ['int', 'pointer']);
        installNetHooks();
        installPumpHook();
        state.armed = true;
        note('armed print hook at ' + config.consolePrintRva.toString(16));
        return {armed: true, printHits: state.printHits, printTargets: state.printTargets,
                pumpHooks: state.pumpHooks, netHooked: state.netHooked};
    },

    // Queue the text for the game's own thread; returns immediately.
    execLineMain(text) {
        state.pending.push(text);
        return {queued: state.pending.length, processed: state.processed};
    },

    progress() {
        return {queued: state.pending.length, processed: state.processed,
                printHits: state.printHits, pending: state.pending.slice(0, 4)};
    },

    // Execute arbitrary console text through the engine's own line executor.
    // Collected output is read with output() so nothing is counted twice.
    execLine(text) {
        const buffer = Memory.allocUtf8String(text);
        const before = state.lines.length;
        state.lineStub(buffer);
        return {newLines: state.lines.length - before, printHits: state.printHits};
    },

    // Execute a script file through the "exec" command handler: (argc, argv) where
    // argv is an array of 0x80-byte NUL-terminated slots, argv[0] = command name.
    execFile(name) {
        const argv = Memory.alloc(0x80 * 2);
        argv.writeUtf8String('exec');
        argv.add(0x80).writeUtf8String(name);
        const before = state.lines.length;
        state.execHandler(2, argv);
        return {newLines: state.lines.length - before, printHits: state.printHits};
    },

    // Call a handler whose signature reads no arguments (observed: no [ebp+8]/[esp+4]).
    callVoid(rva) {
        const before = state.lines.length;
        new NativeFunction(va(rva), 'void', [])();
        return {newLines: state.lines.length - before, printHits: state.printHits};
    },

    // Call any cdecl handler that takes (int argc, char** argv-as-0x80-slots).
    callHandler(rva, argvStrings) {
        const handler = new NativeFunction(va(rva), 'void', ['int', 'pointer']);
        const argv = Memory.alloc(0x80 * Math.max(1, argvStrings.length));
        argvStrings.forEach((text, index) => argv.add(0x80 * index).writeUtf8String(text));
        const before = state.lines.length;
        handler(argvStrings.length, argv);
        return {newLines: state.lines.length - before, printHits: state.printHits};
    },

    output() { return {lines: state.lines.splice(0, state.lines.length), printHits: state.printHits}; },
    stats() { return {armed: state.armed, printHits: state.printHits, notes: state.notes}; },

    // --- read-only inspection -------------------------------------------------
    // Locate the code that references an image string (the strings are plain in
    // .rdata, the referencing code is CEG-encrypted at rest, so this only works in
    // the live process). Returns every hit with the enclosing function and a listing.
    scanText(value) {
        // ``value`` is an RVA of a string inside the image.
        const startAddress = va(config.textRva);
        const endAddress = va(config.textRva + config.textSize);
        const found = Process.findRangeByAddress(startAddress);
        if (found === null) return {error: 'text range unreadable'};
        const limit = found.base.add(found.size);
        const stop = endAddress.compare(limit) < 0 ? endAddress : limit;
        const size = stop.sub(startAddress).toInt32();
        if (size <= 0) return {error: 'text range empty'};
        let matches;
        try { matches = Memory.scanSync(startAddress, size, valueHex(va(value).toInt32() >>> 0)); }
        catch (error) { return {error: String(error)}; }
        return {hits: matches.slice(0, 8).map(match => {
            let start = null;
            try {
                const budget = 0x500;
                const bytes = new Uint8Array(match.address.sub(budget).readByteArray(budget));
                for (let i = 0; i + 1 < bytes.length; i++) {
                    if (bytes[i] === 0xcc && bytes[i + 1] === 0xcc) {
                        let j = i;
                        while (j < bytes.length && bytes[j] === 0xcc) j++;
                        start = match.address.sub(budget).add(j);
                        i = j;
                    }
                }
            } catch (error) {}
            return {rva: rvaOf(match.address).toString(16),
                    functionStart: start === null ? null : rvaOf(start).toString(16),
                    listing: start === null ? [] : listInstructions(rvaOf(start), 45)};
        })};
    },

    disasm(rva, count) { return listInstructions(rva, count); },

    dwords(rva, count) {
        const out = [];
        for (let i = 0; i < (count || 16); i++) {
            const address = va(rva + i * 4);
            const value = u32(address);
            if (value === null) { out.push({rva: rva + i * 4, value: null}); continue; }
            let note = '';
            const offset = (value - main.base.toInt32()) >>> 0;
            if (offset < main.size) {
                try { const text = va(offset).readCString(40); if (text) note = JSON.stringify(text); } catch (e) {}
            }
            out.push({rva: rva + i * 4, value: '0x' + value.toString(16), note});
        }
        return out;
    },

    // Walk the statically allocated command objects. Registration thunks in .text look
    // like "push handler; push help; push flags; push name; mov ecx, <object VA>; call",
    // and the objects are 24-byte records scattered through .data, so the whole section
    // is scanned in chunks and every 4-byte offset whose first slot is an image pointer
    // is scored. Chunked reads keep this fast (one Frida call per megabyte).
    commandTable() {
        const stringAt = (value) => {
            if (value === null || value === undefined || value === 0) return null;
            const rva = (value - main.base.toInt32()) >>> 0;
            if (rva >= main.size) return null;
            try {
                const text = va(rva).readCString(64);
                if (text === null || text.length < 2) return null;
                if (!/^[\x20-\x7e]+$/.test(text)) return null;
                return text;
            } catch (e) { return null; }
        };
        const textAt = (value) => {
            if (value === null || value === undefined) return false;
            const rva = (value - main.base.toInt32()) >>> 0;
            return rva >= 0x1000 && rva < config.textSize;
        };
        const base = main.base.toInt32() >>> 0;
        const found = [];
        const chunk = 0x100000;
        for (let start = config.dataScanStart; start < config.dataScanEnd; start += chunk) {
            const size = Math.min(chunk, config.dataScanEnd - start);
            let bytes;
            try { bytes = new Uint8Array(va(start).readByteArray(size)); } catch (e) { continue; }
            const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
            for (let i = 0; i + 24 <= size; i += 4) {
                const first = view.getUint32(i, true);
                if (first < base || first > base + main.size) continue;
                const slots = [];
                for (let index = 0; index < 6; index++) slots.push(view.getUint32(i + index * 4, true));
                const strings = slots.map(stringAt);
                if (!strings.some(s => s !== null)) continue;
                const handlers = slots.map(value => textAt(value) ? value : null);
                if (!handlers.some(h => h !== null)) continue;
                found.push({
                    rva: (start + i).toString(16),
                    slots: slots.map((value, index) => ({
                        value: '0x' + value.toString(16),
                        string: strings[index],
                        handler: handlers[index] === null ? null : (handlers[index] - base).toString(16)
                    }))
                });
                if (found.length >= 400) return found;
            }
        }
        return found;
    }
};
"""

# Read-only reconnaissance payload: locates the code that references the console
# strings. Kept because it is how the three addresses above were found, and the next
# agent will need the same trick for any other string in this build.
RECON_JS = r"""
const config = __CONFIG__;

function rvaOf(address) { return address.sub(Process.mainModule.base).toInt32() >>> 0; }
function va(rva) { return Process.mainModule.base.add(rva); }
function valueHex(value) {
    const bytes = [];
    let v = value >>> 0;
    for (let i = 0; i < 4; i++) { bytes.push((v & 0xff).toString(16).padStart(2, '0')); v >>>= 8; }
    return bytes.join('');
}
function textRange() {
    const start = va(config.textRva);
    const end = va(config.textRva + config.textSize);
    const range = Process.findRangeByAddress(start);
    if (range === null) return {start, size: 0};
    const stop = end.compare(range.base.add(range.size)) < 0 ? end : range.base.add(range.size);
    return {start, size: stop.sub(start).toInt32()};
}
function scanText(value) {
    const range = textRange();
    if (range.size <= 0) return {error: 'text range unreadable'};
    try { return {matches: Memory.scanSync(range.start, range.size, valueHex(value)).map(m => m.address)}; }
    catch (error) { return {error: String(error)}; }
}
function contextWindow(address, before, after) {
    try {
        const bytes = new Uint8Array(address.sub(before).readByteArray(before + after));
        const marked = [];
        for (let i = 0; i < bytes.length; i++) {
            const hex = bytes[i].toString(16).padStart(2, '0');
            marked.push(i === before ? '[' + hex + ']' : hex);
        }
        return marked.join('');
    } catch (error) { return '<' + String(error) + '>'; }
}
function findFunctionStart(address, budget) {
    let bytes;
    try { bytes = new Uint8Array(address.sub(budget).readByteArray(budget)); } catch (error) { return null; }
    let best = null;
    for (let i = 0; i + 1 < bytes.length; i++) {
        if (bytes[i] === 0xcc && bytes[i + 1] === 0xcc) {
            let j = i;
            while (j < bytes.length && bytes[j] === 0xcc) j++;
            best = address.sub(budget).add(j);
            i = j;
        }
    }
    return best;
}
function disasm(start, maxInstructions) {
    const out = [];
    let address = start;
    for (let i = 0; i < maxInstructions; i++) {
        let instruction;
        try { instruction = Instruction.parse(address); }
        catch (error) { out.push('<' + String(error) + '>'); break; }
        out.push(rvaOf(address).toString(16) + '  ' + instruction.toString());
        address = instruction.next;
    }
    return out;
}

rpc.exports = {
    telemetry() {
        const report = {moduleBase: Process.mainModule.base.toString(), xrefs: {}, errors: []};
        for (const target of config.strings) {
            const entry = {rva: target.rva};
            const hits = scanText(va(target.rva).toInt32() >>> 0);
            if (hits.error) { entry.error = hits.error; }
            else {
                entry.hitCount = hits.matches.length;
                entry.hits = hits.matches.slice(0, 6).map(match => {
                    const start = findFunctionStart(match, 0x500);
                    return {rva: rvaOf(match).toString(16),
                            context: contextWindow(match, 12, 12),
                            functionStart: start === null ? null : rvaOf(start).toString(16),
                            listing: start === null ? disasm(match.sub(32), 20) : disasm(start, 70)};
                });
            }
            report.xrefs[target.label] = entry;
        }
        return report;
    }
};
"""


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def snapshot(root):
    return {path.relative_to(root).as_posix(): sha256(path)
            for path in sorted(Path(root).rglob('*')) if path.is_file()}


def steam_userdata_roots():
    """Steam's per-account save tree for this app, one folder per account id.

    The playbook (methods.md) is explicit: steam.exe writes FileWrite calls into
    `Steam\\userdata\\<id>\\<appid>\\remote` even when Cloud is switched off for the game, so a
    guard that only hashes the Documents profile is not enough.
    """
    candidates = [Path(r'C:\Program Files (x86)\Steam'), Path(r'D:\Steam'),
                  Path(r'D:\SteamLibrary')]
    roots = []
    for steam in candidates:
        userdata = steam / 'userdata'
        if not userdata.is_dir():
            continue
        for account in userdata.iterdir():
            app = account / '213670'
            if app.is_dir():
                roots.append(app)
    return roots


def steam_userdata_snapshot():
    trees = {}
    for root in steam_userdata_roots():
        trees[str(root)] = snapshot(root)
    return trees


def inventory(root):
    return {path.relative_to(root).as_posix(): path.stat().st_size
            for path in sorted(Path(root).rglob('*')) if path.is_file()}


def preflight():
    if not EXE.is_file():
        raise SystemExit('game copy not found: ' + str(EXE))
    if sha256(EXE) != EXPECTED_EXE_SHA256:
        raise SystemExit('unexpected game executable; refusing launch')
    if not ORIGINAL_PROFILE.is_dir():
        raise SystemExit('expected original profile not found; refusing launch')
    if ctypes.windll.user32.FindWindowW(WINDOW_TITLE, None):
        raise SystemExit('a South Park window already exists; refusing to interfere')


class Game:
    """The isolated test copy, spawned under the shared save guard, with a live session.

    Mirrors tools/guard_session.py, which cannot load a script from memory; the guard
    itself is still the shared tools/save_guard.js, unmodified.
    """

    def __init__(self, duration=60, extra_args=(), payload=SESSION_JS, config_extra=None,
                 extra_scripts=None):
        self.duration = duration
        self.extra_args = list(extra_args)
        self.payload = payload
        self.config_extra = dict(config_extra or {})
        self.extra_script_names = list(extra_scripts or [])
        self.extra = {}
        self.run_dir = RUNS / uuid.uuid4().hex
        self.result = {}
        self.script = None
        self.pid = None
        self.armed = False

    def __enter__(self):
        preflight()
        self.run_dir.mkdir(parents=True)
        # Remember the owner's window now, so the run gives focus back when it ends: a test
        # must not leave the game in front of whatever the owner was doing.
        try:
            self.owner_window = int(ctypes.windll.user32.GetForegroundWindow() or 0)
        except Exception:                            # noqa: BLE001
            self.owner_window = 0
        self.profile_before = snapshot(ORIGINAL_PROFILE)
        self.userdata_before = steam_userdata_snapshot()
        backup = self.run_dir / 'original-profile-backup'
        shutil.copytree(ORIGINAL_PROFILE, backup)
        self.profile_dir = self.run_dir / 'test-profile'
        shutil.copytree(backup, self.profile_dir)
        if snapshot(backup) != self.profile_before or snapshot(ORIGINAL_PROFILE) != self.profile_before:
            raise SystemExit('profile changed during backup; refusing launch')
        temporary = self.run_dir / 'temp'
        temporary.mkdir(parents=True)
        redirections = []
        for source, label in [
            (Path(r'C:\Program Files (x86)\Steam\logs'), 'steam-logs'),
            (Path.home() / 'AppData/Local/Microsoft/Windows/Caches', 'windows-cache'),
            (Path(r'C:\ProgramData\NVIDIA Corporation\Drs'), 'nvidia-drs'),
        ]:
            destination = self.run_dir / 'runtime-files' / label
            destination.mkdir(parents=True)
            if label == 'nvidia-drs' and source.is_dir():
                for item in source.iterdir():
                    if item.is_file() and item.stat().st_size < 20 * 1024 * 1024:
                        shutil.copy2(item, destination / item.name)
            redirections.append({'from': str(source), 'to': str(destination)})
        self.config = {'originalSaves': str(ORIGINAL_PROFILE), 'testSaves': str(self.profile_dir),
                       'writeRoots': [str(GAME), str(self.run_dir)], 'redirections': redirections,
                       'nativeCandidates': [], 'traceNatives': [], 'scriptSignatures': [],
                       'blockRemoteStorage': False,
                       'logPaths': ['console_scripts', 'console.gfx', 'console.scn']}
        self.config.update(self.config_extra)
        self.game_before = inventory(GAME)
        self.events = []
        self.errors = []
        ready = []
        self.messages = []

        def on_message(message, data):
            if message['type'] == 'send':
                payload = message['payload']
                self.events.append(payload)
                self.messages.append(payload)
                if payload.get('kind') == 'guard-ready':
                    ready.append(True)
            else:
                self.errors.append(message.get('stack', message.get('description')))

        argv = [str(EXE)] + (self.extra_args or ['-window'])
        environment = {'SteamAppId': '213670', 'SteamGameId': '213670',
                       'TEMP': str(temporary), 'TMP': str(temporary)}
        # Test-only switch: which inline hooks the mod should enable (see mod/src/mod.cpp).
        if os.environ.get('SP_COOP_INLINE'):
            environment['SP_COOP_INLINE'] = os.environ['SP_COOP_INLINE']
        self.device = frida.get_local_device()
        self.pid = self.device.spawn(argv, cwd=str(GAME), stdio='pipe',
                                     env=environment)
        try:
            self.session = self.device.attach(self.pid)
        except Exception as error:                   # noqa: BLE001 - a dead spawn is data
            # "connection closed" / "timeout" hides the reason: report the process exit code,
            # which names a loader failure (0xC0000142) or a normal early exit (0).
            code = ctypes.c_ulong(0)
            kernel32 = ctypes.windll.kernel32
            handle = kernel32.OpenProcess(0x1000, False, self.pid)   # QUERY_LIMITED_INFORMATION
            if handle:
                kernel32.GetExitCodeProcess(handle, ctypes.byref(code))
                kernel32.CloseHandle(handle)
            raise RuntimeError('attach failed (%r); exit code 0x%08x' % (error, code.value))
        guard_source = (ROOT / 'save_guard.js').read_text(encoding='utf-8').replace(
            '__CONFIG__', json.dumps(self.config))
        guard = self.session.create_script(guard_source, name='south-park-save-guard')
        guard.on('message', on_message)
        guard.load()
        deadline = time.time() + 8
        while not ready and time.time() < deadline:
            time.sleep(0.05)
        if not ready or self.errors:
            self._shutdown()
            raise RuntimeError('guard failed before resume: ' + repr(self.errors))
        payload_config = {'textRva': TEXT_RVA, 'textSize': TEXT_SIZE,
                          'consolePrintRva': CONSOLE_PRINT_RVA,
                          'consoleLineRva': CONSOLE_LINE_RVA,
                          'execHandlerRva': EXEC_HANDLER_RVA,
                          'addCommandRva': ADD_COMMAND_RVA,
                          'dataScanStart': DATA_SCAN_START,
                          'dataScanEnd': DATA_SCAN_END,
                          'strings': [{'label': label, 'rva': rva} for label, rva in STRINGS.items()]}
        self.script = self.session.create_script(
            self.payload.replace('__CONFIG__', json.dumps(payload_config)), name='console-exec')
        self.script.on('message', on_message)
        self.script.load()
        # Extra read-only probes (tools/net_probe.js) load as separate scripts so their
        # own rpc.exports namespace does not collide with this one.
        for extra_name in self.extra_script_names:
            source = (ROOT / extra_name).read_text(encoding='utf-8')
            extra = self.session.create_script(source, name=extra_name)
            extra.on('message', on_message)
            extra.load()
            self.extra[extra_name] = extra
        self.device.resume(self.pid)
        self.started = time.time()
        self._hand_focus_back()
        return self

    def _hand_focus_back(self):
        """Give the desktop back as soon as the game window appears, and keep doing it.

        A run must not leave the game in front of the owner. The window stays where it is, but
        focus returns to whatever they were using; captures that need the foreground ask for it
        explicitly and briefly.
        """
        import threading

        owner_window = getattr(self, 'owner_window', 0)
        if not owner_window:
            return

        def watch():
            user = ctypes.windll.user32
            deadline = time.time() + 30
            while time.time() < deadline:
                try:
                    game_window = user.FindWindowW(WINDOW_TITLE, None)
                    if game_window:
                        if int(user.GetForegroundWindow() or 0) == int(game_window):
                            user.ShowWindow(owner_window, 9)
                            user.SetForegroundWindow(owner_window)
                except Exception:                    # noqa: BLE001
                    pass
                time.sleep(0.7)

        threading.Thread(target=watch, daemon=True, name='coop-focus-back').start()

    # -- session helpers ---------------------------------------------------
    def wait(self, seconds):
        end = time.time() + seconds
        while time.time() < end:
            time.sleep(0.1)
            if self.events and self.events[-1].get('kind') == 'crash':
                break

    def wait_for_ready(self, minimum=25.0, quiet=3.0, timeout=45.0):
        """Wait until the game has finished starting.

        The console command registry turns out to be empty in this build, so it cannot
        be used as a readiness signal; the engine's script handler works without it.
        This waits ``minimum`` seconds, then keeps waiting while the guard is still
        reporting file activity, and optionally records whether a command table exists.
        """
        deadline = time.time() + timeout
        while time.time() - self.started < minimum and time.time() < deadline:
            time.sleep(0.5)
        last = time.time()
        seen = len(self.events)
        while time.time() < deadline:
            time.sleep(0.25)
            if len(self.events) != seen:
                seen = len(self.events)
                last = time.time()
            elif time.time() - last >= quiet:
                break
        return True

    def ready(self):
        try:
            return bool(self.command_table())
        except Exception:
            return False

    def stats(self):
        return self.script.exports_sync.stats()

    def alive(self):
        try:
            self.script.exports_sync.stats()
            return True
        except Exception:
            return False

    def arm(self):
        if not self.armed:
            self.result.setdefault('arm', self.script.exports_sync.arm())
            self.armed = True
        return self.result['arm']

    def exec_line(self, text):
        return self.script.exports_sync.exec_line(text)

    def exec_line_main(self, text, timeout=6.0):
        """Run the text on the game's own thread through the message pump.

        Returns {'ran': bool, 'waited': seconds}. If the pump never fires (the engine
        might not call PeekMessage while sitting in the menu), the caller can fall back
        to the direct call - nothing has been executed in that case.
        """
        before = self.script.exports_sync.progress()
        self.script.exports_sync.exec_line_main(text)
        deadline = time.time() + timeout
        while time.time() < deadline:
            time.sleep(0.1)
            progress = self.script.exports_sync.progress()
            if progress.get('processed', 0) > before.get('processed', 0):
                return {'ran': True, 'waited': round(timeout - (deadline - time.time()), 2)}
            if progress.get('queued', 0) == 0 and progress.get('processed', 0) > before.get('processed', 0):
                return {'ran': True, 'waited': 0.1}
        return {'ran': False, 'waited': timeout}

    def exec_file(self, name):
        return self.script.exports_sync.exec_file(name)

    def call_handler(self, rva, argv):
        return self.script.exports_sync.call_handler(rva, argv)

    def call_void(self, rva):
        return self.script.exports_sync.call_void(rva)

    def output(self):
        return self.script.exports_sync.output()

    def disasm(self, rva, count=40):
        return self.script.exports_sync.disasm(rva, count)

    def dwords(self, rva, count=16):
        return self.script.exports_sync.dwords(rva, count)

    def command_table(self):
        return self.script.exports_sync.command_table()

    def scan_text(self, rva):
        """Code that references the string at ``rva`` (live .text scan)."""
        return self.script.exports_sync.scan_text(rva)

    def extra_telemetry(self, name):
        return self.extra[name].exports_sync.telemetry()

    def net_events(self):
        """Network events reported live by the session hooks (crash-tolerant)."""
        return [message for message in self.messages if message.get('kind') == 'net']

    def _shutdown(self):
        try:
            self.device.kill(self.pid)
        except Exception:
            pass
        try:
            self.session.detach()
        except Exception:
            pass

    def __exit__(self, exc_type, exc, traceback):
        if exc is not None:
            self.result['error'] = repr(exc)
        self._shutdown()
        after = snapshot(ORIGINAL_PROFILE)
        self.result['original_profile_unchanged'] = after == self.profile_before
        userdata_after = steam_userdata_snapshot()
        self.result['steam_userdata_unchanged'] = userdata_after == self.userdata_before
        self.result['steam_userdata_changed'] = sorted(
            root for root in userdata_after
            if userdata_after.get(root) != self.userdata_before.get(root))
        self.result['original_exe_hash_still_matches'] = sha256(EXE) == EXPECTED_EXE_SHA256
        self.result['guard_integrity'] = not self.errors and not any(
            isinstance(e, dict) and e.get('kind') == 'guard-error' for e in self.events)
        self.result['guard_errors'] = self.errors
        self.result['events'] = self.events
        self.result['run_dir'] = str(self.run_dir)
        game_after = inventory(GAME)
        self.result['new_game_files'] = {path: size for path, size in game_after.items()
                                         if path not in self.game_before}
        # Hand the desktop back to the owner.
        try:
            if getattr(self, 'owner_window', 0):
                user = ctypes.windll.user32
                if int(user.GetForegroundWindow() or 0) != self.owner_window:
                    user.ShowWindow(self.owner_window, 9)
                    user.SetForegroundWindow(self.owner_window)
        except Exception:                            # noqa: BLE001
            pass
        (self.run_dir / 'result.json').write_text(json.dumps(self.result, indent=2), encoding='utf-8')
        return False


def framework_ok(result):
    return all(result.get(key) is True for key in
               ('original_profile_unchanged', 'guard_integrity', 'original_exe_hash_still_matches'))


def write_script(name, commands):
    """Write a console script into the isolated copy, where the engine reads it.

    The engine builds ``<data root>/global/console_scripts/<name>.txt`` (rva 0x114b10),
    so both candidate roots are written; leftovers are listed in the notes.
    """
    written = []
    for directory in (GAME / 'data' / 'global' / 'console_scripts',
                      GAME / 'global' / 'console_scripts'):
        directory.mkdir(parents=True, exist_ok=True)
        path = directory / (name + '.txt')
        path.write_text('\n'.join(commands) + '\n', encoding='ascii')
        written.append(str(path))
    return written


def run_commands(commands, duration=90, warmup=25, settle=2.5, script_name=None, echo=True):
    """Execute console commands in the guarded test copy and read their output.

    ``commands`` is a list of console lines (``['status', 'find multiplayer']``).
    Returns ``{'ok', 'output': [str], 'evidence': {...}}``.

    Mechanism (proven in run 235566a1, see notes/02-console-and-devtools.md): the lines
    are written to ``<game>/data/global/console_scripts/<name>.txt`` in the isolated
    copy, and the engine's own ``exec`` command handler (rva 0x107e50, called with the
    documented ``(argc, argv-in-0x80-byte-slots)`` convention) reads and executes them
    through the engine console, while the console print function (rva 0x107c60) is
    hooked so the output comes back as text. ``ok`` is true when the engine executed the
    script and the three framework checks stayed true - *not* when the commands
    succeeded: in this build every registered name answers ``unknown var "<name>"``,
    which is the engine's own answer and is returned verbatim in ``output``.
    """
    commands = [str(command) for command in commands]
    script_name = script_name or ('coop_step03_' + uuid.uuid4().hex[:8])
    script_lines = (['con_echo_command 1'] if echo else []) + commands
    script_paths = write_script(script_name, script_lines)
    output = []
    with Game(duration=duration) as game:
        game.result['commands'] = commands
        game.result['script'] = {'name': script_name, 'paths': script_paths,
                                 'lines': script_lines}
        game.result['mechanism'] = {
            'route': 'engine "exec" command handler (rva 0x%x) reading '
                     'data/global/console_scripts/%s.txt' % (EXEC_HANDLER_RVA, script_name),
            'output': 'engine console print (rva 0x%x) hooked, format strings rendered'
                      % CONSOLE_PRINT_RVA,
            'line_executor_rva': hex(CONSOLE_LINE_RVA),
            'add_command_rva': hex(ADD_COMMAND_RVA)}
        game.wait_for_ready(minimum=warmup)
        game.result['command_objects'] = len(game.command_table() or [])
        game.result['arm'] = game.arm()
        before = inventory(GAME)
        survived = True
        call = {}
        try:
            call = game.exec_file(script_name)
        except Exception as error:      # the process died under the call
            survived = False
            call = {'error': repr(error)}
        # The engine drains one queued line per frame, so keep collecting for a moment.
        deadline = time.time() + settle
        expected = len(script_lines)
        while time.time() < deadline:
            time.sleep(0.25)
            try:
                output.extend(game.output().get('lines') or [])
            except Exception:
                break
            if len(output) >= expected:
                break
        try:
            output.extend(game.output().get('lines') or [])
        except Exception:
            pass
        after = inventory(GAME)
        created = sorted(path for path in after if path not in before)
        game.result['call'] = call
        game.result['console_output'] = output
        game.result['created_files'] = created
    evidence = {
        'run_dir': game.result['run_dir'],
        'report': str(Path(game.result['run_dir']) / 'result.json'),
        'mechanism': game.result.get('mechanism'),
        'script': game.result.get('script'),
        'call': game.result.get('call'),
        'print_hits': (game.result.get('arm') or {}).get('printHits'),
        'command_objects': game.result.get('command_objects'),
        'console_lines': len(output),
        'created_files': game.result.get('created_files'),
        'engine_answered_unknown_var': sum(1 for line in output if line.startswith('unknown var')),
        'engine_echoed': sum(1 for line in output if line.startswith('] ')),
        'original_profile_unchanged': game.result.get('original_profile_unchanged'),
        'guard_integrity': game.result.get('guard_integrity'),
        'original_exe_hash_still_matches': game.result.get('original_exe_hash_still_matches'),
    }
    # "ok" describes the harness, not the game: the engine ran the script and answered.
    ok = bool(survived and output and framework_ok(game.result))
    return {'ok': ok, 'output': output, 'evidence': evidence}


def probe(duration=45):
    """Locate the console code in the live process; write a structural report."""
    with Game(duration=duration, payload=RECON_JS) as game:
        game.wait(duration)
        report = game.script.exports_sync.telemetry()
        game.result['probe'] = report
    summary = {
        'run_dir': game.result['run_dir'],
        'xref_hits': {label: entry.get('hitCount', entry.get('error'))
                      for label, entry in report.get('xrefs', {}).items()},
        'original_profile_unchanged': game.result.get('original_profile_unchanged'),
        'guard_integrity': game.result.get('guard_integrity'),
        'original_exe_hash_still_matches': game.result.get('original_exe_hash_still_matches'),
        'guard_errors': game.result.get('guard_errors'),
    }
    print(json.dumps(summary, indent=2))
    return report, game.result


if __name__ == '__main__':
    try:
        sys.stdout.reconfigure(encoding='utf-8')
    except Exception:
        pass
    parser = argparse.ArgumentParser(description='South Park console command runner')
    parser.add_argument('mode', choices=['run', 'probe'])
    parser.add_argument('commands', nargs='*')
    parser.add_argument('--duration', type=float, default=60)
    parser.add_argument('--warmup', type=float, default=25)
    parser.add_argument('--no-lock', action='store_true')
    options = parser.parse_args()
    owner = os.environ.get('SP_AGENT', 'step03')
    import launch_lock
    held = False
    if not options.no_lock:
        if launch_lock.acquire(owner, minutes=45, wait=1800, pid=os.getpid()) != 0:
            raise SystemExit('launch lock unavailable')
        held = True
    try:
        if options.mode == 'probe':
            probe(duration=options.duration)
        else:
            result = run_commands(options.commands, duration=options.duration,
                                  warmup=options.warmup)
            print(json.dumps(result, indent=2, ensure_ascii=True))
    finally:
        if held:
            launch_lock.release(owner)
