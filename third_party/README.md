# Third-party source snapshots

These directories contain copied, pinned source snapshots. They are not Git submodules or forks, and this repository has no upstream remote.

- `vice/`: VICE 3.10, with its license notices preserved. Its desktop user interfaces, its USB SID driver, its documentation (apart from `vice.texi`) and its data files were omitted; the data files include Commodore ROMs, which Kairo64 does not distribute.
- `open-roms/`: the MEGA65 Open ROMs, the free C64 system ROMs the app ships, and their source.
- `zlib/`: zlib 1.3.2, the library VICE links, without its documentation, tests and other platforms' build files.

`tools/local_patches.py` applies Kairo64's few local changes to these snapshots; the source import lists them.

See [source import](../docs/source-import.md) for URLs, hashes, dates, and what was included. Do not replace these snapshots through a Git remote or automated sync.
