"""Compile valid mutated copies, then require real behavior tests to fail."""
import os
from pathlib import Path
import py_compile
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CONTROLS = (
    ('omit-insertion', "chunks.extend((before[cursor:offset], added))",
     "chunks.extend((before[cursor:offset], b''))",
     'test_reconstructs_immutable_head_and_preserves_mode_without_worktree_writes'),
    ('disable-final-hash', "require(sha(after) == entry['after']['sha256'], 'final file hash mismatch')",
     "require(True, 'final file hash mismatch')",
     'test_rejects_bad_base_hash_tampered_insert_and_malformed_edits'),
    ('disable-new-file-allowlist', 'require(additions <= approved,', 'require(True,',
     'test_new_files_require_explicit_exact_allowlist'),
    ('omit-failure-cleanup', 'shutil.rmtree(stage_name, dir_fd=parent_fd)', 'pass',
     'test_base_special_file_failure_cleans_staging'),
    ('disable-atomic-no-replace', 'os.fsencode(output_name), 1)', 'os.fsencode(output_name), 0)',
     'test_atomic_publication_race_preserves_a_new_existing_output'),
)


def main():
    original = (ROOT / 'packager.py').read_text()
    for name, old, new, test in CONTROLS:
        if original.count(old) != 1:
            raise AssertionError('mutation site is not unique: ' + name)
        with tempfile.TemporaryDirectory(prefix='packager-negative-') as directory:
            mutated = Path(directory) / 'mutated.py'
            mutated.write_text(original.replace(old, new))
            py_compile.compile(str(mutated), doraise=True)
            env = dict(os.environ, PACKAGER_TOOL=str(mutated))
            env.pop('PACKAGER_BASELINE', None)
            result = subprocess.run([sys.executable, str(ROOT / 'tests/test_packager.py'),
                                     'PackagingTests.' + test, '-v'], env=env,
                                    capture_output=True, text=True)
            if result.returncode != 1 or 'FAIL:' not in result.stderr or 'ERROR:' in result.stderr:
                sys.stderr.write(result.stdout + result.stderr)
                raise AssertionError('negative control did not fail normally: ' + name)
            print(name + ': expected behavioral assertion failure (exit 1; no compile/test errors)')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
