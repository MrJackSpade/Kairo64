# Development

## Building

Build the app, and optionally install it, with the shared build script (see [Android build](../shared/docs/android-build.md)):

```powershell
shared\tools\build.ps1 -Install <adb serial>
```

## Host test

The machine also builds for the host, without Android: build `core/` with CMake for the host (the shared `shared\tools\test-core.ps1` does, into `.build\host`), then run, under `shared\tools\run-limited.ps1` and with absolute paths:

```
boot_test <firmware folder> <work folder> <game> <seconds> [system [runs [keys [model [state]]]]]
```

`<system>` is `open` (the Open ROMs in `third_party/open-roms/bin`) or `own` (the KERNAL, BASIC and character ROMs in the firmware folder, named `kernal`, `basic` and `chargen`), each with VICE's virtual drive, or with `+1541` an emulated 1541 on the folder's `dos1541`. It writes frames, the text screen and VICE's log to the work folder; `boot_test.cpp` describes the rest.

`tools/firmware_pass.py <seconds>` boots every game the catalog lists on the host, from the Open ROMs and virtual drive up to the player's firmware and the 1541, and records in `catalog/research/firmware-v1.json` the games that need firmware.

## VICE's sources

VICE builds with automake. `tools/vice_sources.py` writes `core/vice_sources.cmake`, the sources and include directories of its `x64sc`, from its `Makefile.am` files, and `tools/generate_infocontrib.sh` writes its credits, `core/generated/infocontrib.h`. Run both after a VICE update, and `tools/local_patches.py` to apply Kairo64's changes to the new snapshot; `tools/checks.sh` checks all three.
