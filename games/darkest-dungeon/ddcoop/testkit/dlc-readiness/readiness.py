"""Read-only DLC groundwork. Prints fingerprints; never enables DLC or edits saves.

python -I testkit/dlc-readiness/readiness.py GAME_ROOT [--enabled FEATURE ...]
Installed data is not proof of ownership or live co-op compatibility.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import struct

KNOWN_EXE = 'f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12'
FEATURES = {
    'musketeer': ('445700_musketeer', '', ['hero skills/upgrades/ownership', 'camping']),
    'flagellant': ('580100_crimson_court', 'features/flagellant', ['hero skills/upgrades/ownership', 'special hero effects']),
    'districts': ('580100_crimson_court', 'features/districts', ['district purchases', 'shared costs', 'weekly district effects']),
    'crimson_court': ('580100_crimson_court', 'features/crimson_court', ['blood/curse/invitations', 'Courtyard departure/return', 'persistent quest progress']),
    'shieldbreaker': ('702540_shieldbreaker', '', ['hero skills/upgrades/ownership', 'camping/nightmare transitions']),
    'color_of_madness': ('735730_color_of_madness', '', ['shard currencies/shop', 'Farmstead departure', 'wave/reward/retreat transitions']),
}
DATA_SUFFIXES = {'.json', '.darkest', '.xml', '.csv', '.txt', '.loc'}


def fingerprint(directory):
    directory = directory.resolve()
    digest = hashlib.sha256()
    files = []
    for path in sorted(directory.rglob('*')):
        if not path.is_file() or path.suffix.lower() not in DATA_SUFFIXES:
            continue
        if not path.resolve().is_relative_to(directory) or path.is_symlink():
            raise ValueError('DLC data link leaves its feature directory')
        name = path.relative_to(directory).as_posix().encode('utf-8')
        data = path.read_bytes()
        digest.update(struct.pack('<I', len(name)) + name + struct.pack('<Q', len(data)) + data)
        files.append(path.relative_to(directory).as_posix())
    return digest.hexdigest() if files else None, len(files)


def inspect(game_root, enabled=None):
    root = Path(game_root)
    chosen = sorted(FEATURES if enabled is None else set(enabled))
    if set(chosen) - FEATURES.keys():
        raise ValueError('Unknown DLC feature; arena_mp is excluded')
    exe = root / '_windows/win64/Darkest.exe'
    game_hash = hashlib.sha256(exe.read_bytes()).hexdigest() if exe.is_file() else None
    result = {'schema': 1, 'known_game_build': game_hash == KNOWN_EXE,
              'game_sha256': game_hash, 'live_dlc_support_verified': False, 'features': []}
    for name in chosen:
        package, subdir, cases = FEATURES[name]
        relative = Path('dlc') / package / subdir
        directory = root / relative
        present = directory.is_dir()
        digest, count = fingerprint(directory) if present else (None, 0)
        profile = {'feature': name, 'relative_root': relative.as_posix(),
                                   'data_present': present and count > 0, 'data_files': count,
                                   'data_sha256': digest, 'status': 'needs_live_validation',
                                   'test_groups': cases}
        if name in {'musketeer', 'shieldbreaker'}:
            info = directory / 'heroes' / name / (name + '.info.darkest')
            skills = set()
            levels = set()
            if info.is_file():
                for line in info.read_text(encoding='utf-8-sig').splitlines():
                    if not line.startswith('combat_skill:'):
                        continue
                    identity = re.search(r'\.id\s+"([^"]+)"', line)
                    level = re.search(r'\.level\s+(\d+)', line)
                    if identity and level:
                        skills.add(identity.group(1)); levels.add(int(level.group(1)))
            profile['hero_data'] = {'combat_skill_count': len(skills), 'levels': sorted(levels),
                                    'fits_existing_skill_codec': 0 < len(skills) <= 32 and bool(levels) and max(levels) <= 4,
                                    'live_hero_support_verified': False}
        result['features'].append(profile)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game_root')
    parser.add_argument('--enabled', nargs='+', choices=sorted(FEATURES))
    args = parser.parse_args()
    print(json.dumps(inspect(args.game_root, args.enabled), indent=2))
