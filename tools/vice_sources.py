#!/usr/bin/env python3
"""Writes core/vice_sources.cmake: the sources of VICE's x64sc (third_party/vice), library by
library, as VICE's own Makefile.am files list them for Kairo64's build.

Usage: vice_sources.py [--check]

VICE builds with automake: x64sc links its base sources and a list of static libraries, each
listed in its directory's Makefile.am. This evaluates those Makefile.am files with the
conditionals and configure substitutions of a headless build without host devices (CONDITIONS
and SUBSTITUTIONS below), so the source lists come from VICE, not from a hand-kept copy.
--check fails when the committed file differs.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'third_party/vice/src'
OUT = ROOT / 'core/vice_sources.cmake'

# Automake conditionals: a headless build (USE_HEADLESSUI) with reSID, no hardware SID, MIDI,
# network or real devices, and not built for a desktop platform's own drivers.
CONDITIONS = {
    'USE_HEADLESSUI': True, 'HAVE_RESID': True, 'NEW_8580_FILTER': False,
    'RESID_DIR_USED': False, 'SUPPORT_X64': False,
    'USE_GTK3UI': False, 'USE_SDLUI': False, 'USE_SDL2UI': False, 'DUMMY_COMPILE': False,
    'WINDOWS_COMPILE': False, 'MACOS_COMPILE': False, 'UNIX_COMPILE': False, 'LINUX_COMPILE': False,
    'BEOS_COMPILE': False, 'HAVE_DEBUG': False, 'HAVE_USBSID': False, 'HAVE_PARSID': False,
    'HAVE_REALDEVICE': False, 'HAVE_CATWEASELMKIII': False,
    'USE_SVN_REVISION': False, 'USE_SVN_REVISION_OVERRIDE': False,
}

# Configure substitutions in the lists. The UI is the headless arch library; reSID is linked;
# no extra sound, hardware SID or other drivers are.
SUBSTITUTIONS = {
    'ARCH_LIBS': '$(top_builddir)/src/arch/headless/libarch.a',
    'RESID_LIBS': '$(top_builddir)/src/resid/libresid.a',
    'LIBOBJS': '', 'SOUND_DRIVERS': '', 'HWSIDDRIVERS': '',
    # System libraries: none (zlib comes from third_party/zlib, linked by core/CMakeLists.txt).
    'TFE_LIBS': '', 'NETPLAY_LIBS': '', 'UI_LIBS': '', 'SDL_EXTRA_LIBS': '', 'SOUND_LIBS': '',
    'GFXOUTPUT_DRIVERS': '', 'JOY_LIBS': '', 'GFXOUTPUT_LIBS': '', 'ZLIB_LIBS': '', 'DYNLIB_LIBS': '',
    # Preprocessor flags, as configure sets them for a headless build (configure.ac): the
    # headless arch's headers, the shared ones and reSID's.
    'VICE_CPPFLAGS': '-I$(top_srcdir)/src/arch/systemheaderoverride',
    'ARCH_INCLUDES': '-I$(top_srcdir)/src/arch/headless -I$(top_srcdir)/src/arch/shared -I$(top_srcdir)/src/arch',
    'RESID_INCLUDES': '-I$(top_builddir)/src/resid', 'RESID_DTV_INCLUDES': '', 'RESID_CPPFLAGS': '',
}

PROGRAM = 'x64sc'
SOURCE_SUFFIXES = ('.c', '.cc', '.cpp')
# Grammar sources whose generated C VICE's archive ships beside them.
GENERATED = {'.y': '.c', '.l': '.c'}


class Makefile:
    """One Makefile.am, its variables evaluated under CONDITIONS."""

    def __init__(self, path):
        self.path = path
        self.directory = path.parent
        self.variables = {}
        text = path.read_text(encoding='latin-1').replace('\\\n', ' ')
        active = []
        for line in text.splitlines():
            stripped = line.strip()
            if line.startswith('\t') or not stripped or stripped.startswith('#'):
                continue
            words = stripped.split()
            if words[0] == 'if':
                name = words[1]
                negate = name.startswith('!')
                name = name.lstrip('!')
                if name not in CONDITIONS:
                    raise SystemExit(f'{path}: conditional {name} has no value in CONDITIONS')
                active.append(CONDITIONS[name] != negate)
                continue
            if words[0] == 'else':
                active[-1] = not active[-1]
                continue
            if words[0] == 'endif':
                active.pop()
                continue
            if not all(active):
                continue
            match = re.match(r'([A-Za-z0-9_]+)\s*(\+?=)\s*(.*)$', stripped)
            if not match:
                continue
            name, operator, value = match.groups()
            if operator == '+=':
                self.variables[name] = self.variables.get(name, '') + ' ' + value
            else:
                self.variables[name] = value
        if active:
            raise SystemExit(f'{path}: unbalanced if/endif')

    def expand(self, value, seen=()):
        def variable(match):
            name = match.group(1)
            if name in seen:
                raise SystemExit(f'{self.path}: {name} refers to itself')
            if name in ('top_builddir', 'top_srcdir'):
                return '$(' + name + ')'
            return self.expand(self.variables.get(name, ''), seen + (name,))

        def substitution(match):
            name = match.group(1)
            if name not in SUBSTITUTIONS:
                raise SystemExit(f'{self.path}: @{name}@ has no value in SUBSTITUTIONS')
            return SUBSTITUTIONS[name]

        value = re.sub(r'@([A-Z0-9_]+)@', substitution, value)
        return re.sub(r'\$\((?!top_)([A-Za-z0-9_]+)\)', variable, value)

    def words(self, name):
        return self.expand(self.variables.get(name, '')).split()

    def library_sources(self, library):
        """The compiled sources of lib<library>.a: its _SOURCES, and the objects its _LIBADD
        names, as source files of this directory."""
        canonical = re.sub(r'[^A-Za-z0-9]', '_', library)
        sources = []
        for word in self.words(canonical + '_SOURCES'):
            suffix = Path(word).suffix
            if suffix in GENERATED:
                word = word[:-len(suffix)] + GENERATED[suffix]
            elif suffix not in SOURCE_SUFFIXES:
                continue
            sources.append(self.source(word))
        for word in self.words(canonical + '_LIBADD'):
            if not word.endswith('.o'):
                raise SystemExit(f'{self.path}: {library} adds {word}, not an object')
            stem = word[:-2]
            found = [self.directory / (stem + suffix) for suffix in SOURCE_SUFFIXES
                     if (self.directory / (stem + suffix)).is_file()]
            if len(found) != 1:
                raise SystemExit(f'{self.path}: {word} is not one source file of this directory')
            sources.append(found[0])
        return sources

    def includes(self):
        """The include directories, as automake compiles them: the Makefile's own directory
        (automake's DEFAULT_INCLUDES), then those AM_CPPFLAGS names, in its order. The build
        directory is the source directory: Kairo64 generates no headers into VICE's tree
        (config.h and the others are core/CMakeLists.txt's, on every library's include path)."""
        directories = [self.directory]
        for word in self.words('AM_CPPFLAGS'):
            if word.startswith('-D'):
                continue
            if not word.startswith('-I'):
                raise SystemExit(f'{self.path}: AM_CPPFLAGS has {word}, not an include directory')
            match = re.fullmatch(r'-I\$\((?:top_srcdir|top_builddir)\)/src(/.*)?', word)
            if not match:
                raise SystemExit(f'{self.path}: {word} is not a directory of VICE\'s source')
            directory = SRC / (match.group(1) or '/').lstrip('/')
            if directory not in directories:
                directories.append(directory)
        return directories

    def defines(self):
        """The names of the macros AM_CPPFLAGS defines (install paths, such as VICE_DATADIR),
        which core/CMakeLists.txt gives Kairo64's values."""
        return [re.match(r'-D([A-Za-z0-9_]+)', word).group(1)
                for word in self.words('AM_CPPFLAGS') if word.startswith('-D')]

    def source(self, name):
        path = self.directory / name
        if not path.is_file():
            raise SystemExit(f'{self.path}: {name} is not in the snapshot')
        return path


def library_path(word):
    """A library word, $(top_builddir)/src/<dir>/lib<name>.a, as (directory, name)."""
    match = re.fullmatch(r'\$\(top_builddir\)/src/(.+)/lib([^/]+)\.a', word)
    if not match:
        raise SystemExit(f'{word} is not a VICE library')
    return SRC / match.group(1), match.group(2)


def generate():
    top = Makefile(SRC / 'Makefile.am')
    makefiles = {}
    libraries = []
    for word in top.words(PROGRAM + '_LDADD'):
        if not word.endswith('.a'):
            continue
        directory, name = library_path(word)
        key = (directory, name)
        if key in [(d, n) for d, n, _ in libraries]:
            continue
        if directory not in makefiles:
            makefiles[directory] = Makefile(directory / 'Makefile.am')
        libraries.append((directory, name, makefiles[directory].library_sources('lib' + name + '.a')))
    base = [top.source(word) for word in top.words(PROGRAM + '_SOURCES')]

    def relative(path):
        return '${VICE}/' + path.relative_to(SRC).as_posix()

    lines = ['# Generated by tools/vice_sources.py from VICE\'s Makefile.am files; do not edit.',
             f'# The sources of {PROGRAM}: its own, then its libraries in its link order.',
             '', 'set(VICE_BASE_SOURCES']
    lines += [f'    {relative(path)}' for path in base]
    lines += [')', '', 'set(VICE_BASE_INCLUDES']
    lines += [f'    {relative(path)}' for path in top.includes()]
    lines += [')', '', 'set(VICE_LIBRARIES']
    lines += [f'    {name}' for _, name, _ in libraries]
    lines.append(')')
    for directory, name, sources in libraries:
        lines += ['', f'set(VICE_LIBRARY_{name}']
        lines += [f'    {relative(path)}' for path in sources]
        lines += [')', f'set(VICE_INCLUDES_{name}']
        lines += [f'    {relative(path)}' for path in makefiles[directory].includes()]
        lines.append(')')
        defines = makefiles[directory].defines()
        if defines:
            lines.append(f'set(VICE_DEFINES_{name} {" ".join(defines)})')
    return '\n'.join(lines) + '\n'


def main():
    text = generate()
    if '--check' in sys.argv[1:]:
        if not OUT.is_file() or OUT.read_text(encoding='utf-8') != text:
            print(f'{OUT.relative_to(ROOT)} is out of date: run tools/vice_sources.py', file=sys.stderr)
            return 1
        return 0
    OUT.write_text(text, encoding='utf-8', newline='\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())
