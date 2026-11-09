"""Builds DDCoop-install.zip: the .bat files, LEESMIJ.txt and files\\ (DDCoop.exe + Codex's bootstrap and runtime).

    python3 -I tools/make_dropkit.py <codex kit dir> <out.zip> [version]

Every binary is checked against launcher/src/lr_hashes.h (bootstrap, runtime) before it goes in; the zip is
refused if anything differs. Text files get Windows line endings."""
import hashlib
import os
import re
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def sha(path):
    with open(path, 'rb') as f:
        return hashlib.sha256(f.read()).hexdigest()


def hashes():
    text = open(os.path.join(ROOT, 'launcher', 'src', 'lr_hashes.h')).read()
    return dict(re.findall(r'#define (LR_HASH_\w+) "([0-9a-f]{64})"', text))


def crlf(path):
    data = open(path, 'rb').read().replace(b'\r\n', b'\n')
    return data.replace(b'\n', b'\r\n')


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    codex, out = sys.argv[1], sys.argv[2]
    version = sys.argv[3] if len(sys.argv) > 3 else '1.0.0'
    h = hashes()
    files = {
        'DDCoop.exe': os.path.join(ROOT, 'build', 'win', 'DDCoop.exe'),
        'SDL2.dll': os.path.join(codex, 'SDL2.dll'),
        'ddcoop-runtime.dll': os.path.join(codex, 'DDCoop', 'ddcoop-runtime.dll'),
    }
    want = {'SDL2.dll': h['LR_HASH_BOOTSTRAP'], 'ddcoop-runtime.dll': h['LR_HASH_RUNTIME']}
    for name, path in files.items():
        if not os.path.isfile(path):
            sys.exit(f'missing: {path}')
        if name in want and sha(path) != want[name]:
            sys.exit(f'{name} has hash {sha(path)}, lr_hashes.h expects {want[name]}: refusing to build the kit')
    kit = os.path.join(ROOT, 'launcher', 'kit')
    lines = [f'  {name:20} {sha(path)}' for name, path in files.items()]
    readme = crlf(os.path.join(kit, 'LEESMIJ.txt')).decode('utf-8')
    readme = readme.replace('{VERSION}', version).replace('{HASHES}', '\r\n'.join(lines))
    tmp = out + '.tmp'
    with zipfile.ZipFile(tmp, 'w', zipfile.ZIP_DEFLATED) as z:
        top = 'DDCoop-install/'
        for bat in sorted(os.listdir(kit)):
            if bat.endswith('.bat'):
                data = crlf(os.path.join(kit, bat))
                data.decode('ascii')  # cmd.exe reads .bat files in the OEM code page: keep them ASCII
                z.writestr(top + bat, data)
        z.writestr(top + 'LEESMIJ.txt', '﻿'.encode('utf-8') + readme.encode('utf-8'))
        for name in sorted(os.listdir(kit)):
            if name.startswith('TESTSTAPPEN') and name.endswith('.txt'):
                z.writestr(top + name, b'\xef\xbb\xbf' + crlf(os.path.join(kit, name)))
        for name, path in files.items():
            z.write(path, top + 'files/' + name)
    os.replace(tmp, out)
    print(f'{out}: {os.path.getsize(out)} bytes, sha256 {sha(out)}')
    for line in lines:
        print(line)


if __name__ == '__main__':
    main()
