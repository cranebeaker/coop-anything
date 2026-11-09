"""Synthetic filesystem tests. No game/saves/Steam calls or external packages."""
from pathlib import Path
import importlib.util
import tempfile

path = Path(__file__).resolve().parents[1] / 'testkit/dlc-readiness/readiness.py'
spec = importlib.util.spec_from_file_location('readiness', path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

with tempfile.TemporaryDirectory(prefix='ddcoop-dlc-fixture-') as tmp:
    root = Path(tmp)
    report = module.inspect(root)
    assert not report['known_game_build'] and not report['live_dlc_support_verified']
    assert len(report['features']) == 6 and all(not f['data_present'] for f in report['features'])
    for name, (package, subdir, _) in module.FEATURES.items():
        directory = root / 'dlc' / package / subdir
        directory.mkdir(parents=True, exist_ok=True)
        (directory / 'data.json').write_text('{"value":1}', encoding='utf-8')
    arena = root / 'dlc/1117860_arena_mp'
    arena.mkdir()
    (arena / 'data.json').write_text('unrelated', encoding='utf-8')
    before = {p.relative_to(root).as_posix(): p.read_bytes() for p in root.rglob('*') if p.is_file()}
    first = module.inspect(root)
    assert first == module.inspect(root)
    assert {f['feature'] for f in first['features']} == set(module.FEATURES)
    assert all(f['data_present'] and f['data_files'] == 1 and len(f['data_sha256']) == 64 for f in first['features'])
    after = {p.relative_to(root).as_posix(): p.read_bytes() for p in root.rglob('*') if p.is_file()}
    assert before == after
    selected = module.inspect(root, ['flagellant', 'flagellant'])
    assert len(selected['features']) == 1 and selected['features'][0]['feature'] == 'flagellant'
    data = root / 'dlc/580100_crimson_court/features/flagellant/data.json'
    data.write_text('{"value":2}', encoding='utf-8')
    assert module.inspect(root, ['flagellant'])['features'][0]['data_sha256'] != selected['features'][0]['data_sha256']
    # Presence of a Crimson Court package alone never stands in for a feature.
    data.unlink()
    assert not module.inspect(root, ['flagellant'])['features'][0]['data_present']
    try:
        module.inspect(root, ['arena_mp'])
        raise AssertionError('Arena feature accepted')
    except ValueError:
        pass
    for hero in ['shieldbreaker', 'musketeer']:
        package = module.FEATURES[hero][0]
        directory = root / 'dlc' / package / 'heroes' / hero
        directory.mkdir(parents=True)
        info = directory / (hero + '.info.darkest')
        info.write_text('\n'.join(f'combat_skill: .id "skill{i}" .level {level}'
                                  for i in range(7) for level in range(5)), encoding='utf-8')
        parsed = module.inspect(root, [hero])['features'][0]['hero_data']
        assert parsed['combat_skill_count'] == 7 and parsed['levels'] == list(range(5)) and parsed['fits_existing_skill_codec']
        assert not parsed['live_hero_support_verified']
        with info.open('a', encoding='utf-8') as f:
            f.write('\ncombat_skill: .id "extra" .level 5\n')
        assert not module.inspect(root, [hero])['features'][0]['hero_data']['fits_existing_skill_codec']
print('PASS DLC readiness: six normal features, feature-specific roots, selected subset, stable/change-sensitive hashes, absent/empty data, arena exclusion, no mutation')
print('PASS DLC heroes: levelled definitions deduplicated, Musketeer/Shieldbreaker32-slot/level4 codec bounds verified; no live-support claim')
