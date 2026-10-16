"""Toy-repository behavior checks; never read private game data."""
import copy
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
TOOL = Path(os.getenv('PACKAGER_TOOL', str(ROOT / (
    'tests/failing_baseline.py' if os.getenv('PACKAGER_BASELINE') else 'packager.py'))))


class PackagingTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.repo = self.root / 'repo with spaces'
        self.repo.mkdir()
        self.git('init', '-q')
        self.git('config', 'user.name', 'Toy Author')
        self.git('config', 'user.email', 'toy@example.invalid')
        (self.repo / 'main.py').write_bytes(b'print("old")\n')
        (self.repo / 'long.txt').write_bytes(b'original line: red flag; blue flag\n')
        (self.repo / 'run.sh').write_bytes(b'#!/bin/sh\nexit 0\n')
        (self.repo / 'run.sh').chmod(0o755)
        self.base = self.commit()
        (self.repo / 'main.py').write_bytes(b'print("new")\n')
        (self.repo / 'long.txt').write_bytes(b'original line: red flag; green flag\n')
        (self.repo / 'addon.py').write_bytes(b'VALUE = "original addon"\n')
        self.head = self.commit()
        self.manifest = self.root / 'manifest.json'
        self.out = self.root / 'new source'

    def git(self, *args):
        return subprocess.run(['git', '-C', str(self.repo), *args], check=True,
                              capture_output=True).stdout.strip().decode()

    def commit(self):
        self.git('add', '-A')
        self.git('commit', '-qm', 'toy fixture')
        return self.git('rev-parse', 'HEAD')

    def cli(self, *args):
        return subprocess.run([sys.executable, str(TOOL), *map(str, args)],
                              capture_output=True, text=True)

    def export(self, *extra, allow_addon=True):
        result = self.cli('export', '--repo', self.repo, '--base', self.base,
                          '--head', self.head, *(['--allow-new', 'addon.py'] if allow_addon else []), *extra)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.manifest.write_text(result.stdout)
        return json.loads(result.stdout)

    def apply(self, data=None, *extra):
        if data is not None:
            self.manifest.write_text(json.dumps(data))
        return self.cli('apply', '--repo', self.repo, '--base', self.base,
                        '--manifest', self.manifest, '--output', self.out, *extra)

    def reject(self, data, message):
        before = self.git('status', '--porcelain=v1', '-uall')
        result = self.apply(data)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(message, result.stderr)
        self.assertFalse(os.path.lexists(self.out))
        self.assertEqual(self.git('status', '--porcelain=v1', '-uall'), before)
        self.assertFalse(list(self.root.glob('.patch-stage-*')))

    def test_reconstructs_immutable_head_and_preserves_mode_without_worktree_writes(self):
        # Catches reading current files, dropping edits/new files or source mutation.
        self.export()
        (self.repo / 'main.py').write_text('dirty worker content\n')
        before = self.git('status', '--porcelain=v1', '-uall')
        result = self.apply()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.out / 'main.py').read_bytes(), b'print("new")\n')
        self.assertEqual((self.out / 'addon.py').read_bytes(), b'VALUE = "original addon"\n')
        self.assertEqual((self.out / 'run.sh').stat().st_mode & 0o777, 0o755)
        run = subprocess.run([sys.executable, str(self.out / 'main.py')], capture_output=True)
        self.assertEqual((run.returncode, run.stdout), (0, b'new\n'))
        self.assertEqual((self.repo / 'main.py').read_text(), 'dirty worker content\n')
        self.assertEqual(self.git('status', '--porcelain=v1', '-uall'), before)

    def test_exports_narrow_character_edits_without_original_line_context(self):
        data = self.export()
        entry = next(f for f in data['files'] if f['path'] == 'long.txt')
        inserted = ''.join(e['insert'] for e in entry['edits'])
        self.assertEqual(inserted, 'gren')
        self.assertEqual(sum(e['remove'] for e in entry['edits']), 3)
        self.assertNotIn('original line', self.manifest.read_text())
        self.assertNotIn('blue', self.manifest.read_text())

    def test_two_distant_changes_do_not_reinsert_unchanged_middle(self):
        (self.repo / 'long.txt').write_text('original line: RED flag; green FLAG\n')
        self.head = self.commit()
        data = self.export()
        entry = next(f for f in data['files'] if f['path'] == 'long.txt')
        self.assertNotIn(' flag; ', ''.join(e['insert'] for e in entry['edits']))
        result = self.apply(data)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.out / 'long.txt').read_text(), 'original line: RED flag; green FLAG\n')

    def test_deterministic_export_ignores_dirty_worktree(self):
        self.export()
        first = self.manifest.read_bytes()
        (self.repo / 'main.py').write_text('uncommitted\n')
        (self.repo / 'untracked.py').write_text('never export me\n')
        self.export()
        self.assertEqual(self.manifest.read_bytes(), first)

    def test_new_files_require_explicit_exact_allowlist(self):
        result = self.cli('export', '--repo', self.repo, '--base', self.base, '--head', self.head)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('allowlist', result.stderr)
        result = self.cli('export', '--repo', self.repo, '--base', self.base,
                          '--head', self.head, '--allow-new', '*.py')
        self.assertNotEqual(result.returncode, 0)

    def test_symbolic_or_short_commits_are_rejected(self):
        for ref in ('HEAD', self.head[:12], '--help'):
            result = self.cli('export', '--repo', self.repo, '--base', self.base,
                              '--head', ref, '--allow-new', 'addon.py')
            self.assertNotEqual(result.returncode, 0)

    def test_wrong_expected_base_is_rejected(self):
        self.export()
        result = self.apply(None, '--base', self.head)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('base', result.stderr)
        self.assertFalse(self.out.exists())

    def test_rejects_manifest_attacks_without_partial_output(self):
        original = self.export()
        attacks = []
        for path in ('../escape', '/absolute', 'a/../../escape', 'a//b', 'a\\b',
                     '.git/config', 'A/.GiT/config', './main.py', 'a\nfile', 'C:drive'):
            data = copy.deepcopy(original)
            data['files'][0]['path'] = path
            attacks.append((data, 'path'))
        for field, value, message in [('version', 999, 'version'), ('base', self.head, 'base')]:
            data = copy.deepcopy(original)
            data[field] = value
            attacks.append((data, message))
        data = copy.deepcopy(original)
        data['files'].append(data['files'][0])
        attacks.append((data, 'duplicate'))
        data = copy.deepcopy(original)
        data['files'][0]['after']['sha256'] = '0' * 64
        attacks.append((data, 'hash'))
        for data, message in attacks:
            with self.subTest(message=message, data=data):
                self.reject(data, message)

    def test_rejects_bad_base_hash_tampered_insert_and_malformed_edits(self):
        original = self.export()
        index = next(i for i, f in enumerate(original['files']) if f['path'] == 'main.py')
        for mutate, message in (
            (lambda f: f['before'].update(sha256='0' * 64), 'hash'),
            (lambda f: f['edits'][0].update(insert='tampered'), 'hash'),
            (lambda f: f['edits'][0].update(offset=-1), 'edit'),
            (lambda f: f['edits'][0].update(offset=99999), 'edit'),
            (lambda f: f['edits'][0].update(remove=99999), 'edit'),
            (lambda f: f['edits'][0].update(remove=True), 'edit'),
            (lambda f: f['edits'].append(dict(f['edits'][0])), 'edit'),
            (lambda f: f['edits'][0].update(insert='\x00'), 'binary'),
            (lambda f: f['edits'][0].update(unknown=1), 'schema'),
        ):
            data = copy.deepcopy(original)
            mutate(data['files'][index])
            with self.subTest(message=message):
                self.reject(data, message)

    def test_never_overwrites_existing_output_or_uses_source_as_output(self):
        self.export()
        self.out.mkdir()
        (self.out / 'keep').write_text('original')
        result = self.apply()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual((self.out / 'keep').read_text(), 'original')
        self.out = self.repo / 'nested-output'
        result = self.apply()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('source', result.stderr)
        self.assertFalse(self.out.exists())

    def test_rejects_symlink_output_parent_and_dangling_output(self):
        self.export()
        link = self.root / 'link'
        link.symlink_to(self.root, target_is_directory=True)
        self.out = link / 'escape'
        result = self.apply()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('symlink', result.stderr)
        self.assertFalse((self.root / 'escape').exists())
        self.out = self.root / 'dangling'
        self.out.symlink_to(self.root / 'absent')
        result = self.apply()
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(self.out.is_symlink())

    def test_binary_or_symlink_changed_files_cannot_export(self):
        (self.repo / 'addon.py').write_bytes(b'\x00\xff')
        self.head = self.commit()
        result = self.cli('export', '--repo', self.repo, '--base', self.base,
                          '--head', self.head, '--allow-new', 'addon.py')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('binary', result.stderr)
        (self.repo / 'addon.py').unlink()
        (self.repo / 'addon.py').symlink_to('main.py')
        self.head = self.commit()
        result = self.cli('export', '--repo', self.repo, '--base', self.base,
                          '--head', self.head, '--allow-new', 'addon.py')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('special', result.stderr)

    def test_base_special_file_failure_cleans_staging(self):
        (self.repo / 'asset-link').symlink_to('main.py')
        self.base = self.commit()
        (self.repo / 'main.py').write_text('print("third")\n')
        self.head = self.commit()
        self.export(allow_addon=False)
        result = self.apply()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('special', result.stderr)
        self.assertFalse(self.out.exists())
        self.assertFalse(list(self.root.glob('.patch-stage-*')))

    def test_untouched_base_binary_is_private_output_only(self):
        # Unchanged upstream bytes are never exported but belong in the private tree.
        (self.repo / 'asset.bin').write_bytes(b'\x00\xff\x80UPSTREAM_ONLY')
        self.base = self.commit()
        (self.repo / 'main.py').write_text('print("third")\n')
        self.head = self.commit()
        data = self.export(allow_addon=False)
        self.assertEqual([entry['path'] for entry in data['files']], ['main.py'])
        self.assertNotIn('UPSTREAM_ONLY', self.manifest.read_text())
        result = self.apply()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.out / 'asset.bin').read_bytes(), b'\x00\xff\x80UPSTREAM_ONLY')

    def test_repository_subdirectory_cannot_bypass_source_output_guard(self):
        self.export()
        subdir = self.repo / 'nested'
        subdir.mkdir()
        self.out = self.repo / 'source-escape'
        result = self.cli('apply', '--repo', subdir, '--base', self.base,
                          '--manifest', self.manifest, '--output', self.out)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('source', result.stderr)
        self.assertFalse(self.out.exists())

    def test_atomic_publication_race_preserves_a_new_existing_output(self):
        data = self.export()
        spec = importlib.util.spec_from_file_location('packager_under_test', TOOL)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        real_publish = module.publish
        created_inode = []

        def race(parent_fd, stage_name, output_name):
            self.out.mkdir()
            created_inode.append(self.out.stat().st_ino)
            return real_publish(parent_fd, stage_name, output_name)

        module.publish = race
        with self.assertRaises(module.PackagerError):
            module.apply_manifest(self.repo, data, self.out, self.base)
        self.assertEqual(self.out.stat().st_ino, created_inode[0])
        self.assertEqual(list(self.out.iterdir()), [])
        self.assertFalse(list(self.root.glob('.patch-stage-*')))

    def test_deletion_empty_file_unicode_crlf_and_mode_change_round_trip(self):
        (self.repo / 'main.py').unlink()
        (self.repo / 'addon.py').write_bytes('café\r\n'.encode())
        (self.repo / 'empty.txt').write_bytes(b'')
        (self.repo / 'long.txt').write_bytes(b'original line: red flag; blue flag')
        (self.repo / 'run.sh').chmod(0o644)
        self.head = self.commit()
        self.export('--allow-new', 'empty.txt')
        result = self.apply()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse((self.out / 'main.py').exists())
        self.assertEqual((self.out / 'addon.py').read_bytes(), 'café\r\n'.encode())
        self.assertEqual((self.out / 'empty.txt').read_bytes(), b'')
        self.assertEqual((self.out / 'long.txt').read_bytes(), b'original line: red flag; blue flag')
        self.assertEqual((self.out / 'run.sh').stat().st_mode & 0o777, 0o644)

    def test_git_hooks_and_filters_are_never_executed(self):
        marker = self.root / 'executed'
        hook = self.repo / '.git/hooks/post-checkout'
        hook.write_text('#!/bin/sh\ntouch "' + str(marker) + '"\n')
        hook.chmod(0o755)
        self.git('config', 'filter.toy.smudge', 'touch "' + str(marker) + '"')
        self.git('config', 'diff.toy.textconv', 'touch "' + str(marker) + '"')
        (self.repo / '.gitattributes').write_text('*.py filter=toy diff=toy\n')
        self.head = self.commit()
        self.export('--allow-new', '.gitattributes')
        result = self.apply()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(marker.exists())

    def test_large_generated_file_small_edits_are_narrow(self):
        (self.repo / 'long.txt').write_text('prefix ' + 'x' * 200000 + ' old\n')
        self.base = self.commit()
        (self.repo / 'long.txt').write_text('prefix ' + 'x' * 200000 + ' new\n')
        self.head = self.commit()
        data = self.export(allow_addon=False)
        self.assertEqual(data['files'][0]['edits'], [{'offset': 200008, 'remove': 3, 'insert': 'new'}])

    def test_large_unmatched_replacement_is_refused_without_broad_source_copy(self):
        (self.repo / 'long.txt').write_text('a' * 10000 + '\n')
        self.base = self.commit()
        (self.repo / 'long.txt').write_text('b' * 10000 + '\n')
        self.head = self.commit()
        result = self.cli('export', '--repo', self.repo, '--base', self.base, '--head', self.head)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('bounded', result.stderr)
        self.assertEqual(result.stdout, '')

    def test_existing_unicode_and_newline_boundaries_reconstruct_exactly(self):
        (self.repo / 'long.txt').write_bytes('café 🍵\r\nlast'.encode())
        self.base = self.commit()
        (self.repo / 'long.txt').write_bytes('cafè 🍶\r\nlast\n'.encode())
        self.head = self.commit()
        self.export(allow_addon=False)
        result = self.apply()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.out / 'long.txt').read_bytes(), 'cafè 🍶\r\nlast\n'.encode())

    def test_path_metacharacters_are_literal_and_never_execute_a_shell(self):
        path = 'literal $(touch SHELL_RAN); [x]*.txt'
        (self.repo / path).write_text('original\n')
        self.base = self.commit()
        (self.repo / path).write_text('changed\n')
        self.head = self.commit()
        self.export(allow_addon=False)
        result = self.apply()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.out / path).read_text(), 'changed\n')
        self.assertFalse((self.repo / 'SHELL_RAN').exists())

    def test_json_duplicate_keys_and_untrusted_descriptors_are_refused(self):
        data = self.export()
        self.manifest.write_text('{"version":1,"version":2}')
        result = self.apply()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('duplicate JSON', result.stderr)
        for field, value, message in [('after', {'sha256': '0' * 64, 'mode': '120000'}, 'special'),
                                      ('edits', 'invalid', 'edit')]:
            bad = copy.deepcopy(data)
            bad['files'][0][field] = value
            self.reject(bad, message)

    def test_base_file_presence_and_file_directory_conflicts_are_refused(self):
        data = self.export()
        bad = copy.deepcopy(data)
        index = next(i for i, e in enumerate(bad['files']) if e['path'] == 'main.py')
        bad['files'][index]['before'] = None
        self.reject(bad, 'base')
        bad = copy.deepcopy(data)
        entry = copy.deepcopy(next(e for e in data['files'] if e['path'] == 'addon.py'))
        entry['path'] = 'main.py/nested.py'
        bad['files'].append(entry)
        self.reject(bad, 'conflict')

    def test_base_symlink_cleanup_and_new_gitlink_refusal(self):
        (self.repo / 'link.py').symlink_to('main.py')
        self.base = self.commit()
        (self.repo / 'main.py').write_text('print("third")\n')
        self.head = self.commit()
        self.export(allow_addon=False)
        result = self.apply()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('special', result.stderr)
        self.assertFalse(self.out.exists())
        self.assertFalse(list(self.root.glob('.patch-stage-*')))
        self.git('update-index', '--add', '--cacheinfo', '160000,' + self.head + ',vendor/submodule')
        self.git('commit', '-qm', 'toy gitlink')
        self.head = self.git('rev-parse', 'HEAD')
        result = self.cli('export', '--repo', self.repo, '--base', self.base,
                          '--head', self.head, '--allow-new', 'vendor/submodule')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('special', result.stderr)

    def test_existing_empty_output_is_never_overwritten(self):
        self.export()
        self.out.mkdir()
        inode = self.out.stat().st_ino
        result = self.apply()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.out.stat().st_ino, inode)
        self.assertEqual(list(self.out.iterdir()), [])

    def test_unchanged_gitlink_is_uninitialized_with_explicit_provenance(self):
        pinned_submodule = self.head
        self.git('update-index', '--add', '--cacheinfo', '160000,' + pinned_submodule + ',vendor/upstream')
        self.git('commit', '-qm', 'toy base gitlink')
        self.base = self.git('rev-parse', 'HEAD')
        (self.repo / 'main.py').write_text('print("third")\n')
        self.git('add', 'main.py')
        self.git('commit', '-qm', 'change main only')
        self.head = self.git('rev-parse', 'HEAD')
        self.export(allow_addon=False)
        result = self.apply()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue((self.out / 'vendor/upstream').is_dir())
        self.assertEqual(list((self.out / 'vendor/upstream').iterdir()), [])
        provenance = json.loads(result.stdout)
        self.assertEqual(provenance['uninitialized_gitlinks'], [
            {'path': 'vendor/upstream', 'mode': '160000', 'commit': pinned_submodule}])
        self.assertNotIn('vendor/upstream', self.manifest.read_text())

    def test_modified_and_deleted_gitlinks_cannot_export(self):
        self.git('update-index', '--add', '--cacheinfo', '160000,' + self.head + ',vendor/upstream')
        self.git('commit', '-qm', 'toy base gitlink')
        self.base = self.git('rev-parse', 'HEAD')
        self.git('update-index', '--cacheinfo', '160000,' + self.base + ',vendor/upstream')
        self.git('commit', '-qm', 'change gitlink pin')
        modified = self.git('rev-parse', 'HEAD')
        self.git('update-index', '--force-remove', 'vendor/upstream')
        self.git('commit', '-qm', 'remove gitlink')
        deleted = self.git('rev-parse', 'HEAD')
        for head in (modified, deleted):
            result = self.cli('export', '--repo', self.repo, '--base', self.base, '--head', head)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('special', result.stderr)


if __name__ == '__main__':
    unittest.main()
