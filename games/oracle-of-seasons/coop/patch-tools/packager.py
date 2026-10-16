#!/usr/bin/env python3
"""Original insertion/removal manifests; raw Git objects in, new text tree out."""
import argparse
import ctypes
import difflib
import hashlib
import json
import os
from pathlib import Path
import re
import secrets
import shutil
import subprocess
import sys

PINNED_BASE = '533860de269e0e9b338e12f3484ce8497367e6ed'
FORMAT = 'original-source-edits'
VERSION = 1
MAX_MATCH_CHARS = 8192
MAX_BLOB_BYTES = 64 * 1024 * 1024
MAX_MANIFEST_BYTES = 64 * 1024 * 1024
SHA1 = re.compile(r'[0-9a-f]{40}\Z')
SHA256 = re.compile(r'[0-9a-f]{64}\Z')
HUNK = re.compile(rb'^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@')
MODES = {'100644', '100755'}


class PackagerError(ValueError):
    """A refused operation, with no promised output."""


def require(condition, message):
    if not condition:
        raise PackagerError(message)


def safe_path(path):
    require(isinstance(path, str) and bool(path), 'unsafe path')
    require(not any(ord(c) < 32 or ord(c) == 127 for c in path), 'unsafe path control character')
    require(not any(c in path for c in '\\:'), 'unsafe path separator')
    parts = path.split('/')
    require(all(p and p not in ('.', '..') and p.casefold() != '.git' for p in parts), 'unsafe path component')
    try:
        path.encode('utf-8', errors='strict')
    except UnicodeError as exc:
        raise PackagerError('unsafe path encoding') from exc
    return parts


def text(data):
    require(b'\x00' not in data, 'binary content refused')
    try:
        result = data.decode('utf-8', errors='strict')
    except UnicodeError as exc:
        raise PackagerError('binary or non-UTF-8 content refused') from exc
    require(not any(ord(c) < 32 and c not in '\t\n\r\f' for c in result), 'binary control content refused')
    return result


def sha(data):
    return hashlib.sha256(data).hexdigest()


def git(repo, *args):
    # No checkout/archive/filter-capable operations. Ignore inherited Git routing
    # variables and replacement objects; no shell, project hooks or textconv.
    env = {k: v for k, v in os.environ.items() if not k.startswith('GIT_')}
    env.update(LC_ALL='C', GIT_CONFIG_NOSYSTEM='1', GIT_CONFIG_GLOBAL=os.devnull)
    command = ['git', '--no-optional-locks', '--no-replace-objects', '-c',
               'core.hooksPath=' + os.devnull, '-c', 'core.attributesFile=' + os.devnull,
               '-C', os.fspath(repo), *args]
    result = subprocess.run(command, capture_output=True, env=env)
    require(result.returncode == 0, 'Git object operation failed')
    return result.stdout


def commit(repo, value):
    require(isinstance(value, str) and SHA1.fullmatch(value), 'commit must be an exact 40-character lowercase object ID')
    require(git(repo, 'cat-file', '-t', value).strip() == b'commit', 'object is not a commit')
    return value


def tree(repo, revision):
    result = {}
    for row in git(repo, 'ls-tree', '-rz', '--full-tree', revision).split(b'\x00'):
        if not row:
            continue
        metadata, raw_path = row.split(b'\t', 1)
        mode, kind, oid = metadata.decode('ascii').split(' ')
        try:
            path = raw_path.decode('utf-8', errors='strict')
        except UnicodeError as exc:
            raise PackagerError('unsafe path encoding') from exc
        safe_path(path)
        require(path not in result, 'duplicate tree path')
        result[path] = (mode, kind, oid)
    return result


def blob(repo, entry, *, require_text=True):
    mode, kind, oid = entry
    require(mode in MODES and kind == 'blob', 'unsupported special file')
    size = int(git(repo, 'cat-file', '-s', oid))
    require(size <= MAX_BLOB_BYTES, 'source blob size limit exceeded')
    value = git(repo, 'cat-file', 'blob', oid)
    require(len(value) == size, 'Git blob size mismatch')
    if require_text:
        text(value)
    return value


def canonical_bytes(manifest):
    return (json.dumps(manifest, ensure_ascii=True, sort_keys=True, indent=2) + '\n').encode('utf-8')


def narrow_edits(before, after, offset=0):
    """Trim in linear time, then match only a bounded changed Unicode region."""
    old, new = text(before), text(after)
    prefix = 0
    while prefix < min(len(old), len(new)) and old[prefix] == new[prefix]:
        prefix += 1
    suffix = 0
    while suffix < min(len(old), len(new)) - prefix and old[-suffix - 1] == new[-suffix - 1]:
        suffix += 1
    end_old, end_new = len(old) - suffix, len(new) - suffix
    a, b = old[prefix:end_old], new[prefix:end_new]
    start = offset + len(old[:prefix].encode('utf-8'))
    if not a or not b:
        return [] if a == b else [{'offset': start, 'remove': len(a.encode('utf-8')), 'insert': b}]
    require(max(len(a), len(b)) <= MAX_MATCH_CHARS,
            'changed hunk exceeds bounded matching limit; split the change before packaging')
    offsets = [start]
    for character in a:
        offsets.append(offsets[-1] + len(character.encode('utf-8')))
    edits = []
    for tag, i, j, k, l in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes():
        if tag != 'equal':
            edits.append({'offset': offsets[i], 'remove': offsets[j] - offsets[i], 'insert': b[k:l]})
    return edits


def line_offsets(data):
    offsets = [0]
    offsets.extend(i + 1 for i, value in enumerate(data) if value == 10)
    if offsets[-1] != len(data):
        offsets.append(len(data))
    return offsets


def changed_edits(repo, base, head, path, before, after):
    if before == after:
        return []
    patch = git(repo, 'diff', '--no-ext-diff', '--no-textconv', '--no-renames',
                '--no-color', '--text', '--unified=0', '--diff-algorithm=histogram',
                base, head, '--', ':(literal)' + path)
    old_lines, new_lines = line_offsets(before), line_offsets(after)
    edits = []
    for line in patch.split(b'\n'):
        match = HUNK.match(line)
        if match:
            a, count_a, b, count_b = [int(x) if x is not None else 1 for x in match.groups()]
            i, j = (a - 1 if count_a else a), (b - 1 if count_b else b)
            require(0 <= i <= i + count_a < len(old_lines), 'invalid Git hunk')
            require(0 <= j <= j + count_b < len(new_lines), 'invalid Git hunk')
            lo, hi = old_lines[i], old_lines[i + count_a]
            edits.extend(narrow_edits(before[lo:hi], after[new_lines[j]:new_lines[j + count_b]], lo))
    require(edits, 'no source edits found')
    require(apply_edits(before, edits) == after, 'Git hunk reconstruction mismatch')
    return edits


def descriptor(data, entry):
    return {'sha256': sha(data), 'mode': entry[0]}


def export_manifest(repo, base, head, allow_new=()):
    commit(repo, base)
    commit(repo, head)
    approved = set()
    for path in allow_new:
        safe_path(path)
        require(path not in approved, 'duplicate new-file allowlist path')
        approved.add(path)
    old_tree, new_tree = tree(repo, base), tree(repo, head)
    additions = new_tree.keys() - old_tree.keys()
    require(additions <= approved, 'new source files require an explicit exact allowlist')
    require(approved <= additions, 'new-file allowlist contains a path that is not newly added')
    files = []
    for path in sorted(old_tree.keys() | new_tree.keys()):
        old_entry, new_entry = old_tree.get(path), new_tree.get(path)
        if old_entry == new_entry:
            continue
        before = blob(repo, old_entry) if old_entry else b''
        after = blob(repo, new_entry) if new_entry else b''
        if old_entry and new_entry:
            edits = changed_edits(repo, base, head, path, before, after)
        else:
            edits = narrow_edits(before, after)
        files.append({'path': path, 'before': descriptor(before, old_entry) if old_entry else None,
                      'after': descriptor(after, new_entry) if new_entry else None, 'edits': edits})
    result = {'format': FORMAT, 'version': VERSION, 'base': base, 'head': head, 'files': files}
    require(len(canonical_bytes(result)) <= MAX_MANIFEST_BYTES, 'manifest size limit exceeded')
    return result


def keys(value, expected):
    require(isinstance(value, dict) and set(value) == set(expected), 'invalid manifest schema')


def validate_descriptor(value):
    if value is None:
        return
    keys(value, ('sha256', 'mode'))
    require(isinstance(value['sha256'], str) and SHA256.fullmatch(value['sha256']), 'invalid file hash')
    require(isinstance(value['mode'], str) and value['mode'] in MODES, 'unsupported special file mode')


def apply_edits(before, edits):
    require(isinstance(edits, list), 'invalid edit list')
    cursor, previous_offset = 0, -1
    chunks = []
    for edit in edits:
        keys(edit, ('offset', 'remove', 'insert'))
        offset, remove, insert = edit['offset'], edit['remove'], edit['insert']
        require(type(offset) is int and type(remove) is int and isinstance(insert, str), 'invalid edit types')
        require(0 <= cursor <= offset <= offset + remove <= len(before) and offset > previous_offset,
                'overlapping or out-of-bounds edit')
        require(remove or insert, 'empty edit refused')
        try:
            added = insert.encode('utf-8', errors='strict')
        except UnicodeError as exc:
            raise PackagerError('invalid inserted text encoding') from exc
        text(added)
        chunks.extend((before[cursor:offset], added))
        cursor, previous_offset = offset + remove, offset
    chunks.append(before[cursor:])
    result = b''.join(chunks)
    require(len(result) <= MAX_BLOB_BYTES, 'result blob size limit exceeded')
    text(result)
    return result


def validate_manifest(manifest, expected_base):
    keys(manifest, ('format', 'version', 'base', 'head', 'files'))
    require(manifest['format'] == FORMAT, 'manifest format mismatch')
    require(type(manifest['version']) is int and manifest['version'] == VERSION, 'manifest version mismatch')
    require(isinstance(expected_base, str) and SHA1.fullmatch(expected_base), 'invalid expected base')
    require(manifest['base'] == expected_base, 'manifest base mismatch')
    require(isinstance(manifest['head'], str) and SHA1.fullmatch(manifest['head']), 'invalid head commit identity')
    require(isinstance(manifest['files'], list), 'invalid files schema')
    seen = set()
    for entry in manifest['files']:
        keys(entry, ('path', 'before', 'after', 'edits'))
        safe_path(entry['path'])
        require(entry['path'] not in seen, 'duplicate manifest path')
        seen.add(entry['path'])
        validate_descriptor(entry['before'])
        validate_descriptor(entry['after'])
        require(entry['before'] is not None or entry['after'] is not None, 'empty file operation')
        require(isinstance(entry['edits'], list), 'invalid edit list')


def reconstruct_entry(repo, old_tree, entry):
    original = old_tree.get(entry['path'])
    require((original is None) == (entry['before'] is None), 'base file presence mismatch')
    before = blob(repo, original) if original else b''
    if original:
        require(descriptor(before, original) == entry['before'], 'base file hash or mode mismatch')
    after = apply_edits(before, entry['edits'])
    if entry['after'] is None:
        require(after == b'', 'deleted file still has final content')
    else:
        require(sha(after) == entry['after']['sha256'], 'final file hash mismatch')
    return after


def load_manifest(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, 'duplicate JSON key')
            result[key] = value
        return result
    with open(path, 'rb') as stream:
        data = stream.read(MAX_MANIFEST_BYTES + 1)
    require(len(data) <= MAX_MANIFEST_BYTES, 'manifest size limit exceeded')
    try:
        return json.loads(data, object_pairs_hook=unique)
    except (UnicodeError, json.JSONDecodeError, RecursionError) as exc:
        raise PackagerError('invalid JSON manifest') from exc


def open_parent(output):
    # Walk with O_NOFOLLOW: no symlink in any existing parent component.
    absolute = os.path.abspath(os.fspath(output))
    require(os.fspath(output) == absolute, 'output must be an explicit absolute path')
    parts = Path(absolute).parts
    require(len(parts) > 1 and parts[-1] not in ('.', '..'), 'invalid output path')
    fd = os.open('/', os.O_RDONLY | os.O_DIRECTORY)
    try:
        for part in parts[1:-1]:
            try:
                next_fd = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=fd)
            except OSError as exc:
                raise PackagerError('output parent is missing, a symlink or not a directory') from exc
            os.close(fd)
            fd = next_fd
        return fd, parts[-1]
    except BaseException:
        os.close(fd)
        raise


def write_source(stage_fd, path, data, mode):
    parts = safe_path(path)
    fd = os.dup(stage_fd)
    try:
        for part in parts[:-1]:
            try:
                os.mkdir(part, 0o755, dir_fd=fd)
            except FileExistsError:
                pass
            child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=fd)
            os.close(fd)
            fd = child
        target = os.open(parts[-1], os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600, dir_fd=fd)
        with os.fdopen(target, 'wb') as stream:
            stream.write(data)
            stream.flush()
            os.fchmod(stream.fileno(), int(mode, 8) & 0o777)
    finally:
        os.close(fd)


def write_gitlink_directory(stage_fd, path):
    fd = os.dup(stage_fd)
    try:
        for part in safe_path(path):
            try:
                os.mkdir(part, 0o755, dir_fd=fd)
            except FileExistsError:
                pass
            child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=fd)
            os.close(fd)
            fd = child
    finally:
        os.close(fd)


def publish(parent_fd, stage_name, output_name):
    # Atomic no-replace publication is a Linux requirement, not check-then-rename.
    libc = ctypes.CDLL(None, use_errno=True)
    try:
        rename = libc.renameat2
    except AttributeError as exc:
        raise PackagerError('atomic no-replace publication requires Linux renameat2') from exc
    rename.argtypes = (ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint)
    rename.restype = ctypes.c_int
    if rename(parent_fd, os.fsencode(stage_name), parent_fd, os.fsencode(output_name), 1) != 0:
        code = ctypes.get_errno()
        raise PackagerError('output already exists or atomic publication failed: ' + os.strerror(code))


def apply_manifest(repo, manifest, output, expected_base=PINNED_BASE):
    validate_manifest(manifest, expected_base)
    commit(repo, expected_base)
    # The caller may pass a nested directory. Protect the actual checkout root,
    # and refuse bare/Git-metadata directories that have no worktree identity.
    source = Path(os.fsdecode(git(repo, 'rev-parse', '--show-toplevel').removesuffix(b'\n'))).resolve()
    require(not Path(output).resolve().is_relative_to(source), 'output cannot be inside the source checkout')
    old_tree = tree(repo, expected_base)
    changes = {entry['path']: entry for entry in manifest['files']}
    final_paths = set(old_tree)
    for path, entry in changes.items():
        if entry['after'] is None:
            final_paths.discard(path)
        else:
            final_paths.add(path)
    for path in final_paths:
        parts = safe_path(path)
        require(not any('/'.join(parts[:i]) in final_paths for i in range(1, len(parts))), 'file/directory path conflict')
    parent_fd, output_name = open_parent(output)
    stage_name = '.patch-stage-' + secrets.token_hex(16)
    stage_created = False
    stage_fd = None
    gitlinks = []
    try:
        try:
            os.stat(output_name, dir_fd=parent_fd, follow_symlinks=False)
        except FileNotFoundError:
            pass
        else:
            raise PackagerError('output already exists')
        os.mkdir(stage_name, 0o700, dir_fd=parent_fd)
        stage_created = True
        stage_fd = os.open(stage_name, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=parent_fd)
        for path in sorted(old_tree.keys() | changes.keys()):
            if path in changes:
                entry = changes[path]
                data = reconstruct_entry(repo, old_tree, entry)
                if entry['after'] is None:
                    continue
                mode = entry['after']['mode']
            else:
                original = old_tree[path]
                if original[0:2] == ('160000', 'commit'):
                    write_gitlink_directory(stage_fd, path)
                    gitlinks.append({'path': path, 'mode': '160000', 'commit': original[2]})
                    continue
                data, mode = blob(repo, original, require_text=False), original[0]
            write_source(stage_fd, path, data, mode)
        os.fchmod(stage_fd, 0o755)
        publish(parent_fd, stage_name, output_name)
        stage_created = False
        return {'format': 'source-reconstruction-report', 'version': VERSION,
                'base': expected_base, 'head': manifest['head'],
                'uninitialized_gitlinks': gitlinks}
    finally:
        if stage_fd is not None:
            os.close(stage_fd)
        if stage_created:
            shutil.rmtree(stage_name, dir_fd=parent_fd)
        os.close(parent_fd)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    export = commands.add_parser('export', help='write a review-required manifest to stdout')
    export.add_argument('--repo', required=True)
    export.add_argument('--base', default=PINNED_BASE)
    export.add_argument('--head', required=True)
    export.add_argument('--allow-new', action='append', default=[])
    apply = commands.add_parser('apply', help='reconstruct into a new absolute output directory')
    apply.add_argument('--repo', required=True)
    apply.add_argument('--base', default=PINNED_BASE)
    apply.add_argument('--manifest', required=True)
    apply.add_argument('--output', required=True)
    args = parser.parse_args(argv)
    try:
        if args.command == 'export':
            sys.stdout.buffer.write(canonical_bytes(export_manifest(args.repo, args.base, args.head, args.allow_new)))
        else:
            report = apply_manifest(args.repo, load_manifest(args.manifest), args.output, args.base)
            sys.stdout.buffer.write(canonical_bytes(report))
    except (PackagerError, OSError, RecursionError) as exc:
        print('refused: ' + str(exc), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
