# Original source edit packager

A small Python standard-library utility for reviewing original source insertions
against an exact Git base, then reconstructing a **new private source tree**.
Requires Python 3.11+, Git and Linux with `renameat2(RENAME_NOREPLACE)` support.
No dependencies are downloaded. It does not build or run the source project.

Generated insertions require human review for authorship, source origin and rights
before distribution. An exact new-file allowlist is a mechanical gate, not an
approval of its contents. This utility cannot certify rights or identify copied
game code. Never distribute reconstructed source, private scratch, ROMs, assets,
saves, states, binaries, logs or personal paths with the original addon helpers.

## Usage

Use exact, lowercase 40-character commit object IDs. Symbolic refs and abbreviations
are refused. The default base is
`533860de269e0e9b338e12f3484ce8497367e6ed`; an explicit `--base` supports toy tests
or a deliberately reviewed different upstream pin. The head must already exist
locally; export reads Git objects, never the worker's current file contents.

```sh
python3 packager.py export --repo "$SOURCE_REPOSITORY" \
  --head "$EXACT_HEAD_COMMIT" \
  --allow-new src/original_addon.c > private-review-manifest.json
```

Repeat `--allow-new` for each new original file. Paths are exact, relative names;
no glob expansion or automatic untracked-file discovery is performed. Unused or
duplicate allowlist entries are errors. Review every insertion and new file.

```sh
python3 packager.py apply --repo "$SOURCE_REPOSITORY" \
  --manifest private-review-manifest.json \
  --output "$NEW_ABSOLUTE_OUTPUT_DIRECTORY" > private-reconstruction-report.json
```

The output must not exist, its parent must already exist, and it cannot be inside
the source checkout. Every parent component is opened without following symlinks.
The source repository must identify a worktree; bare repositories and `.git`
metadata directories are not accepted for application. No checkout, clone,
submodule initialization, build, hook, filter, textconv, listener or account is used.

Application copies unchanged upstream blobs from the pinned base into the private
output, including unchanged binary files. Their bytes never enter the manifest.
Unchanged base gitlinks become empty, uninitialized directories. The JSON report
on stdout records their path, mode and exact commit; no submodule content is read
or fetched. This is the state of a source checkout before submodule initialization.
New, modified or deleted binary files and gitlinks cannot be exported. Symlinks
and other unsupported special files are refused, including in an unchanged base.

## Manifest version 1

UTF-8 JSON, sorted object keys, two-space indentation, ASCII-escaped strings and
one final newline produce repeatable bytes with no timestamps or local paths.
Top-level fields are exactly `format`, `version`, `base`, `head`, `files`:

- `format` is `original-source-edits`; `version` is integer `1`.
- `base` and `head` identify immutable Git commits. `head` is provenance; apply
  needs only the base Git objects, not a locally available private head commit.
- `files` is sorted by relative path. Each entry has exactly `path`, `before`,
  `after`, `edits`. A missing file is represented by a null descriptor.
- A file descriptor has `sha256` for its exact bytes and `mode` of `100644` or
  `100755`. Renames are represented as a deletion and an allowlisted new file.
- Each edit has `offset`, `remove`, `insert`. Offsets and removal lengths count
  **bytes in the original base file**, not positions after preceding edits.
  `insert` contains only the replacement UTF-8 text. Removed bytes, old lines,
  surrounding lines and function-context headers are never serialized.
- Edits are ordered, nonempty, nonoverlapping and have strictly increasing
  offsets. They must stay in bounds, produce valid text and match the final hash.
  A deletion must reconstruct empty content. Empty new files have no edits.

Git's histogram line diff with zero context selects changed hunks. Linear common
prefix/suffix trimming removes surrounding unchanged text from each hunk. Only
the remaining changed Unicode region is passed to `SequenceMatcher`, capped at
8,192 characters on either side when both sides are nonempty. Pure insertions and
deletions need no matcher. A larger replacement fails explicitly and must be split
before export; it never silently falls back to copying a large old block. This
bounds Python's quadratic matcher rather than applying it to a generated file.
It does not promise the globally shortest edit script. Blob and manifest sizes
are capped at 64 MiB. Export still reads Git's line-diff output into memory.

Unknown schema keys, duplicate JSON keys or paths, traversal, `.git` components,
backslashes, colons, control characters, invalid UTF-8, unsupported modes, bad
hashes, overlapping edits and version/base mismatches are errors. Subprocesses
use argument arrays, literal pathspecs and no shell. Raw object reads use no Git
replacement objects; external diffs and text conversion are disabled.

Application verifies changed files' base hashes and modes, validates each edit,
checks resulting hashes, and preserves regular-file executable permissions.
All bytes are written beneath a private sibling staging directory through open
directory descriptors. Linux's atomic no-replace rename publishes only after
successful reconstruction. Failed validation/publication removes owned staging;
existing outputs, including an output created concurrently, are preserved.
Directory deletion/file replacement is supported when its final path set is
valid. Directory permissions are 0755; source-file modes are 0644 or 0755.

Hashes detect inconsistency and accidental/tampered content relative to the
reviewed manifest. There is no signature: someone who replaces both inserted
content and its declared hash has created a different manifest requiring review.
`head` alone is not an authenticity check or a claim that all head changes were
included in an externally edited manifest. The utility does not defend against
a compromised Git executable, modified object database or hostile same-user
concurrent filesystem changes, and does not guarantee crash/power-loss recovery.

## Behavioral verification

The tests create small original toy Git repositories. No private native source or
game data is needed.

```sh
python3 -m unittest discover -s tests -p 'test*.py' -v
python3 tests/negative_controls.py
PACKAGER_BASELINE=1 python3 tests/test_packager.py \
  PackagingTests.test_reconstructs_immutable_head_and_preserves_mode_without_worktree_writes -v
```

The first two commands must succeed. The last command is the preserved runnable
missing-feature baseline and must return 1 with an assertion failure. Negative
controls first compile valid mutated copies, then require behavioral failures
when insertions, final-hash checks, new-file allowlisting, staging cleanup or atomic
no-replace publication are disabled. They do not use syntax errors as evidence.

This directory contains only the original utility, documentation and three
original Python test files. Actual source-change manifests require separate
content-origin review before publication.
**Exclude the entire `scratch/` directory and all `__pycache__/` directories.**
Private immutable-object checks, snapshots and their insertion manifests stay
private. These checks make no playable-package, native-game, campaign, transport
or Internet QA claim. Full playable Task5 remains open.
