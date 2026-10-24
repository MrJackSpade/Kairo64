# Source import policy

[← Kairo64](../README.md)

This repository is independent. Its only Git remote is `origin`, pointing to this project's repository. The first-party Kairo frontend is pinned at `shared/` as a submodule. Do not add an `upstream` remote, fork relationship, other submodule, subtree sync, or automated upstream merge. Third-party source is copied in as pinned snapshots under `third_party/`.

## VICE snapshot

`third_party/vice/` holds [VICE](https://vice-emu.sourceforge.io/) 3.10.

| | |
| --- | --- |
| Release | 3.10 (2025-12-24) |
| Archive | <https://sourceforge.net/projects/vice-emu/files/releases/vice-3.10.tar.gz/download> |
| Archive SHA-256 | `8e5bac18cbcb9f192380ad3ef881f8790f5b75c41d7b3da65d831985d864d6d1` |
| Imported | 2026-10-24 |

Copied from the archive: `COPYING`, `README`, `NEWS`, `configure.ac` (the reference for Kairo64's `config.h` settings), `doc/vice.texi` (the source of VICE's credits, below) and `src/`, except for:

- `data/`: Commodore's system and drive ROMs, which Kairo64 does not distribute, and VICE's palette files, keyboard maps and desktop resources. Kairo64 uses VICE's built-in palette.
- `doc/` apart from `vice.texi`.
- `src/arch/gtk3/` and `src/arch/sdl/`: VICE's desktop user interfaces. Kairo64 builds the headless one.
- `src/lib/libusbsiddrv/`: the driver for USB SID hardware, the only code in VICE whose notice is GPL version 2 without "or later" (see [licensing](licensing.md)).
- The nested `.gitignore` files, removed so every copied file is tracked.

VICE builds with automake. `tools/vice_sources.py` evaluates its `Makefile.am` files for a headless `x64sc` and writes `core/vice_sources.cmake`: the program's sources, then each library it links, with its include directories; `--check` regenerates and compares. VICE's build generates its credits (`infocontrib.h`) from `doc/vice.texi`; `tools/generate_infocontrib.sh` runs VICE's own script and keeps the output in `core/generated/`.

## MEGA65 Open ROMs

| | |
| --- | --- |
| Source and ROM images (`third_party/open-roms/`) | Commit `ad178dbe4d48cd6a317737a8e0e7e662f7e33d32` (2026-05-16), <https://codeload.github.com/MEGA65/open-roms/tar.gz/ad178dbe4d48cd6a317737a8e0e7e662f7e33d32>, SHA-256 `7e7fb6e775a0d820e8605107fec168e17ab232ad1172bc788bbc492996fcf229` |
| Imported | 2026-10-24 |

The app ships the generic C64 build in `bin/`: `kernal_generic.rom`, `basic_generic.rom` and `chargen_openroms.rom`. The snapshot is the project's tree at that commit, with its prebuilt images, copied whole except for `testsuite/` (prebuilt test disks and tapes), the empty `3rdparty/` submodule folders, `.github/` and the nested Git files.

## Libraries VICE needs

VICE links zlib. It is a pinned release copied into `third_party/`, built by `core/CMakeLists.txt`.

| Library | Release | Archive | Archive SHA-256 | Released |
| --- | --- | --- | --- | --- |
| zlib (`zlib/`) | 1.3.2 | GitHub release `v1.3.2` of `madler/zlib`, `zlib-1.3.2.tar.gz` | `bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16` | 2026-02-17 |

It was imported on 2026-10-24 as the same snapshot as KairoST's (the files match), which copied the archive whole except for `amiga/`, `contrib/`, `doc/`, `msdos/`, `os400/`, `qnx/`, `test/`, `watcom/` and `win32/`.

## Local changes

`tools/local_patches.py` makes these changes to the snapshots and checks they are in place (`--check`). Each replaces text that occurs exactly once.

| Change | File | What and why |
| --- | --- | --- |
| `vice-kairo-sound` | `vice/src/sound.c`, `sound.h` | Lists Kairo64's sound device (`core/kairo/soundkairo.c`) first in VICE's fixed table of sound devices. It hands VICE's mixed sound to the host, whose audio device paces the machine. |
| `vice-jam` | `vice/src/arch/headless/ui.c` | A JAM instruction, which stops the C64's CPU or a drive's, reaches Kairo64 (`KairoC64Host_Jam`), which ends the game with the reason. The headless UI only logged it, and the machine went on running a CPU that does nothing. |

The rest of Kairo64's replacements need no change to VICE. `core/CMakeLists.txt` builds VICE without these files and compiles Kairo64's in their place (`core/kairo/`):

| VICE's file | Kairo64's | Why |
| --- | --- | --- |
| `src/vsync.c` | `vsync.c` | VICE paces itself to the host's clock at the end of each frame. In Kairo64 the frontend paces the machine by its audio device, so the end of a frame does VICE's frame work and hands the machine to the host until it asks for the next frame. |
| `src/arch/headless/main.c` | `main.c` | VICE starts on Kairo64's thread (`KairoC64_Run`), once per process. |
| `src/arch/headless/video.c` | `video.c` | The headless UI draws nothing; Kairo64 renders the VIC-II's frames to RGBA for the host. |
| `src/arch/headless/mousedrv.c` | `mousedrv.c` | The host's mouse buttons reach VICE's 1351 mouse emulation. |
| `src/arch/shared/archdep_exit.c` | `archdep_exit.c` | VICE's exit would end the whole app; Kairo64 ends the game with an error instead. |
| `src/arch/shared/archdep_default_logger.c` | `archdep_default_logger.c` | VICE's log goes to the host, which keeps the errors where Android's logs are read. |

VICE keeps process-wide state and cannot start twice in one process. Kairo64 starts it with the first game and keeps it: each later game is the same VICE set to the game's model, ROMs and drive, switched on again (a power cycle) and started with VICE's autostart (`core/c64_machine.cpp`).
