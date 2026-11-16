// Read-only network probe for the guarded South Park test session (step 1, Gate 0).
// Hooks exported Winsock and Steamworks entry points, plus the vtables of the Steam
// interfaces the game asks for (lobby, P2P, game server). It never writes game memory
// and never changes arguments or return values. Vtable slot labels are only indexes:
// their names are unknown without the Steamworks header, so they are reported as
// "<Interface>[<slot>]".
const counters = {};
const details = {socket: [], bind: [], listen: [], connect: [], wsaConnect: [], steam: []};
const tracedFrames = {};
const interfaces = {};
const hookedAddresses = new Set();
const LIMIT = 32;
const SLOTS = 64;
let attached = 0;
let wsaGetLastErrorFn = null;

function bump(key) {
    counters[key] = (counters[key] || 0) + 1;
    return counters[key];
}

function pushDetail(key, entry) {
    if (details[key].length < LIMIT) details[key].push(entry);
}

function sockaddr(pointer) {
    try {
        if (pointer.isNull()) return null;
        const family = pointer.readU16();
        if (family === 2) {
            const port = (pointer.add(2).readU8() << 8) | pointer.add(3).readU8();
            const bytes = [];
            for (let index = 4; index < 8; index++) bytes.push(pointer.add(index).readU8());
            return {family: 'inet', address: bytes.join('.'), port};
        }
        return {family};
    } catch (error) {
        return {error: String(error)};
    }
}

function lastError() {
    if (wsaGetLastErrorFn === null) return null;
    try {
        return wsaGetLastErrorFn();
    } catch (error) {
        return null;
    }
}

function trace(context, label) {
    if ((tracedFrames[label] || []).length >= 2) return;
    try {
        const frames = Thread.backtrace(context, Backtracer.FUZZY).slice(0, 8).map(a => {
            const symbol = DebugSymbol.fromAddress(a);
            const delta = symbol.address === null ? 0 : a.sub(symbol.address).toInt32();
            return (symbol.moduleName || '?') + '!' + (symbol.name || '?') + '+' + delta;
        });
        tracedFrames[label] = tracedFrames[label] || [];
        tracedFrames[label].push(frames);
    } catch (error) {
        bump('backtrace-error');
    }
}

// Winsock hooks: key = counter name, detail = list in details, addr = sockaddr argument index,
// ints = integer argument indexes to record, ret = record return value and WSAGetLastError.
const winsockHooks = [
    {name: 'WSAStartup', key: 'wsaStartup'},
    {name: 'socket', key: 'socket', detail: 'socket', ints: [0, 1, 2]},
    {name: 'bind', key: 'bind', detail: 'bind', addr: 1, ret: true},
    {name: 'listen', key: 'listen', detail: 'listen', ints: [1], ret: true},
    {name: 'accept', key: 'accept'},
    {name: 'connect', key: 'connect', detail: 'connect', addr: 1, ret: true},
    {name: 'WSAConnect', key: 'wsaConnect', detail: 'wsaConnect', addr: 1, ret: true},
    {name: 'WSASocketW', key: 'wsaSocketW'},
    {name: 'WSASocketA', key: 'wsaSocketA'},
    {name: 'send', key: 'send'},
    {name: 'sendto', key: 'sendto', addr: 4},
    {name: 'WSASendTo', key: 'wsaSendTo'},
    {name: 'WSASend', key: 'wsaSend'},
    {name: 'recv', key: 'recv'},
    {name: 'recvfrom', key: 'recvfrom'},
    {name: 'WSARecv', key: 'wsaRecv'},
    {name: 'WSARecvFrom', key: 'wsaRecvFrom'},
    {name: 'closesocket', key: 'closesocket'},
    {name: 'ioctlsocket', key: 'ioctlsocket'},
    {name: 'setsockopt', key: 'setsockopt'},
    {name: 'select', key: 'select'},
    {name: 'WSAEventSelect', key: 'wsaEventSelect'},
    {name: 'WSAAsyncSelect', key: 'wsaAsyncSelect'},
    {name: 'getaddrinfo', key: 'getaddrinfo'},
    {name: 'gethostbyname', key: 'gethostbyname'},
    {name: 'getsockname', key: 'getsockname'},
    {name: 'getpeername', key: 'getpeername'}
];

// Steam exports. Interface getters are hooked and their returned vtables are hooked too.
const steamGetters = ['SteamNetworking', 'SteamGameServerNetworking', 'SteamMatchmaking', 'SteamFriends',
    'SteamUser', 'SteamGameServer', 'SteamUtils', 'SteamUserStats', 'SteamApps'];
const steamPlain = ['SteamAPI_Init', 'SteamAPI_InitSafe', 'SteamAPI_Shutdown', 'SteamAPI_RunCallbacks',
    'SteamGameServer_Init', 'SteamGameServer_Shutdown', 'SteamAPI_RestartAppIfNecessary'];

function hookInterface(name, pointer) {
    if (pointer.isNull()) {
        bump(name + '-null');
        return;
    }
    const key = pointer.toString();
    if (interfaces[key]) return;
    const slots = [];
    interfaces[key] = {name, pointer: key, slots};
    try {
        const vtable = pointer.readPointer();
        for (let index = 0; index < SLOTS; index++) {
            const target = vtable.add(index * Process.pointerSize).readPointer();
            const range = Process.findRangeByAddress(target);
            if (range === null || range.protection.indexOf('x') < 0) break;
            const label = name + '[' + index + ']';
            slots.push(label);
            if (hookedAddresses.has(target.toString())) continue;
            hookedAddresses.add(target.toString());
            try {
                Interceptor.attach(target, {
                    onEnter(args) {
                        if (bump(label) <= 2) trace(this.context, label);
                    }
                });
                attached += 1;
            } catch (error) {
                bump('attach-error');
                pushDetail('steam', {label, error: String(error)});
            }
        }
    } catch (error) {
        bump('vtable-error');
        pushDetail('steam', {interface: name, error: String(error)});
    }
}

function attachWinsock(module) {
    const exported = (functionName) => module.findExportByName(functionName);
    const lastErrorAddress = exported('WSAGetLastError');
    if (lastErrorAddress !== null) wsaGetLastErrorFn = new NativeFunction(lastErrorAddress, 'int', []);
    for (const hook of winsockHooks) {
        const address = exported(hook.name);
        if (address === null) continue;
        try {
            Interceptor.attach(address, {
                onEnter(args) {
                    const count = bump(hook.key);
                    if (hook.detail === undefined) {
                        if (hook.key === 'closesocket' || hook.key === 'send' || hook.key === 'recv') {
                            if (count <= 2) trace(this.context, hook.key);
                        }
                        return;
                    }
                    const entry = {call: count};
                    if (hook.addr !== undefined) entry.addr = sockaddr(args[hook.addr]);
                    if (hook.ints !== undefined) entry.ints = hook.ints.map(index => args[index].toInt32());
                    this.entry = entry;
                    pushDetail(hook.detail, entry);
                    if (count <= 2) trace(this.context, hook.key);
                },
                onLeave(retval) {
                    if (hook.ret === true && this.entry !== undefined) {
                        this.entry.ret = retval.toInt32();
                        this.entry.lastError = lastError();
                    }
                }
            });
            attached += 1;
        } catch (error) {
            bump('attach-error');
            pushDetail('steam', {name: hook.name, error: String(error)});
        }
    }
}

function attachSteam(module) {
    for (const functionName of steamPlain) {
        const address = module.findExportByName(functionName);
        if (address === null) continue;
        try {
            Interceptor.attach(address, {
                onEnter(args) {
                    const count = bump(functionName);
                    if (functionName === 'SteamGameServer_Init') {
                        pushDetail('steam', {call: count, name: functionName});
                    }
                    if (count <= 3) trace(this.context, functionName);
                }
            });
            attached += 1;
        } catch (error) {
            bump('attach-error');
        }
    }
    for (const functionName of steamGetters) {
        const address = module.findExportByName(functionName);
        if (address === null) continue;
        try {
            Interceptor.attach(address, {
                onEnter() {
                    bump(functionName);
                },
                onLeave(retval) {
                    hookInterface(functionName, retval);
                }
            });
            attached += 1;
        } catch (error) {
            bump('attach-error');
        }
    }
}

const instrumented = new Set();

function attachModule(module) {
    const name = module.name.toLowerCase();
    if (name !== 'ws2_32.dll' && name !== 'steam_api.dll') return;
    if (instrumented.has(name)) return;
    instrumented.add(name);
    if (name === 'ws2_32.dll') attachWinsock(module);
    else attachSteam(module);
    send({kind: 'net-probe-module', module: name, attached});
}

for (const module of Process.enumerateModules()) attachModule(module);

try {
    Process.attachModuleObserver({
        onAdded(module) { attachModule(module); }
    });
} catch (error) {
    bump('module-observer-fallback');
    const seen = new Set();
    setInterval(() => {
        for (const module of Process.enumerateModules()) {
            if (!seen.has(module.name + module.base)) {
                seen.add(module.name + module.base);
                attachModule(module);
            }
        }
    }, 500);
}

rpc.exports = {
    telemetry() {
        return {
            counters, details, tracedFrames, attached,
            interfaces: Object.values(interfaces).map(entry => ({name: entry.name, pointer: entry.pointer, slotCount: entry.slots.length})),
            modules: Process.enumerateModules().map(m => m.name)
        };
    }
};

send({kind: 'net-probe-ready', attached});
