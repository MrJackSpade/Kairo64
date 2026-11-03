# Development

## Building

Build the app, and optionally install it, with the shared build script (see [Android build](../shared/docs/android-build.md)):

```powershell
shared\tools\build.ps1 -Install <adb serial>
```

## Host test

The shared PC boot test runs the core on the host from `core/test/boot-tests.json`: run `shared\tools\test-core.ps1` (`-Test <name>` for some entries).

VICE keeps its state in process-wide variables, so the core builds it as a library of its own (`kairo64_vice`) and loads a fresh copy of it for every machine (`core/kairo/kairo_c64.h`): each machine starts from VICE's initial state, as a new process would.

The catalog passes boot games through `core/test/firmware_pass_run.cpp`, which builds for the host with `core/` (CMake, into `.build\host`) and runs under `shared\tools\run-limited.ps1`, with absolute paths:

```
firmware_pass_run <firmware folder> <work folder> <game> <seconds> <system> <keys> <model>
```

`<system>` is `open` (the Open ROMs in `third_party/open-roms/bin`) or `own` (the KERNAL, BASIC and character ROMs in the firmware folder, named `kernal`, `basic` and `chargen`), each with VICE's virtual drive, or with `+1541` an emulated 1541 on the folder's `dos1541`. It prints one JSON line (how long the app would run the load flat out, the sound's range and the distinct pictures after the load, the last picture's colours, the highest raster interrupt line) and leaves the last picture and the text screen in the work folder; `firmware_pass_run.cpp` describes the rest.

`tools/firmware_pass.py <seconds>` boots every game the catalog lists on the host, from the Open ROMs and virtual drive up to the player's firmware and the 1541, and records in `catalog/research/firmware-v1.json` the games that need firmware. A game that only shows one still picture in silence is doubtful: the pass tries the next setups too, and keeps its frame for review.

## VICE's sources

VICE builds with automake. `tools/vice_sources.py` writes `core/vice_sources.cmake`, the sources and include directories of its `x64sc`, from its `Makefile.am` files, and `tools/generate_infocontrib.sh` writes its credits, `core/generated/infocontrib.h`. Run both after a VICE update, and `tools/local_patches.py` to apply Kairo64's changes to the new snapshot; `tools/checks.sh` checks all three.
