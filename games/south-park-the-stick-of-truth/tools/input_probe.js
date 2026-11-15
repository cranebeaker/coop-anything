// Synthetic controller for South Park: The Stick of Truth.
//
// The game reads XINPUT1_3.dll, so a pad state can be injected inside the process
// without touching the window, the focus or the OS input queue. That makes movement
// tests deterministic: the harness sets a stick for N seconds, the engine sees a real
// XInput state, and nothing depends on which window is foreground.
//
// Read-only with respect to game data: the only writes are the 16 bytes of the caller's
// own XINPUT_STATE buffer, on the way out of XInputGetState. Nothing in the image or the
// heap is patched. Hooks are installed only when `install()` is called, never at load,
// because .text is still encrypted while the process is suspended.
//
// rpc.exports:
//   modules()            -> [{name, base, size}] for every module whose name looks like input
//   install()            -> {installed, hooks, errors}
//   push(state)          -> {active, pressed}  state: {lx, ly, rx, ry, buttons}
//   release()            -> {active:false}
//   telemetry()          -> {installed, calls, frame, lastWrite, errors}

const state = {
    installed: false,
    hooks: [],
    calls: 0,
    capabilityCalls: 0,
    frame: 0,
    errors: [],
    active: false,
    pad: {lx: 0, ly: 0, rx: 0, ry: 0, buttons: 0},
    lastWrite: null,
    dinput: null,
    keys: {},
    keyWrites: 0,
    deviceStates: 0,
    stateCalls: 0,
    dataCalls: 0,
    dataSizes: {},
    keyQueue: [],
    sequence: 1,
    samples: [],
    deviceInfo: [],
    callers: {},
    callerSamples: [],
    pump: {},
    pumpCalls: 0,
    keyCalls: 0,
    noReturn: 0,
};

function note(text) {
    if (state.errors.length < 40) state.errors.push(String(text));
}

// Who calls the key-state APIs, and for which key: the return address is inside the
// engine's own input code, which is the place to look for the "input disabled" check.
function recordPump(kind, caller) {
    state.pumpCalls += 1;
    if (caller === null || caller === undefined) { state.noReturn += 1; return; }
    try {
        const module = moduleOf(caller);
        const key = kind + ':' + (module === null ? 'unknown' : module.name) + '+' +
            (module === null ? String(caller) : caller.sub(module.base).toString(16));
        state.pump[key] = (state.pump[key] || 0) + 1;
    } catch (error) {
        state.noReturn += 1;
    }
}

function disassemble(rva, before, count) {
    const main = Process.mainModule;
    const lines = [];
    let address;
    try { address = main.base.add(rva - before); } catch (error) { return lines; }
    for (let index = 0; index < count; index++) {
        let instruction;
        try { instruction = Instruction.parse(address); } catch (error) {
            lines.push('<stop at ' + address.sub(main.base).toString(16) + '>');
            break;
        }
        const offset = address.sub(main.base).toInt32();
        lines.push((offset >= 0 ? offset.toString(16) : '-' + (-offset).toString(16)) +
                   '  ' + instruction.toString());
        address = instruction.next;
    }
    return lines;
}

function recordCall(kind, caller, argument) {
    state.keyCalls += 1;
    try {
        if (caller === null) { state.noReturn += 1; return; }
        const module = moduleOf(caller);
        const name = module === null ? 'unknown' : module.name;
        const offset = module === null ? caller : caller.sub(module.base);
        const key = kind + ':' + name + '+' + offset.toString(16) + ':' + argument;
        state.callers[key] = (state.callers[key] || 0) + 1;
        if (state.callerSamples.length < 40) state.callerSamples.push(key);
    } catch (error) {
        state.noReturn += 1;
    }
}

function inputModules() {
    return Process.enumerateModules()
        .filter((m) => /xinput|dinput|hid/i.test(m.name))
        .map((m) => ({name: m.name, base: m.base.toString(), size: m.size}));
}

function writePad(buffer, pad) {
    // XINPUT_STATE: DWORD dwPacketNumber; XINPUT_GAMEPAD (16 bytes total).
    // GAMEPAD: wButtons u16 @0, bLeftTrigger u8 @2, bRightTrigger u8 @3,
    //          sThumbLX i16 @4, sThumbLY i16 @6, sThumbRX i16 @8, sThumbRY i16 @10.
    buffer.add(0).writeU32(state.frame >>> 0);
    buffer.add(4).writeU16(pad.buttons & 0xffff);
    buffer.add(6).writeU8(0);
    buffer.add(7).writeU8(0);
    buffer.add(8).writeS16(pad.lx | 0);
    buffer.add(10).writeS16(pad.ly | 0);
    buffer.add(12).writeS16(pad.rx | 0);
    buffer.add(14).writeS16(pad.ry | 0);
}

function hookExport(moduleName, exportName) {
    let module = null;
    try {
        module = Process.findModuleByName(moduleName);
    } catch (error) {
        note('findModule ' + moduleName + ': ' + error);
        return false;
    }
    if (module === null) return false;
    let address = null;
    try {
        address = module.findExportByName(exportName);
    } catch (error) {
        note('findExport ' + exportName + ': ' + error);
    }
    if (address === null) return false;
    try {
        if (exportName === 'XInputGetCapabilities') {
            // A game decides whether a pad exists by asking for its capabilities, long
            // before it polls GetState. Fake a wired gamepad so the pad is enumerated.
            Interceptor.attach(address, {
                onEnter(args) {
                    this.target = args[1];
                },
                onLeave(retval) {
                    if (!state.active || this.target === null) return;
                    try {
                        const buffer = this.target;
                        // XINPUT_CAPABILITIES: Type u8, SubType u8, Flags u16,
                        // XINPUT_GAMEPAD (16 bytes), XINPUT_VIBRATION (4 bytes).
                        buffer.add(0).writeU8(1);      // XINPUT_DEVTYPE_GAMEPAD
                        buffer.add(1).writeU8(1);      // XINPUT_DEVSUBTYPE_GAMEPAD
                        buffer.add(2).writeU16(0);     // no flags
                        writePad(buffer.add(4), {lx: 0, ly: 0, rx: 0, ry: 0, buttons: 0});
                        buffer.add(20).writeU16(0);    // vibration left
                        buffer.add(22).writeU16(0);    // vibration right
                        retval.replace(0);
                        state.capabilityCalls += 1;
                    } catch (error) {
                        note('writeCapabilities: ' + error);
                    }
                },
            });
            state.hooks.push(moduleName + '!' + exportName);
            return true;
        }
        Interceptor.attach(address, {
            onEnter(args) {
                this.target = args[1];
            },
            onLeave(retval) {
                if (!state.active || this.target === null) return;
                try {
                    state.frame = (state.frame + 1) >>> 0;
                    writePad(this.target, state.pad);
                    state.calls += 1;
                    state.lastWrite = {
                        frame: state.frame,
                        at: Date.now(),
                        pad: JSON.parse(JSON.stringify(state.pad)),
                    };
                    retval.replace(0); // ERROR_SUCCESS
                } catch (error) {
                    note('writePad: ' + error);
                }
            },
        });
        state.hooks.push(moduleName + '!' + exportName);
        return true;
    } catch (error) {
        note('attach ' + exportName + ': ' + error);
        return false;
    }
}

// --- DirectInput8 -----------------------------------------------------------
//
// The engine loads DINPUT8.dll and creates one DirectInput object, then (probably) a
// keyboard device. Synthetic keybd_event never reached its gameplay code, so the plan is
// the in-process route the hub's playbook recommends: hook the device's own
// IDirectInputDevice8::GetDeviceState and write the key state into the buffer the engine
// reads. The buffer is 256 bytes of DIK_ codes for a keyboard device; anything else is left
// untouched.

const DIK = {
    escape: 0x01, enter: 0x1C, space: 0x39, tab: 0x0F,
    w: 0x11, a: 0x1E, s: 0x1F, d: 0x20,
    up: 0xC8, down: 0xD0, left: 0xCB, right: 0xCD,
};

function moduleOf(address) {
    try { return Process.findModuleByAddress(address); } catch (error) { return null; }
}

function validObject(candidate) {
    // A COM object's first dword points at a vtable, and every vtable lives in a module.
    try {
        if (candidate === null || candidate.isNull()) return null;
        const object = candidate.readPointer();
        if (object.isNull()) return null;
        const vtable = object.readPointer();
        if (moduleOf(vtable) === null) return null;
        return object;
    } catch (error) {
        return null;
    }
}

function hookDeviceState(device) {
    try {
        const vtable = device.readPointer();
        // IDirectInputDevice8: GetDeviceState is slot 9, GetDeviceData is slot 10.
        const method = vtable.add(9 * Process.pointerSize).readPointer();
        const dataMethod = vtable.add(10 * Process.pointerSize).readPointer();
        // A wrong pointer level (or a non-COM object) would otherwise hook arbitrary
        // hot code: only attach to addresses that belong to a loaded module.
        if (state.deviceInfo.length < 4) {
            const moduleOfState = moduleOf(method);
            const moduleOfData = moduleOf(dataMethod);
            state.deviceInfo.push({
                vtable: String(vtable),
                state: method.isNull() ? null : String(method),
                stateModule: moduleOfState === null ? null : moduleOfState.name,
                stateOffset: moduleOfState === null ? null : String(method.sub(moduleOfState.base)),
                data: dataMethod.isNull() ? null : String(dataMethod),
                dataModule: moduleOfData === null ? null : moduleOfData.name,
            });
        }
        if (!method.isNull() && Process.findModuleByAddress(method) !== null && Process.findModuleByAddress(dataMethod) !== null) {
            Interceptor.attach(method, {
                onEnter(args) {
                    this.size = args[0].toInt32();
                    this.buffer = args[1];
                },
                onLeave(retval) {
                    state.stateCalls += 1;
                    if (this.buffer === null || this.size !== 256) return;
                    state.deviceStates += 1;
                    if (retval.toInt32() !== 0) return;
                    try {
                        const pressed = Object.keys(state.keys);
                        if (pressed.length === 0) return;
                        for (const name of pressed) {
                            if (state.keys[name]) this.buffer.add(DIK[name]).writeU8(0x80);
                        }
                        state.keyWrites += 1;
                    } catch (error) {
                        note('device state: ' + error);
                    }
                },
            });
        }
        if (!dataMethod.isNull()) {
            // Buffered mode: the engine asks for the events since the last call. Feed it
            // our own DIDEVICEOBJECTDATA records (16 bytes each) instead.
            Interceptor.attach(dataMethod, {
                onEnter(args) {
                    this.stride = args[0].toInt32();
                    this.array = args[1];
                    this.count = args[2];
                    this.flags = args[3].toInt32();
                },
                onLeave(retval) {
                    state.dataCalls += 1;
                    if (this.array === null || this.count === null) return;
                    try {
                        state.dataSizes[this.stride] = (state.dataSizes[this.stride] || 0) + 1;
                        const room = this.count.readU32();
                        if (state.keyQueue.length === 0 || room === 0) return;
                        if (this.stride < 16) return;
                        const take = Math.min(room, state.keyQueue.length);
                        for (let index = 0; index < take; index++) {
                            const event = state.keyQueue.shift();
                            const slot = this.array.add(index * this.stride);
                            slot.add(0).writeU32(event.ofs >>> 0);   // dwOfs = DIK code
                            slot.add(4).writeU32(event.data >>> 0);  // dwData
                            slot.add(8).writeU32(Date.now() >>> 0);  // dwTimeStamp
                            slot.add(12).writeU32(state.sequence >>> 0);
                            state.sequence += 1;
                        }
                        this.count.writeU32(take);
                        state.keyWrites += 1;
                        retval.replace(0);
                    } catch (error) {
                        note('device data: ' + error);
                    }
                },
            });
        }
        return !method.isNull() || !dataMethod.isNull();
    } catch (error) {
        note('hookDeviceState: ' + error);
        return false;
    }
}

function hookCreateDevice(dinput) {
    try {
        const vtable = dinput.readPointer();
        // IDirectInput8: CreateDevice is slot 3.
        const method = vtable.add(3 * Process.pointerSize).readPointer();
        Interceptor.attach(method, {
            onEnter(args) {
                // thiscall through Frida: args[0] is `this`, then rguid, ppvOut, punkOuter.
                this.out = args[2];
                if (state.samples.length < 6) {
                    state.samples.push({call: 'CreateDevice', args: [0, 1, 2, 3].map(
                        (i) => String(args[i]))});
                }
            },
            onLeave(retval) {
                if (retval.toInt32() !== 0 || this.out === null) return;
                try {
                    if (this.out.isNull()) return;
                    const device = this.out.readPointer();
                    if (!device.isNull()) state.devices += 1;
                    if (!device.isNull()) hookDeviceState(device);
                } catch (error) {
                    note('create device: ' + error);
                }
            },
        });
        state.devices = 0;
        return true;
    } catch (error) {
        note('hookCreateDevice: ' + error);
        return false;
    }
}

function installDinput() {
    if (state.dinput && state.dinput.installed) return state.dinput;
    const result = {installed: false, hooks: [], errors: state.errors};
    let module = null;
    try { module = Process.findModuleByName('dinput8.dll'); } catch (error) { note(String(error)); }
    if (module === null) {
        state.dinput = result;
        return result;
    }
    state.devices = state.devices || 0;
    const address = module.findExportByName('DirectInput8Create');
    if (address === null) {
        state.dinput = result;
        return result;
    }
    try {
        Interceptor.attach(address, {
            onEnter(args) {
                this.out = args[3];              // LPVOID* ppvOut (5th argument overall)
                if (state.samples.length < 6) {
                    state.samples.push({call: 'DirectInput8Create', args: [0, 1, 2, 3, 4].map(
                        (i) => String(args[i]))});
                }
            },
            onLeave(retval) {
                if (retval.toInt32() !== 0 || this.out === null) return;
                try {
                    if (this.out.isNull()) return;
                    const dinput = this.out.readPointer();
                    if (state.samples.length < 6) {
                        state.samples.push({call: 'DirectInput8Create.out', object: String(dinput),
                                            vtable: String(dinput.readPointer())});
                    }
                    if (!dinput.isNull() && hookCreateDevice(dinput)) {
                        state.dinput.hooks.push('IDirectInput8::CreateDevice');
                    }
                } catch (error) {
                    note('DirectInput8Create: ' + error);
                }
            },
        });
        state.dinput = result;
        result.installed = true;
        result.hooks.push('DirectInput8Create');
    } catch (error) {
        note('DirectInput8Create hook: ' + error);
        state.dinput = result;
    }
    return result;
}

rpc.exports = {
    modules: function () {
        return inputModules();
    },
    // Count-only hooks on the input APIs a Windows game can use. Tells the harness
    // which delivery path the engine actually reads, without changing anything.
    apis: function () {
        if (state.apiHooks) return state.apiHooks;
        const counts = {};
        const hooked = [];
        const targets = [
            ['dinput8.dll', 'DirectInput8Create'],
            ['user32.dll', 'GetKeyboardState'],
            ['user32.dll', 'GetAsyncKeyState'],
            ['user32.dll', 'GetKeyState'],
            ['user32.dll', 'GetRawInputData'],
            ['user32.dll', 'GetRawInputBuffer'],
            ['user32.dll', 'PeekMessageA'],
            ['user32.dll', 'PeekMessageW'],
            ['user32.dll', 'GetMessageA'],
            ['user32.dll', 'GetMessageW'],
            ['user32.dll', 'DispatchMessageA'],
            ['user32.dll', 'DispatchMessageW'],
        ];
        for (const [moduleName, exportName] of targets) {
            let module = null;
            try { module = Process.findModuleByName(moduleName); } catch (error) { note(String(error)); }
            if (module === null) continue;
            let address = null;
            try { address = module.findExportByName(exportName); } catch (error) { note(String(error)); }
            if (address === null) continue;
            try {
                if (exportName === 'GetKeyState' || exportName === 'GetAsyncKeyState') {
                    Interceptor.attach(address, {
                        onEnter(args) {
                            recordCall(exportName, this.returnAddress, args[0].toInt32());
                        },
                    });
                } else if (exportName === 'PeekMessageA' || exportName === 'PeekMessageW') {
                    // The message pump: its return address is where the engine looks at
                    // messages right after peeking, which is where keys are dispatched.
                    Interceptor.attach(address, {
                        onEnter(args) {
                            recordPump(exportName, this.returnAddress);
                            counts[exportName] = (counts[exportName] || 0) + 1;
                        },
                    });
                } else {
                    Interceptor.attach(address, {
                        onEnter(args) { counts[exportName] = (counts[exportName] || 0) + 1; },
                    });
                }
                hooked.push(exportName);
            } catch (error) {
                note('api hook ' + exportName + ': ' + error);
            }
        }
        state.apiHooks = {hooked: hooked, counts: counts};
        return state.apiHooks;
    },
    apiCounts: function () {
        return state.apiHooks || {hooked: [], counts: {}};
    },
    // The message pump: where PeekMessageA returns, with a live disassembly of the hottest
    // sites. The key dispatch and its "input enabled" check sit right after the peek.
    pump: function (limit, count) {
        const entries = Object.keys(state.pump)
            .map((key) => ({key: key, count: state.pump[key]}))
            .sort((a, b) => b.count - a.count)
            .slice(0, limit || 6);
        const listing = {};
        for (const entry of entries) {
            const match = entry.key.match(/\+([0-9a-f]+)$/);
            if (match === null) continue;
            const rva = parseInt(match[1], 16);
            if (listing[match[1]] !== undefined) continue;
            listing[match[1]] = disassemble(rva, 0x30, count || 120);
        }
        return {entries: entries, disasm: listing, pumpCalls: state.pumpCalls,
                keyCalls: state.keyCalls, noReturn: state.noReturn};
    },
    // Who called GetKeyState/GetAsyncKeyState, with the key code, grouped by call site.
    callers: function (limit) {
        const entries = Object.keys(state.callers)
            .map((key) => ({key: key, count: state.callers[key]}))
            .sort((a, b) => b.count - a.count)
            .slice(0, limit || 12);
        return {entries: entries, keyCalls: state.keyCalls, noReturn: state.noReturn};
    },
    // Hold DIK keys from inside the process: {w: true, space: false, ...}
    keys: function (wanted) {
        installDinput();
        const next = {};
        for (const name of Object.keys(wanted || {})) {
            if (DIK[name] === undefined) continue;
            next[name] = !!wanted[name];
        }
        const names = {};
        for (const name of Object.keys(state.keys)) names[name] = true;
        for (const name of Object.keys(next)) names[name] = true;
        for (const name of Object.keys(names)) {
            const before = !!state.keys[name];
            const after = !!next[name];
            if (before !== after) {
                state.keyQueue.push({ofs: DIK[name], data: after ? 0x80 : 0x00});
            }
        }
        state.keys = next;
        return {keys: state.keys, queued: state.keyQueue.length, dinput: state.dinput};
    },
    dinput: function () {
        return installDinput();
    },
    dinputTelemetry: function () {
        return {
            dinput: state.dinput,
            devices: state.devices || 0,
            deviceStates: state.deviceStates,
            stateCalls: state.stateCalls,
            dataCalls: state.dataCalls,
            dataSizes: state.dataSizes,
            queued: state.keyQueue.length,
            keyWrites: state.keyWrites,
            samples: state.samples,
            deviceInfo: state.deviceInfo,
            keys: state.keys,
            errors: state.errors,
        };
    },
    install: function () {
        if (state.installed) return {installed: true, hooks: state.hooks, errors: state.errors};
        const names = ['xinput1_3.dll', 'xinput9_1_0.dll', 'xinput1_4.dll'];
        for (const name of names) {
            hookExport(name, 'XInputGetCapabilities');
            hookExport(name, 'XInputGetState');
            hookExport(name, 'XInputGetStateEx');
        }
        state.installed = state.hooks.some((hook) => hook.indexOf('XInputGetCapabilities') >= 0);
        return {installed: state.installed, hooks: state.hooks, errors: state.errors,
                modules: inputModules()};
    },
    push: function (pad) {
        state.pad = {
            lx: (pad && pad.lx) | 0, ly: (pad && pad.ly) | 0,
            rx: (pad && pad.rx) | 0, ry: (pad && pad.ry) | 0,
            buttons: (pad && pad.buttons) | 0,
        };
        state.active = true;
        return {active: true, pad: state.pad};
    },
    release: function () {
        state.active = false;
        return {active: false};
    },
    telemetry: function () {
        return {
            installed: state.installed,
            hooks: state.hooks,
            calls: state.calls,
            capabilityCalls: state.capabilityCalls,
            frame: state.frame,
            active: state.active,
            pad: state.pad,
            lastWrite: state.lastWrite,
            errors: state.errors,
            modules: inputModules(),
            apiHooks: state.apiHooks || null,
        };
    },
};
