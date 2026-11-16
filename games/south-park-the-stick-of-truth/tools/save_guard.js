const config = __CONFIG__;
const retained = [];
const abi = Process.arch === 'ia32' ? 'stdcall' : 'default';
const deniedStatus = -1073741790;
const pointerSize = Process.pointerSize;
const oaSize = pointerSize === 8 ? 48 : 24;
const nameOffset = pointerSize === 8 ? 16 : 8;
const rootOffset = pointerSize === 8 ? 8 : 4;
const stringPointerOffset = pointerSize === 8 ? 8 : 4;
const mutations = 0x500d0116;
let eventCount = 0;
const seenEvents = new Set();
const counters = {};

function event(kind, details) {
    counters[kind] = (counters[kind] || 0) + 1;
    const key = kind + JSON.stringify(details);
    if (seenEvents.has(key)) return;
    seenEvents.add(key);
    if (eventCount++ < 300 || kind === 'guard-error') send({kind, ...details});
}

const nativeTraceCounts = {};
function scanLoadedScripts() {
    const output = [];
    const signatures = config.scriptSignatures || [];
    let scanned = 0;
    const seenRanges = new Set();
    for (const protection of ['r--', 'rw-', 'r-x', 'rwx']) for (const range of Process.enumerateRanges({protection, coalesce: true})) {
        const rangeKey = range.base + ':' + range.size;
        if (seenRanges.has(rangeKey)) continue;
        seenRanges.add(rangeKey);
        if (scanned >= 512 * 1024 * 1024 || range.size > 64 * 1024 * 1024) continue;
        scanned += range.size;
        for (const signature of signatures) {
            if (output.some(item => item.name === signature.name)) continue;
            try {
                const matches = Memory.scanSync(range.base, range.size, signature.pattern);
                if (matches.length) output.push({name: signature.name, address: matches[0].address.toString(), protection: range.protection});
            } catch (_) {}
        }
    }
    return {scannedBytes: scanned, hits: output};
}
const nativeTraceListeners = [];
for (const candidate of (config.traceNatives || [])) {
    try {
        const address = Process.mainModule.base.add(candidate.function_rva);
        nativeTraceListeners.push(Interceptor.attach(address, {
            onEnter(args) {
                const count = (nativeTraceCounts[candidate.name] || 0) + 1;
                nativeTraceCounts[candidate.name] = count;
                if (count <= 8) {
                    let params = [];
                    try { for (let i = 0; i < 4; i++) params.push(args[1].add(i * 4).readS32()); } catch (_) { params = ['unreadable']; }
                    event('native-call', {name: candidate.name, params});
                }
            }
        }));
    } catch (error) { event('native-trace-unavailable', {name: candidate.name, error: String(error)}); }
}

rpc.exports = {
    telemetry() {
        const nativeLayout = [];
        for (const candidate of (config.nativeCandidates || [])) {
            try {
                const record = Process.mainModule.base.add(candidate.record_rva);
                const name = record.readPointer().readCString();
                const handler = record.add(4).readPointer();
                const expected = Process.mainModule.base.add(candidate.function_rva);
                const valid = name === candidate.name && handler.equals(expected);
                const instructions = [];
                if (valid && Process.findRangeByAddress(handler).protection.includes('x')) {
                    let address = handler;
                    for (let index = 0; index < 12; index++) {
                        const instruction = Instruction.parse(address);
                        instructions.push(instruction.toString());
                        address = instruction.next;
                        if (instruction.mnemonic.startsWith('ret')) break;
                    }
                }
                nativeLayout.push({name: candidate.name, matches: valid, instructions});
            } catch (error) { nativeLayout.push({name: candidate.name, error: String(error)}); }
        }
        return {counters, nativeTraceCounts, loadedScripts: scanLoadedScripts(), modules: Process.enumerateModules().map(module => module.name), nativeLayout};
    }
};

function exported(module, name) {
    const address = Process.getModuleByName(module).findExportByName(name);
    if (address === null) throw new Error('Missing required export ' + name);
    return address;
}

const getPath = new NativeFunction(exported('kernel32.dll', 'GetFinalPathNameByHandleW'), 'uint32', ['pointer', 'pointer', 'uint32', 'uint32'], abi);
const queryDevice = new NativeFunction(exported('kernel32.dll', 'QueryDosDeviceW'), 'uint32', ['pointer', 'pointer', 'uint32'], abi);
const getFileType = new NativeFunction(exported('kernel32.dll', 'GetFileType'), 'uint32', ['pointer'], abi);
const queryObject = new NativeFunction(exported('ntdll.dll', 'NtQueryObject'), 'int32', ['pointer', 'uint32', 'pointer', 'uint32', 'pointer'], abi);
const devices = [];
for (const drive of ['C:', 'D:']) {
    const output = Memory.alloc(2048);
    if (queryDevice(Memory.allocUtf16String(drive), output, 1024)) devices.push([output.readUtf16String().toLowerCase(), drive]);
}

function normalize(path) {
    let value = path.replaceAll('/', '\\');
    if (value.startsWith('\\??\\') || value.startsWith('\\\\?\\')) value = value.slice(4);
    for (const [device, drive] of devices) {
        if (value.toLowerCase().startsWith(device + '\\')) value = drive + value.slice(device.length);
    }
    if (/^[a-z]:\\/i.test(value)) {
        const parts = [];
        for (const part of value.slice(3).split('\\')) {
            if (!part || part === '.') continue;
            if (part === '..') parts.pop();
            else parts.push(part);
        }
        value = value.slice(0, 3) + parts.join('\\');
    }
    return value.replace(/\\+$/, '');
}

function under(path, parent) {
    path = normalize(path).toLowerCase();
    parent = normalize(parent).toLowerCase();
    return path === parent || path.startsWith(parent + '\\');
}

function handlePath(handle) {
    const buffer = Memory.alloc(65536);
    const length = getPath(handle, buffer, 32768, 0);
    if (length > 0 && length < 32768) return normalize(buffer.readUtf16String(length));
    const returned = Memory.alloc(4);
    if (queryObject(handle, 1, buffer, 65536, returned) >= 0 && buffer.readU16() > 0) {
        return normalize(buffer.add(stringPointerOffset).readPointer().readUtf16String(buffer.readU16() / 2));
    }
    return null;
}

function objectPath(oa) {
    const string = oa.add(nameOffset).readPointer();
    const length = string.readU16();
    const buffer = string.add(stringPointerOffset).readPointer();
    let path = length === 0 || buffer.isNull() ? '' : buffer.readUtf16String(length / 2);
    const root = oa.add(rootOffset).readPointer();
    if (!root.isNull() && (path === '' || (!path.startsWith('\\') && !/^[a-z]:/i.test(path)))) {
        const parent = handlePath(root);
        if (parent === null && path === '' && getFileType(root) === 3) {
            event('pipe-root-reopen', {});
            return '\\Device\\NamedPipe\\<existing-pipe>';
        }
        if (parent === null) throw new Error('Cannot resolve root handle ' + root + ' type=' + getFileType(root) + ' name=' + JSON.stringify(path));
        path = parent + '\\' + path;
    }
    return normalize(path);
}

function redirect(oa, path) {
    const rules = [{from: config.originalSaves, to: config.testSaves}, ...(config.redirections || [])];
    const rule = rules.find(rule => under(path, rule.from));
    if (rule === undefined) return {copy: oa, destination: path};
    const mapped = rule.to + path.slice(normalize(rule.from).length);
    const text = Memory.allocUtf16String('\\??\\' + mapped);
    const string = Memory.alloc(pointerSize === 8 ? 16 : 8);
    const bytes = ('\\??\\' + mapped).length * 2;
    string.writeU16(bytes);
    string.add(2).writeU16(bytes + 2);
    string.add(stringPointerOffset).writePointer(text);
    const copy = Memory.alloc(oaSize);
    Memory.copy(copy, oa, oaSize);
    copy.add(rootOffset).writePointer(NULL);
    copy.add(nameOffset).writePointer(string);
    event('redirect', {from: path, to: mapped});
    return {copy, string, text, destination: mapped};
}

function permittedWrite(path) {
    if (under(path, config.testSaves) || config.writeRoots.some(root => under(path, root))) return true;
    const lower = path.toLowerCase();
    if (lower.startsWith('hid#')) return true;
    return lower.startsWith('\\device\\') && !lower.startsWith('\\device\\harddisk') && !lower.startsWith('\\device\\mup');
}

function replace(name, resultType, argTypes, factory) {
    let original;
    const callback = new NativeCallback(function (...args) { return factory(original, args); }, resultType, argTypes, abi);
    const trampoline = Interceptor.replaceFast(exported('ntdll.dll', name), callback);
    original = new NativeFunction(trampoline, resultType, argTypes, abi);
    retained.push(callback, original);
}

function logPath(path, api) {
    const filters = config.logPaths || [];
    if (filters.length === 0) return;
    const lower = path.toLowerCase();
    if (filters.some(filter => lower.includes(filter.toLowerCase()))) event('path-open', {api, path});
}

for (const [name, count] of [['NtCreateFile', 11], ['NtOpenFile', 6]]) {
    const types = ['pointer', 'uint32', 'pointer', 'pointer'];
    while (types.length < count) types.push(types.length === 4 && count === 11 ? 'pointer' : 'uint32');
    if (count === 11) { types[9] = 'pointer'; types[10] = 'uint32'; }
    replace(name, 'int32', types, (original, args) => {
        try {
            const path = objectPath(args[2]);
            logPath(path, name);
            const mapped = redirect(args[2], path);
            const destination = mapped.destination;
            const writes = (args[1] & mutations) !== 0 || (count === 11 && args[7] !== 1);
            if (writes && !permittedWrite(destination)) {
                event('blocked-write-open', {path, access: args[1], disposition: count === 11 ? args[7] : 1});
                return deniedStatus;
            }
            if (mapped.copy !== undefined) args[2] = mapped.copy;
            return original(...args);
        } catch (error) { event('guard-error', {api: name, error: String(error)}); return deniedStatus; }
    });
}

replace('NtDeleteFile', 'int32', ['pointer'], (original, args) => {
    try {
        const path = objectPath(args[0]);
        const mapped = redirect(args[0], path);
        const destination = mapped.destination;
        if (!permittedWrite(destination)) { event('blocked-delete', {path}); return deniedStatus; }
        if (mapped.copy !== undefined) args[0] = mapped.copy;
        return original(...args);
    } catch (error) { event('guard-error', {api: 'NtDeleteFile', error: String(error)}); return deniedStatus; }
});

replace('NtSetInformationFile', 'int32', ['pointer', 'pointer', 'pointer', 'uint32', 'uint32'], (original, args) => {
    const allowedClasses = [4, 13, 14, 19, 20, 64];
    try {
        if (args[4] === 14) return original(...args);
        const path = handlePath(args[0]);
        if (path === null || !permittedWrite(path) || !allowedClasses.includes(args[4])) {
            event('blocked-file-info', {path, infoClass: args[4]}); return deniedStatus;
        }
        return original(...args);
    } catch (error) { event('guard-error', {api: 'NtSetInformationFile', error: String(error)}); return deniedStatus; }
});

replace('NtWriteFile', 'int32', ['pointer', 'pointer', 'pointer', 'pointer', 'pointer', 'pointer', 'uint32', 'pointer', 'pointer'], (original, args) => {
    try {
        const path = handlePath(args[0]);
        if ((path === null && ![2, 3].includes(getFileType(args[0]))) || (path !== null && !permittedWrite(path))) { event('blocked-write', {path}); return deniedStatus; }
        return original(...args);
    } catch (error) { event('guard-error', {api: 'NtWriteFile', error: String(error)}); return deniedStatus; }
});

replace('NtCreateUserProcess', 'int32', Array(11).fill('pointer'), (original, args) => {
    let image = null;
    try {
        const text = args[8].add(pointerSize === 8 ? 96 : 56);
        image = text.add(stringPointerOffset).readPointer().readUtf16String(text.readU16() / 2);
    } catch (_) {}
    event('blocked-child-process', {image}); return deniedStatus;
});

if (config.blockRemoteStorage) {
    const module = Process.findModuleByName('steam_api.dll');
    if (module === null) throw new Error('steam_api.dll must be loaded before resuming');
    const getter = module.findExportByName('SteamRemoteStorage');
    if (getter === null) throw new Error('SteamRemoteStorage export missing');
    const callback = new NativeCallback(() => { event('remote-storage-disabled', {}); return NULL; }, 'pointer', []);
    Interceptor.replace(getter, callback);
    retained.push(callback);
}

Interceptor.flush();
send({kind: 'guard-ready', arch: Process.arch, originalSaves: config.originalSaves, testSaves: config.testSaves});
