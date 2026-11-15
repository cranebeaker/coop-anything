"""South Park co-op launcher (first skeleton).

What it does today, and nothing more:
  - finds the game install (Steam library folders), checks the exe hash;
  - installs the mod (`dinput8.dll` next to the exe) and removes it again;
  - starts the game through Steam;
  - shows the truth: co-op is not playable yet, so Host/Join are disabled.

Run: py -3.14 -B launcher.py        (window)
     py -3.14 -B launcher.py status (console)
"""
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

EXPECTED_EXE_SHA256 = '54a83d055f97cad8beeb5aa027c96c0c2aa4a6c36394634270297dd47856a0d6'
EXE_NAME = 'South Park - The Stick of Truth.exe'
MOD_DLL = Path(__file__).resolve().parent.parent / 'mod' / 'build' / 'dinput8.dll'
APP_ID = '213670'


def steam_roots():
    roots = []
    for candidate in (r'C:\Program Files (x86)\Steam', r'D:\Steam', r'D:\SteamLibrary'):
        if Path(candidate, 'steamapps').is_dir():
            roots.append(Path(candidate))
    return roots


def library_folders():
    libraries = []
    for root in steam_roots():
        libraries.append(root)
        vdf = root / 'steamapps' / 'libraryfolders.vdf'
        if not vdf.is_file():
            continue
        text = vdf.read_text(encoding='utf-8', errors='replace')
        for match in re.finditer(r'"path"\s+"([^"]+)"', text):
            path = Path(match.group(1).replace('\\\\', '\\'))
            if path.is_dir():
                libraries.append(path)
    seen, unique = set(), []
    for library in libraries:
        key = str(library).lower()
        if key not in seen:
            seen.add(key)
            unique.append(library)
    return unique


def find_game():
    for library in library_folders():
        candidate = library / 'steamapps' / 'common' / 'South Park - The Stick of Truth' / EXE_NAME
        if candidate.is_file():
            return candidate
    return None


def sha256(path):
    digest = hashlib.sha256()
    with open(path, 'rb') as handle:
        for block in iter(lambda: handle.read(1 << 20), b''):
            digest.update(block)
    return digest.hexdigest()


def status():
    exe = find_game()
    if exe is None:
        return {'found': False, 'reason': 'game folder not found in any Steam library'}
    report = {'found': True, 'exe': str(exe), 'size': exe.stat().st_size,
              'sha256': sha256(exe)}
    report['hash_matches'] = report['sha256'] == EXPECTED_EXE_SHA256
    installed = exe.parent / 'dinput8.dll'
    report['mod_installed'] = installed.is_file()
    if installed.is_file():
        report['mod_sha256'] = sha256(installed)
    report['mod_built'] = MOD_DLL.is_file()
    return report


def _target_allowed(folder: Path) -> bool:
    """Only a copy named by SP_GAME_DIR is fair game; the retail install is not.

    Ground rule 2: never touch the owner's game install. The launcher is meant for players, who
    point it at their own install; in this workspace every test must use a copy, and touching the
    real folder needs COOP_ALLOW_RETAIL=1 on purpose.
    """
    copy_root = os.environ.get('SP_GAME_DIR')
    if copy_root:
        try:
            if Path(copy_root).resolve() == Path(folder).resolve():
                return True
        except OSError:
            pass
    return os.environ.get('COOP_ALLOW_RETAIL') == '1'


def install():
    exe = find_game()
    if exe is None:
        raise SystemExit('game folder not found')
    if not _target_allowed(exe.parent):
        raise SystemExit('refusing to write into the retail install; set SP_GAME_DIR to a test '
                         'copy, or COOP_ALLOW_RETAIL=1 if this is the player\'s own machine')
    if not MOD_DLL.is_file():
        raise SystemExit('mod not built: ' + str(MOD_DLL))
    target = exe.parent / 'dinput8.dll'
    if target.is_file():
        shutil.copy2(target, target.with_suffix('.dll.before-coop'))
    shutil.copy2(MOD_DLL, target)
    return {'installed': str(target), 'sha256': sha256(target)}


def uninstall():
    exe = find_game()
    if exe is None:
        raise SystemExit('game folder not found')
    target = exe.parent / 'dinput8.dll'
    backup = target.with_suffix('.dll.before-coop')
    if target.is_file():
        target.unlink()
    if backup.is_file():
        shutil.copy2(backup, target)
        backup.unlink()
    return {'removed': str(target)}


def launch():
    exe = find_game()
    if exe is None:
        raise SystemExit('game folder not found')
    if not _target_allowed(exe.parent):
        raise SystemExit('refusing to start the retail install from a test run; set SP_GAME_DIR to '
                         'a test copy, or COOP_ALLOW_RETAIL=1 if this is the player\'s own machine')
    subprocess.run(['cmd', '/c', 'start', '', 'steam://rungameid/' + APP_ID], check=False)
    return {'launched': APP_ID}


def window():
    import tkinter as tk
    from tkinter import messagebox

    root = tk.Tk()
    root.title('South Park co-op')
    text = tk.Text(root, width=76, height=14, font=('Consolas', 9))
    text.pack(padx=8, pady=8)

    def show():
        info = status()
        text.delete('1.0', 'end')
        text.insert('end', json.dumps(info, indent=2))
        if info.get('found') and not info.get('hash_matches'):
            text.insert('end', '\n\nWARNING: exe hash differs from the one this build was tested on.')

    def do_install():
        try:
            show_after(install())
        except SystemExit as error:
            messagebox.showerror('co-op', str(error))

    def do_uninstall():
        try:
            show_after(uninstall())
        except SystemExit as error:
            messagebox.showerror('co-op', str(error))

    def show_after(result):
        messagebox.showinfo('co-op', json.dumps(result, indent=2))
        show()

    bar = tk.Frame(root)
    bar.pack(pady=6)
    for label, action in (('Status', show), ('Install mod', do_install),
                          ('Remove mod', do_uninstall), ('Start game', launch)):
        tk.Button(bar, text=label, command=action).pack(side='left', padx=4)
    for label in ('Host', 'Join'):
        button = tk.Button(bar, text=label, state='disabled')
        button.pack(side='left', padx=4)
    tk.Label(root, text='Host and Join appear when co-op is playable: today only the mod installs '
                        'and the game starts.').pack(pady=(0, 8))
    show()
    root.mainloop()


def main(argv):
    command = argv[1] if len(argv) > 1 else 'window'
    if command == 'window':
        window()
        return 0
    if command == 'status':
        print(json.dumps(status(), indent=2))
        return 0
    if command == 'install':
        print(json.dumps(install(), indent=2))
        return 0
    if command == 'uninstall':
        print(json.dumps(uninstall(), indent=2))
        return 0
    if command == 'launch':
        print(json.dumps(launch(), indent=2))
        return 0
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
