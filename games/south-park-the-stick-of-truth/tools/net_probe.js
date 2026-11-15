// Read-only network probe for the guarded South Park test session.
// Hooks only exported Winsock and Steamworks entry points; it never writes to
// game memory and never changes arguments or return values.
const counters = {};
const tracedFrames = {};
const details = {bind: [], connect: [], socket: [], wsaStartup: []};
let attached = 0;

function bump(key, payload) {
    counters[key] = (counters[key] || 0) + 1;
    if (payload !== undefined && details[key] !== undefined && details[key].length < 32) {
        details[key].push(payload);
    }
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

const winsockHooks = {
    'WSAStartup': {key: 'wsaStartup'},
    'socket': {key: 'socket'},
    'bind': {key: 'bind', parse: 1},
    'listen': {key: 'listen'},
    'accept': {key: 'accept'},
    'connect': {key: 'connect', parse: 1},
    'send': {key: 'send'},
    'sendto': {key: 'sendto', parse: 4},
    'recv': {key: 'recv'},
    'recvfrom': {key: 'recvfrom'},
    'closesocket': {key: 'closesocket'},
    'getaddrinfo': {key: 'getaddrinfo'},
    'gethostbyname': {key: 'gethostbyname'}
};

const steamHooks = [
    'SteamAPI_Init',
    'SteamAPI_Shutdown',
    'SteamGameServer_Init',
    'SteamGameServerNetworking',
    'SteamNetworking',
    'SteamMatchmaking',
    'SteamFriends',
    'SteamUserStats'
];

const instrumented = new Set();

function attachModule(module) {
    const name = module.name.toLowerCase();
    if (name !== 'ws2_32.dll' && name !== 'steam_api.dll') return;
    const moduleKey = name;
    if (instrumented.has(moduleKey)) return;
    instrumented.add(moduleKey);

    if (name === 'ws2_32.dll') {
        for (const [functionName, hook] of Object.entries(winsockHooks)) {
            const address = module.findExportByName(functionName);
            if (address === null) continue;
            try {
                Interceptor.attach(address, {
                    onEnter(args) {
                        if (hook.parse !== undefined) {
                            bump(hook.key, sockaddr(args[hook.parse]));
                        } else {
                            bump(hook.key);
                        }
                    }
                });
                attached += 1;
            } catch (error) {
                bump('attach-error', {name: functionName, error: String(error)});
            }
        }
    } else {
        for (const functionName of steamHooks) {
            const address = module.findExportByName(functionName);
            if (address === null) continue;
            try {
                Interceptor.attach(address, {
                    onEnter() {
                        bump(functionName);
                        if ((counters[functionName] || 0) <= 3) {
                            try {
                                const frames = Thread.backtrace(this.context, Backtracer.FUZZY).slice(0, 8).map(a => {
                                    const symbol = DebugSymbol.fromAddress(a);
                                    const delta = symbol.address === null ? 0 : a.sub(symbol.address).toInt32();
                                    return (symbol.moduleName || '?') + '!' + (symbol.name || '?') + '+' + delta;
                                });
                                tracedFrames[functionName] = tracedFrames[functionName] || [];
                                tracedFrames[functionName].push(frames);
                            } catch (error) {
                                bump('backtrace-error', {name: functionName, error: String(error)});
                            }
                        }
                    }
                });
                attached += 1;
            } catch (error) {
                bump('attach-error', {name: functionName, error: String(error)});
            }
        }
    }
    send({kind: 'net-probe-module', module: name, attached});
}

for (const module of Process.enumerateModules()) attachModule(module);

let moduleObserver = null;
try {
    moduleObserver = Process.attachModuleObserver({
        onAdded(module) { attachModule(module); }
    });
} catch (error) {
    bump('module-observer-fallback', {error: String(error)});
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
        return {counters, details, tracedFrames, attached, modules: Process.enumerateModules().map(m => m.name)};
    }
};

send({kind: 'net-probe-ready', attached});
