# Licensing and credits

[← Kairo64](../README.md)

Kairo64 first-party code is [GPL-2.0-or-later](../LICENSE.md). The pinned [Kairo frontend](https://github.com/MrJackSpade/Kairo) is also GPL-2.0-or-later. Third-party components keep their own notices.

## VICE

The Commodore 64 emulation comes from [VICE](https://vice-emu.sourceforge.io/) 3.10, the Versatile Commodore Emulator: its cycle-exact C64 (`x64sc`), the 1541 drive and the reSID sound chip emulation. Its source headers license it under the GNU General Public License, version 2 or (at your option) any later version; the GPL version 2 text is in `third_party/vice/COPYING`, and its team and contributors are listed in `third_party/vice/README` and `doc/vice.texi`.

Kairo64 checked every C and C++ file in the snapshot for its license notice. The only files whose notice names GPL version 2 without "or later" are VICE's USB SID hardware driver (`src/lib/libusbsiddrv`), which a phone has no use for; Kairo64 leaves it out of its snapshot (see [source import](source-import.md)). The others whose notice is not VICE's own:

| Files | License |
| --- | --- |
| `src/resid/` (reSID by Dag Lem) | GPL-2.0-or-later (its `COPYING` holds the GPL version 3 text) |
| `src/lib/libzmbv/zmbv*.c` (from DOSBox) | GPL-3.0-or-later |
| `src/lib/libzmbv/miniz.c` | Public domain (Unlicense) |
| `src/core/fmopl.c` (from MAME, by Jarek Burczynski and Tatsuyuki Satoh) | GPL-2.0-or-later |
| `src/lib/p64/` (P64 reference implementation by Benjamin Rosseaux) | zlib license |
| `src/lib/md5/` | Derived from the RSA Data Security MD5 reference; no further terms |
| `src/sha1.c`, `src/sha1.h` | Public domain |
| `src/lib/linenoise-ng/` | BSD license (used only by VICE's `c1541` tool, which Kairo64 does not build) |
| `src/tapeport/tapecart-loader.h` | BSD license |

### The shader runtime

Since VICE is GPL-2.0-or-later, with parts GPL-3.0-or-later, Kairo64 links the shared RetroArch Slang shader runtime, which uses librashader under its GPL-3.0-only option and includes Rust crates licensed only under Apache-2.0, and the combined app is distributed under GPLv3 terms, as the other Kairo apps are.

### Kairo64's build of VICE

Kairo64 builds VICE's `x64sc` sources, as VICE's own makefiles list them for a build without a user interface (`tools/vice_sources.py`), with its own replacements for the parts that start the program, pace it, show its picture, play its sound and end it (`core/kairo/`, GPL-2.0-or-later). It builds none of VICE's other emulators, its tools, or its GTK3 and SDL user interfaces.

## MEGA65 Open ROMs

`third_party/open-roms/` holds the [MEGA65 Open ROMs](https://github.com/MEGA65/open-roms), a free replacement for the C64's KERNAL, BASIC and character ROMs written without Commodore's code, and its source. Its `LICENSE` licenses it under the GNU Lesser General Public License, version 3 or (at your option) any later version, with some BASIC files under the MIT license from Microsoft's released 6502 BASIC. The app ships its generic C64 build (`bin/kernal_generic.rom`, `bin/basic_generic.rom`, `bin/chargen_openroms.rom`), which runs inside the emulated C64 as its system ROMs; it is not linked into the app.

## Notices in the app

`shared/tools/notices.py .` writes the app's `THIRD_PARTY_NOTICES.txt` (About → Licenses) from `tools/notices_sources.py`, which lists the license files and source notices above: VICE's license statement and GPL text, the notices of the files under other terms, the Open ROMs and zlib. `tools/notices.py --check` fails when the file is out of date.

## Libraries VICE needs

| Library | Location | License |
| --- | --- | --- |
| zlib 1.3.2 | `third_party/zlib/` | zlib license (`LICENSE`) |

## Shared frontend

<!-- kairo:shared-frontend-license -->
The shared RetroArch Slang shader runtime uses librashader under its GPL-3.0-only option, so a shader-enabled Kairo64 build is distributed under GPLv3 terms using first-party "or later" permission. See [shared licensing](../shared/docs/licensing.md).
<!-- /kairo:shared-frontend-license -->

## Firmware and games

Apart from the Open ROMs, Kairo64 does not include Commodore system firmware (the KERNAL, BASIC, character or drive ROMs) or games. Users supply any Commodore ROMs and games they need; the app copies imported ROMs to its private storage.
