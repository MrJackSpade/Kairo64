<!-- kairo:discord -->
<p align="center">
  <a href="https://discord.gg/ZkYQ3bPTTq"><img src="https://raw.githubusercontent.com/MrJackSpade/Kairo/main/docs/assets/discord-banner.svg" alt="Join the Kairo Discord" width="49%"></a>
  <a href="https://play.google.com/store/apps/details?id=com.loxifi.kairo64"><img src="https://raw.githubusercontent.com/MrJackSpade/Kairo/main/docs/assets/donate-banner.svg" alt="Donate: buying Kairo64 on Google Play is the donation; it adds no features" width="49%"></a>
</p>
<!-- /kairo:discord -->

<!-- kairo:family -->
<h3 align="center">Other Kairo apps</h3>
<table align="center">
  <tr>
    <td align="center" width="33%"><a href="https://github.com/MrJackSpade/Kairo98"><img src="https://raw.githubusercontent.com/MrJackSpade/Kairo98/main/kairo98/src/main/res/drawable-nodpi/product_icon_art.png" width="96" alt="Kairo98"><br><b>Kairo98</b></a><br>PC-98</td>
    <td align="center" width="33%"><a href="https://github.com/MrJackSpade/KairoDos"><img src="https://raw.githubusercontent.com/MrJackSpade/KairoDos/main/kairodos/src/main/res/drawable-nodpi/product_icon_art.png" width="96" alt="KairoDos"><br><b>KairoDos</b></a><br>DOS</td>
    <td align="center" width="33%"><a href="https://github.com/MrJackSpade/Kairo88"><img src="https://raw.githubusercontent.com/MrJackSpade/Kairo88/main/kairo88/src/main/res/drawable-nodpi/product_icon_art.png" width="96" alt="Kairo88"><br><b>Kairo88</b></a><br>PC-88</td>
  </tr>
  <tr>
    <td align="center" width="33%"><a href="https://github.com/MrJackSpade/Kairo68"><img src="https://raw.githubusercontent.com/MrJackSpade/Kairo68/main/kairo68/src/main/res/drawable-nodpi/product_icon_art.png" width="96" alt="Kairo68"><br><b>Kairo68</b></a><br>X68000</td>
    <td align="center" width="33%"><a href="https://github.com/MrJackSpade/KairoTowns"><img src="https://raw.githubusercontent.com/MrJackSpade/KairoTowns/main/kairotowns/src/main/res/drawable-nodpi/product_icon_art.png" width="96" alt="KairoTowns"><br><b>KairoTowns</b></a><br>FM Towns</td>
    <td align="center" width="33%"><a href="https://github.com/MrJackSpade/KairoMSX"><img src="https://raw.githubusercontent.com/MrJackSpade/KairoMSX/main/kairomsx/src/main/res/drawable-nodpi/product_icon_art.png" width="96" alt="KairoMSX"><br><b>KairoMSX</b></a><br>MSX</td>
  </tr>
  <tr>
    <td align="center" width="33%"><a href="https://github.com/MrJackSpade/KairoST"><img src="https://raw.githubusercontent.com/MrJackSpade/KairoST/main/kairost/src/main/res/drawable-nodpi/product_icon_art.png" width="96" alt="KairoST"><br><b>KairoST</b></a><br>Atari ST</td>
  </tr>
</table>
<p align="center">Every Kairo app is built on <a href="https://github.com/MrJackSpade/Kairo">Kairo</a>, the shared frontend for the game library, controls, and settings.</p>
<!-- /kairo:family -->

# Kairo64

**Play your own Commodore 64 games on Android phones and handhelds.** Kairo64 is an Android emulator for the Commodore 64, built on the [Kairo shared frontend](https://github.com/MrJackSpade/Kairo): the same game library with artwork, controller and touch controls, on-screen keyboard, save states, and per-game settings as [Kairo98](https://github.com/MrJackSpade/Kairo98) and [KairoDos](https://github.com/MrJackSpade/KairoDos). It runs a pinned copy of [VICE](https://vice-emu.sourceforge.io/)'s cycle-exact C64 emulation. It is an independent project, not an official VICE release.

## Download

<!-- kairo:download -->
- **[Google Play](https://play.google.com/store/apps/details?id=com.loxifi.kairo64)** (paid): updates automatically. Buying it supports development.
- **[GitHub Releases](https://github.com/MrJackSpade/Kairo64/releases)** (free): install the APK yourself. The larger `-with-images` APK also bundles catalog artwork.

Both editions are built from the same source and have the same features. Requires an ARM64 device running Android 8.0 or newer.
<!-- /kairo:download -->

## Quick start

1. Install Kairo64 from [GitHub Releases](https://github.com/MrJackSpade/Kairo64/releases).
2. Open Kairo64 and choose the folder that holds your C64 games. You can change it later in **Menu › Manage library**.
3. Select a game and tap **Play**. Games start on the Open ROMs, which come with the app; some need the original Commodore firmware (see [Firmware](#firmware)).

Games and Commodore firmware are not supplied.

## Supported formats

| Type | Formats |
| --- | --- |
| Disks | D64, G64, D71, D81 |
| Tapes | T64, TAP |
| Programs | PRG, P00 |
| Cartridges | CRT |
| Archives | Any of the above inside ZIP or 7z archives |

A game starts the way a C64 user would start it: Kairo64 loads its first program and runs it. **Menu › Disks** changes the disk in drive 8 during play, for games with several disks or sides.

## Firmware

Kairo64 comes with the [MEGA65 Open ROMs](https://github.com/MEGA65/open-roms), a free replacement for the C64's KERNAL, BASIC and character ROMs, which start most games without any firmware from you. For the games that need the original Commodore firmware, import the ROMs dumped from your own machine in **Menu › Machine**: **Import all from a folder…** finds the KERNAL, BASIC and character ROMs and the 1541 drive ROM by their usual names or by their contents.

- **Your C64 firmware** (KERNAL, BASIC and character ROMs) runs games that call into the KERNAL's own code.
- **The 1541 drive ROM** lets drive 8 be an emulated 1541 down to its own processor, which fast loaders and copy-protected disks need. Without it, drive 8 is a virtual drive that loads files the standard way.

## Machine settings

**Menu › Machine** sets these for all games; each game can override them in its **Game settings**, and the catalog sets them for games that need something else.

- **Model:** C64 (PAL, the default), C64C (PAL, with the 8580 sound chip), their NTSC versions, the early C64 and the Drean.
- **System:** Open ROMs (the default) or your C64 firmware.
- **Drive:** the virtual drive (the default) or the 1541.
- **Controls:** a joystick in port 2 (the default) or port 1, or the 1351 mouse in port 1 with a joystick in port 2.

## Documentation

- [Licensing and credits](docs/licensing.md): code license and third-party components
- [Source provenance](docs/source-import.md): where the emulator source came from and what was copied
- [Development](docs/development.md): building, the host test and VICE's generated sources

## Help and community

<!-- kairo:community -->
- **Report a game problem** in [Issues](https://github.com/MrJackSpade/Kairo64/issues). Include the title, file format, and what happened.
- **Chat** with other players on [Discord](https://discord.gg/ZkYQ3bPTTq).
<!-- /kairo:community -->
