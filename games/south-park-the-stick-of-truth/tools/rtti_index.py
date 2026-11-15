#!/usr/bin/env python3
"""Static MSVC RTTI and vtable index for South Park: The Stick of Truth (x86 PE).

Reads the on-disk image only. No process is started, so no launch lock is needed.
Recovers type descriptors (.?AV / .?AU / .?AT / .?AW4 names), x86 Complete Object
Locators (COL), class hierarchy descriptors with base class arrays, and vtables (the
word after the COL pointer at vtable[-1] is the first slot). A vtable is accepted only
if its first slot points into an executable section. .text bytes are never decoded.

Evidence label: every class is observed in RTTI only. Nothing here says anything about
runtime layout, field offsets or behaviour.

Usage, from the game's tools folder:
  py -3.14 -B rtti_index.py [--exe PATH] [--out PATH] [--pylibs PATH]
  py -3.14 -B rtti_index.py --self-test

Refuses to run unless the exe matches the pinned SHA-256 and size.
"""

import argparse
import hashlib
import json
import os
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent
EXPECTED_SHA256 = '54a83d055f97cad8beeb5aa027c96c0c2aa4a6c36394634270297dd47856a0d6'
EXPECTED_SIZE = 11468656
DEFAULT_EXE = Path(r'D:\coop-work\game-test\South Park - The Stick of Truth.exe')
DEFAULT_OUT = ROOT / 'rtti_index.json'
IMAGE_SCN_MEM_EXECUTE = 0x20000000
NAME_RE = re.compile(rb'\.\?A[UVTW][\x21-\x7e]{2,400}\x00')
KIND = {'V': 'class', 'U': 'struct', 'T': 'union', 'W': 'enum'}
FOCUS = (
    'Player_Object', 'Secondary_Player_Object', 'Remote_Camera_Object', 'Companion',
    'Creature', 'Actor', 'Game_State', 'Level_State', 'Component_Network',
    'Host_Object_Controller', 'Ghost_Object_Controller', 'Session_Func',
    'Client_Node', 'NetConnection',
)
BASIC_TYPES = {
    'X': 'void', 'D': 'char', 'C': 'signed char', 'E': 'unsigned char',
    'F': 'short', 'G': 'unsigned short', 'H': 'int', 'I': 'unsigned int',
    'J': 'long', 'K': 'unsigned long', 'M': 'float', 'N': 'double',
    '_J': '__int64', '_K': 'unsigned __int64', '_N': 'bool', '_W': 'wchar_t',
}
POINTER_CV = {'A': '', 'B': 'const ', 'C': 'volatile ', 'D': 'const volatile '}


def load_pefile(extra):
    candidates = []
    if extra:
        candidates.append(Path(extra))
    if os.environ.get('SP_PYLIBS'):
        candidates.append(Path(os.environ['SP_PYLIBS']))
    candidates.extend(parent / 'local' / 'pylibs' for parent in [ROOT, *ROOT.parents])
    for candidate in candidates:
        if (candidate / 'pefile.py').is_file():
            sys.path.insert(0, str(candidate))
            break
    import pefile
    return pefile


def _read_until(text, index, stop):
    end = text.index(stop, index)
    return text[index:end], end + 1


def _qualified(text, index):
    parts = []
    while True:
        if index >= len(text):
            raise ValueError('unterminated scope')
        if text[index] == '@':
            return list(reversed(parts)), index + 1
        if text.startswith('?$', index):
            name, index = _read_until(text, index + 2, '@')
            args, index = _template_args(text, index)
            parts.append('%s<%s>' % (name, ', '.join(args)))
        else:
            name, index = _read_until(text, index, '@')
            parts.append(name)


def _template_args(text, index):
    args = []
    while True:
        if index >= len(text):
            raise ValueError('unterminated template')
        if text[index] == '@':
            return args, index + 1
        arg, index = _type(text, index)
        args.append(arg)


def _type(text, index):
    pair = text[index:index + 2]
    if pair in BASIC_TYPES:
        return BASIC_TYPES[pair], index + 2
    code = text[index]
    if code in BASIC_TYPES:
        return BASIC_TYPES[code], index + 1
    if code in 'VUT':
        parts, index = _qualified(text, index + 1)
        return '::'.join(parts), index
    if text.startswith('W4', index):
        parts, index = _qualified(text, index + 2)
        return '::'.join(parts), index
    if code == 'P' and index + 1 < len(text) and text[index + 1] in POINTER_CV:
        cv = POINTER_CV[text[index + 1]]
        inner, index = _type(text, index + 2)
        return cv + inner + '*', index
    raise ValueError('unsupported type code %r' % code)


def demangle(mangled):
    """Readable name for an MSVC RTTI type name; the input comes back on any doubt."""
    if len(mangled) < 6 or not mangled.startswith('.?A'):
        return mangled
    index = 5 if mangled[3] == 'W' and mangled[4:5] == '4' else 4
    try:
        parts, end = _qualified(mangled, index)
    except (ValueError, IndexError):
        return mangled
    if end != len(mangled):
        return mangled
    return '::'.join(parts)


def base_name(name):
    return name.split('<', 1)[0].rsplit('::', 1)[-1]


def self_test():
    cases = {
        '.?AVPlayer_Object@@': 'Player_Object',
        '.?AVPlayer@Actor@@': 'Actor::Player',
        '.?AV?$Host_Object_Controller@VPlayer_Object@@@@': 'Host_Object_Controller<Player_Object>',
        '.?AU?$Ghost_Object_Controller@UPlayer_Object_Net_Data@@@@':
            'Ghost_Object_Controller<Player_Object_Net_Data>',
        '.?AV?$Foo@H@@': 'Foo<int>',
        '.?AV?$Pair@PAVA@@VB@@@@': 'Pair<A*, B>',
        '.?AW4Mode@@': 'Mode',
        '.?AVBroken': '.?AVBroken',
    }
    failures = [(m, demangle(m), want) for m, want in cases.items() if demangle(m) != want]
    for mangled, got, want in failures:
        print('self-test FAIL %s -> %r (want %r)' % (mangled, got, want))
    print('self-test: %d/%d demangler cases passed' % (len(cases) - len(failures), len(cases)))
    return 1 if failures else 0


class Section:
    def __init__(self, name, rva, end, executable):
        self.name = name
        self.rva = rva
        self.end = end
        self.executable = executable


class Image:
    """The file laid out by RVA, built only from the on-disk bytes."""

    def __init__(self, pe, raw):
        self.base = pe.OPTIONAL_HEADER.ImageBase
        self.machine = pe.FILE_HEADER.Machine
        size = pe.OPTIONAL_HEADER.SizeOfImage
        self.image = bytearray(size)
        self.sections = []
        for section in pe.sections:
            name = section.Name.rstrip(b'\0').decode('ascii', 'replace')
            start = section.VirtualAddress
            raw_len = max(0, min(section.SizeOfRawData, len(raw) - section.PointerToRawData))
            self.image[start:start + raw_len] = raw[section.PointerToRawData:section.PointerToRawData + raw_len]
            end = min(size, start + max(section.Misc_VirtualSize, section.SizeOfRawData))
            executable = bool(section.Characteristics & IMAGE_SCN_MEM_EXECUTE)
            self.sections.append(Section(name, start, end, executable))

    def u32(self, rva):
        if rva is None or rva < 0 or rva + 4 > len(self.image):
            return None
        return struct.unpack_from('<I', self.image, rva)[0]

    def s32(self, rva):
        value = self.u32(rva)
        if value is None:
            return None
        return value - (1 << 32) if value & 0x80000000 else value

    def section_at(self, rva):
        for section in self.sections:
            if section.rva <= rva < section.end:
                return section
        return None

    def is_data(self, rva):
        section = self.section_at(rva)
        return section is not None and not section.executable

    def is_code(self, rva):
        section = self.section_at(rva)
        return section is not None and section.executable

    def data_words(self):
        for section in self.sections:
            if section.executable:
                continue
            start = (section.rva + 3) & ~3
            end = section.end & ~3
            if end <= start:
                continue
            words = memoryview(self.image)[start:end].cast('I')
            for k, value in enumerate(words):
                yield start + 4 * k, value

    def find_pointers(self, targets):
        hits = defaultdict(list)
        for rva, value in self.data_words():
            if value in targets:
                hits[value].append(rva)
        return hits

    def find_type_descriptors(self):
        found = {}
        for section in self.sections:
            if section.executable:
                continue
            blob = bytes(self.image[section.rva:section.end])
            for match in NAME_RE.finditer(blob):
                name_rva = section.rva + match.start()
                td = name_rva - 8
                if td < 0 or not self.is_data(td):
                    continue
                if self.u32(td + 4) != 0 or not self.u32(td):
                    continue
                found[td] = match.group()[:-1].decode('ascii')
        return found

    def verify_col(self, col, td_rva):
        """Check a COL (x86 layout) and the class hierarchy and base array it points to."""
        if self.u32(col) != 0:
            return None
        chd_va = self.u32(col + 16)
        if chd_va is None:
            return None
        chd = chd_va - self.base
        if not self.is_data(chd) or self.u32(chd) != 0:
            return None
        attributes = self.u32(chd + 4)
        count = self.u32(chd + 8)
        if count is None or not 1 <= count <= 64:
            return None
        array = self.u32(chd + 12) - self.base
        if not self.is_data(array) or not self.is_data(array + 4 * count - 1):
            return None
        bcds = []
        for k in range(count):
            bcd = self.u32(array + 4 * k) - self.base
            if not self.is_data(bcd) or not self.is_data(bcd + 27):
                return None
            bcds.append(bcd)
        if self.u32(bcds[0]) != self.base + td_rva or self.u32(bcds[0] + 24) != chd_va:
            return None
        return {
            'td_rva': td_rva,
            'chd_rva': chd,
            'attributes': attributes,
            'num_bases': count,
            'this_offset': self.s32(col + 4),
            'cd_offset': self.s32(col + 8),
            'bcds': bcds,
        }


def read_bases(img, info, names):
    bases = []
    for bcd in info['bcds'][1:]:
        td = img.u32(bcd) - img.base
        bases.append({
            'name': names.get(td, '<unnamed td %s>' % hex(td)),
            'mdisp': img.s32(bcd + 8),
            'pdisp': img.s32(bcd + 12),
            'vdisp': img.s32(bcd + 16),
            'attributes': img.u32(bcd + 20),
        })
    return bases


def build_index(img):
    counts = defaultdict(int)
    counts['duplicate_demangled_names'] = 0
    counts['base_list_mismatch_between_cols'] = 0
    counts['vtable_rejected_first_slot_not_code'] = 0
    tds = img.find_type_descriptors()
    counts['type_descriptors'] = len(tds)
    names = {td: demangle(mangled) for td, mangled in tds.items()}
    counts['undemangled_names'] = sum(1 for td in tds if names[td] == tds[td])

    refs = img.find_pointers({img.base + td for td in tds})
    cols = {}
    for td_va, positions in refs.items():
        td_rva = td_va - img.base
        for pointer in positions:
            counts['col_candidates'] += 1
            info = img.verify_col(pointer - 12, td_rva)
            if info is not None:
                cols[pointer - 12] = info
    counts['cols_verified'] = len(cols)

    col_refs = img.find_pointers({img.base + col for col in cols})
    vtables = defaultdict(list)
    for col_va, positions in col_refs.items():
        for pointer in positions:
            vt = pointer + 4
            first = img.u32(vt)
            if first is None or not img.is_data(vt):
                continue
            if not img.is_code(first - img.base):
                counts['vtable_rejected_first_slot_not_code'] += 1
                continue
            vtables[col_va - img.base].append(vt)
    counts['vtables'] = sum(len(v) for v in vtables.values())

    cols_by_td = defaultdict(list)
    for col, info in cols.items():
        cols_by_td[info['td_rva']].append(col)

    records = {}
    for td, mangled in tds.items():
        vt_list = []
        base_lists = []
        for col in sorted(cols_by_td.get(td, [])):
            info = cols[col]
            bases = read_bases(img, info, names)
            base_lists.append(bases)
            for vt in sorted(vtables.get(col, [])):
                vt_list.append({
                    'rva': hex(vt),
                    'col_rva': hex(col),
                    'this_offset': info['this_offset'],
                    'cd_offset': info['cd_offset'],
                })
        if base_lists and any(b != base_lists[0] for b in base_lists):
            counts['base_list_mismatch_between_cols'] += 1
        if vt_list:
            evidence = 'rtti-observed'
        elif cols_by_td.get(td):
            evidence = 'rtti-col-without-vtable'
        else:
            evidence = 'rtti-type-descriptor-only'
        records[td] = {
            'mangled': mangled,
            'kind': KIND.get(mangled[3], 'unknown'),
            'type_descriptor_rva': hex(td),
            'evidence': evidence,
            'vtable_rvas': [v['rva'] for v in vt_list],
            'vtables': vt_list,
            'col_rvas': [hex(c) for c in sorted(cols_by_td.get(td, []))],
            'chd_rva': hex(cols[sorted(cols_by_td[td])[0]]['chd_rva']) if cols_by_td.get(td) else None,
            'base_classes': base_lists[0] if base_lists else [],
        }

    display = defaultdict(list)
    for td, record in records.items():
        display[names[td]].append(td)
    classes = {}
    for name in sorted(display):
        for td in display[name]:
            key = name if len(display[name]) == 1 else records[td]['mangled']
            if len(display[name]) > 1:
                counts['duplicate_demangled_names'] += 1
            classes[key] = records[td]
    counts['classes'] = len(classes)
    counts['classes_with_vtable'] = sum(1 for r in classes.values() if r['vtables'])
    counts['classes_col_without_vtable'] = sum(1 for r in classes.values() if r['evidence'] == 'rtti-col-without-vtable')
    counts['classes_type_descriptor_only'] = sum(1 for r in classes.values() if r['evidence'] == 'rtti-type-descriptor-only')
    return classes, dict(counts)


def focus_report(classes):
    report = {}
    for focus in FOCUS:
        matches = sorted(name for name in classes if base_name(name) == focus)
        entry = {
            'found': bool(matches),
            'matches': matches,
            'vtable_rvas': {name: classes[name]['vtable_rvas'] for name in matches},
        }
        if not matches:
            token = focus.split('_')[0].lower()
            entry['near_matches'] = sorted(n for n in classes if token in n.lower())[:30]
        report[focus] = entry
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description='Static MSVC RTTI and vtable index (no launch).')
    parser.add_argument('--exe', type=Path, default=DEFAULT_EXE)
    parser.add_argument('--out', type=Path, default=DEFAULT_OUT)
    parser.add_argument('--pylibs', help='folder that holds pefile.py (or set SP_PYLIBS)')
    parser.add_argument('--self-test', action='store_true', help='run the demangler checks and exit')
    args = parser.parse_args(argv)
    if args.self_test:
        return self_test()

    raw = args.exe.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest != EXPECTED_SHA256 or len(raw) != EXPECTED_SIZE:
        print('refusing: exe does not match the pinned test copy (sha256 %s, size %d)' % (digest, len(raw)),
              file=sys.stderr)
        return 2

    pefile = load_pefile(args.pylibs)
    pe = pefile.PE(data=raw, fast_load=True)
    if pe.FILE_HEADER.Machine != 0x14C:
        print('refusing: not an i386 image', file=sys.stderr)
        return 2
    img = Image(pe, raw)
    classes, counts = build_index(img)
    focus = focus_report(classes)

    out = {
        'tool': 'games/south-park-the-stick-of-truth/tools/rtti_index.py',
        'evidence': 'static, on-disk image only. Every class is observed in MSVC RTTI. '
                    'No runtime layout, field offset or behaviour is implied.',
        'exe': {
            'file_name': args.exe.name,
            'size': len(raw),
            'sha256': digest,
            'image_base': hex(img.base),
            'machine': 'i386 (0x14c)',
            'sections': [{'name': s.name, 'rva': hex(s.rva), 'end': hex(s.end), 'executable': s.executable}
                         for s in img.sections],
        },
        'counts': counts,
        'focus': focus,
        'classes': classes,
    }
    args.out.write_text(json.dumps(out, indent=2) + '\n', encoding='utf-8')

    print('exe sha256 ok, image base %s' % hex(img.base))
    print('counts: %s' % json.dumps(counts, sort_keys=True))
    for name in FOCUS:
        entry = focus[name]
        if entry['found']:
            print('focus %-28s %s' % (name, '; '.join('%s %s' % (m, ','.join(classes[m]['vtable_rvas']) or '(no vtable)')
                                                     for m in entry['matches'])))
        else:
            print('focus %-28s not found' % name)
    print('wrote %s' % args.out)
    return 0


if __name__ == '__main__':
    sys.exit(main())
