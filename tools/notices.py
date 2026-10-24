#!/usr/bin/env python3
"""Writes kairo64/src/main/assets/THIRD_PARTY_NOTICES.txt from the license files and notices in
third_party/: VICE, the code it carries under other terms, the MEGA65 Open ROMs and zlib. The
shared frontend's own notices (its Java libraries, font, NDK runtime and AndroidX) follow these in
About -> Licenses. Run it again after an import.

Usage: notices.py [--check]   (--check fails when the file is out of date)
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
THIRD_PARTY = ROOT / 'third_party'
OUT = ROOT / 'kairo64/src/main/assets/THIRD_PARTY_NOTICES.txt'
RULE = '=' * 72

HEADER = '''Kairo64 third-party notices
These notices apply to the emulator code, system software and libraries in this package. Kairo64
emulates the Commodore 64 with VICE 3.10 and starts games with the MEGA65 Open ROMs; Kairo64 is not
affiliated with the VICE team, the MEGA65 project or Commodore.'''

VICE_TERMS = '''VICE, the Versatile Commodore Emulator, including its reSID sound chip emulation by Dag Lem, is
distributed under the GNU General Public License, version 2 or (at your option) any later version.
Its team and contributors are listed in README and doc/vice.texi of its source. A few of its files
carry other terms, given below. The GPL version 2 text follows; the GPL version 3 text, which
reSID's own COPYING holds and which applies to the files under version 3, follows it.'''

ZMBV_TERMS = '''The ZMBV video codec in src/lib/libzmbv (zmbv.c, zmbv_avi.c), copyright (C) 2002-2013 the DOSBox
Team, is distributed under the GNU General Public License, version 3 or (at your option) any later
version; the GPL version 3 text is above.'''

FMOPL_TERMS = '''src/core/fmopl.c, the OPL FM sound generator by Jarek Burczynski and Tatsuyuki Satoh from MAME,
adapted for VICE by Marco van den Heuvel, is distributed under the GNU General Public License,
version 2 or later (license:GPL-2.0+); the GPL text is above.'''

MD5_TERMS = '''src/lib/md5 is derived from the RSA Data Security, Inc. MD5 Message-Digest Algorithm.'''

OPEN_ROMS_TERMS = '''The Open ROMs run inside the emulated C64 as its system ROMs. Their source is in
third_party/open-roms of Kairo64's source. Their license statement, which covers the MIT-licensed
BASIC files from Microsoft too, and the GNU LGPL version 3 text follow; the LGPL builds on the GNU
GPL version 3, whose text is above.'''


def text(path):
    return (THIRD_PARTY / path).read_text(encoding='utf-8', errors='strict').replace('\r\n', '\n').strip('\n')


def excerpt(path, first, last):
    """The lines of a source file from the one containing `first` through the next containing `last`."""
    lines = text(path).split('\n')
    start = next(i for i, line in enumerate(lines) if first in line)
    end = next(i for i in range(start, len(lines)) if last in lines[i])
    return '\n'.join(lines[start:end + 1])


# (title, source, [text]) in the order shown.
SECTIONS = [
    ('VICE 3.10', 'https://vice-emu.sourceforge.io/, third_party/vice',
     [VICE_TERMS, text('vice/COPYING'), text('vice/src/resid/COPYING')]),
    ('VICE: the ZMBV codec from DOSBox (GPL-3.0-or-later)', 'third_party/vice/src/lib/libzmbv', [ZMBV_TERMS]),
    ('VICE: fmopl.c from MAME (GPL-2.0-or-later)', 'third_party/vice/src/core/fmopl.c', [FMOPL_TERMS]),
    ('VICE: P64 reference implementation by Benjamin Rosseaux (zlib license)', 'third_party/vice/src/lib/p64/p64.c',
     [excerpt('vice/src/lib/p64/p64.c', 'Copyright (c) 2011-2012, Benjamin Rosseaux', '3. This notice may not be removed')]),
    ('VICE: tapecart loader by Ingo Korb (BSD license)', 'third_party/vice/src/tapeport/tapecart-loader.h',
     [excerpt('vice/src/tapeport/tapecart-loader.h', 'Copyright (C) 2013-2017  Ingo Korb', 'SUCH DAMAGE.')]),
    ('VICE: miniz by Rich Geldreich (public domain)', 'third_party/vice/src/lib/libzmbv/miniz.c',
     [excerpt('vice/src/lib/libzmbv/miniz.c', 'This is free and unencumbered software released into the public domain.',
              'For more information, please refer to')]),
    ('VICE: SHA-1 by Steve Reid (public domain)', 'third_party/vice/src/sha1.c',
     [excerpt('vice/src/sha1.c', 'SHA-1 in C', '100% Public Domain')]),
    ('VICE: MD5', 'third_party/vice/src/lib/md5', [MD5_TERMS]),
    ('MEGA65 Open ROMs', 'https://github.com/MEGA65/open-roms, third_party/open-roms',
     [OPEN_ROMS_TERMS, text('open-roms/LICENSE'), text('open-roms/COPYING.LESSER')]),
    ('zlib 1.3.2', 'https://zlib.net/, third_party/zlib', [text('zlib/LICENSE')]),
]


def section(title, source, texts):
    return '\n\n'.join([f'{RULE}\n{title}\nSource: {source}\n{RULE}'] + texts)


def main():
    content = HEADER + '\n\n' + '\n\n'.join(section(*s) for s in SECTIONS) + '\n'
    if sys.argv[1:] == ['--check']:
        # Git may check the file out with CRLF line endings.
        if OUT.read_text(encoding='utf-8').replace('\r\n', '\n') != content:
            raise SystemExit(f'{OUT.relative_to(ROOT)} is out of date: run tools/notices.py')
        return
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(content, encoding='utf-8', newline='\n')
    print(f'wrote {OUT.relative_to(ROOT)}')


if __name__ == '__main__':
    main()
