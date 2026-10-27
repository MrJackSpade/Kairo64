package com.mrjackspade.kairo64

import com.mrjackspade.kairo.frontend.MachineOption
import com.mrjackspade.kairo.frontend.CatalogGame
import com.mrjackspade.kairo.frontend.ControllerGuestSpec
import com.mrjackspade.kairo.frontend.ControllerLayout
import com.mrjackspade.kairo.frontend.DiskImageEntry
import com.mrjackspade.kairo.frontend.DiskImageLibrary
import com.mrjackspade.kairo.frontend.FirmwareSet
import com.mrjackspade.kairo.frontend.LayeredGameCatalog
import com.mrjackspade.kairo.frontend.LibraryStrings
import com.mrjackspade.kairo.frontend.MachineActivity
import com.mrjackspade.kairo.frontend.MachineFirstRun
import com.mrjackspade.kairo.frontend.NativeMachine

import java.io.File
import java.util.concurrent.atomic.AtomicBoolean

/** Kairo64: the Commodore 64 on VICE, in the shared Kairo frontend. */
class MainActivity : MachineActivity<DiskImageEntry>() {
    override val identity = Identity("KAIRO64", 0xff006BFD.toInt(), "Kairo64", "C64", "kairo64",
        LibraryStrings("KAIRO64", "Kairo64"),
        "Kairo64 uses VICE's Commodore 64 emulation and the MEGA65 project's free C64 firmware. Source and " +
            "provenance: github.com/MrJackSpade/Kairo64.",
        "Choose a games folder to find disk, tape, program and cartridge images and the ZIP and 7z archives holding them",
        "Folder access expired. Select the games folder again.",
        MachineFirstRun.Folder("Choose a games folder", "Select the folder containing your C64 games. Kairo64 will " +
            "scan its disk, tape, program and cartridge images and the ZIP and 7z archives holding them.",
            "Choose games folder…"),
        "SAVE STATES", "MrJackSpade/Kairo64")
    // The VIC-II's picture with its normal borders is 384 x 272 (PAL); mouse motion is counted in
    // C64 pixels of its 320 x 200 screen.
    override val machine = Machine(320, 200, 4.0 / 3.0, "state.vsf")
    override val bridge by lazy { NativeMachine("kairo64") }
    override val keyboard = C64Keys.layout
    override val maxKeyCode = C64Keys.MAX_CODE
    override fun guestKey(androidKey: Int) = C64Keys.fromAndroid(androidKey)
    override val joystickControls get() = C64Keys.joystick
    override val controllerSpec = ControllerGuestSpec("C64", C64Keys.bindableCodes, { C64Keys.label(it) ?: "Key $it" },
        setOf(C64Keys.LEFT_SHIFT, C64Keys.RIGHT_SHIFT, C64Keys.CONTROL, C64Keys.COMMODORE),
        C64Keys.joystick.zip(C64Keys.joystickLabels), "The joystick in the game's port (Game settings › Controls).",
        ControllerGuestSpec.STANDARD_ACTIONS, C64Keys.layout)
    override val bindingsCodec = C64Keys.bindings
    override val artworkDownloadsEnabled: Boolean get() = resources.getBoolean(R.bool.catalog_art_download_enabled)

    /** Disk, tape, program and cartridge images, which VICE's autostart starts (c64_machine.h). */
    private val images by lazy {
        DiskImageLibrary(this, DiskImageLibrary.Format(
            kinds = listOf("d64", "g64", "d71", "d81").associateWith { "disk" } +
                listOf("t64", "tap").associateWith { "tape" } +
                listOf("prg", "p00").associateWith { "prg" } +
                mapOf("crt" to "cart"),
            labels = mapOf("disk" to "Disk", "tape" to "Tape", "prg" to "Program", "cart" to "Cartridge"),
            maxBytes = mapOf("disk" to 2L * 1024 * 1024, "tape" to 64L * 1024 * 1024, "prg" to 64L * 1024,
                "cart" to 16L * 1024 * 1024),
            guestName = "C64"))
    }
    override fun createCatalog() = LayeredGameCatalog(this, C64Catalog)
    override fun createLibrary(catalog: LayeredGameCatalog) = images

    override val machineOptions = C64Catalog.options
    override val mediaPageTitle = "Disks"
    private fun isDisk(entry: DiskImageEntry) = entry.mediaLabel == "Disk"

    // In the core's drive order (c64_machine.h Drive).
    override val mediaSlots = listOf(MediaSlot<DiskImageEntry>("Drive 8", ::isDisk))

    private val firmwareDirectory by lazy { File(filesDir, "firmware") }
    override val firmwareSets by lazy { listOf("C64 firmware" to C64Firmware.set(firmwareDirectory)) }
    override fun ownFirmwareInstalled(option: MachineOption, values: Map<String, String>): Boolean {
        check(option.id == C64Catalog.system.id) { "${option.id} has no firmware of the player's" }
        return C64Firmware.system.all(firmwareSets.single().second::installed)
    }

    override fun startGame(entry: DiskImageEntry, game: CatalogGame, cancelled: AtomicBoolean,
                           progress: (String) -> Unit) {
        val home = prepareHome(progress)
        val options = optionValues(game).toMutableMap()
        val set = firmwareSets.single().second
        if (options.getValue(C64Catalog.system.id) == C64Catalog.OWN_ROMS) {
            val missing = C64Firmware.system.filterNot(set::installed)
            check(missing.isEmpty()) {
                "This game starts on your C64 firmware: import ${missing.joinToString(", ") { it.label }} in Menu › Machine"
            }
        }
        if (options.getValue(C64Catalog.drive.id) == C64Catalog.TRUE_DRIVE) {
            check(set.installed(C64Firmware.dos1541)) {
                "This game runs on the 1541 drive: import the ${C64Firmware.dos1541.label} in Menu › Machine"
            }
        }
        val start = images.prepare(entry, cancelled, progress).absolutePath
        options[C64Catalog.PROGRAM] = C64Catalog.program(game)
        // In the core's order (c64_core.cpp): the game to start, then drive 8's disk.
        check(bridge.start(firmwareDirectory.absolutePath, home.absolutePath,
            listOf(start, if (isDisk(entry)) start else ""), options)) { "Another machine is still running" }
        awaitRunning()
        mediaStarted(listOf(entry.takeIf(::isDisk)))
    }

    /**
     * The core's home (c64_core.cpp): "openroms", the Open ROMs copied from assets/openroms when
     * they differ from the app's, and "vice", VICE's own files.
     */
    private fun prepareHome(progress: (String) -> Unit): File {
        val home = File(filesDir, "machine")
        val openRoms = File(home, "openroms")
        for (name in OPEN_ROMS) {
            val installed = File(openRoms, name)
            val shipped = assets.open("openroms/$name").use { it.readBytes() }
            if (installed.isFile && installed.readBytes().contentEquals(shipped)) continue
            progress("Preparing the emulator")
            check(openRoms.mkdirs() || openRoms.isDirectory) { "Could not create $openRoms" }
            val copy = File(openRoms, "$name.new")
            copy.writeBytes(shipped)
            check(copy.renameTo(installed)) { "Could not install $name in $openRoms" }
        }
        File(home, "vice").mkdirs()
        return home
    }

    private companion object {
        /** The MEGA65 Open ROMs the app ships (third_party/open-roms/bin). */
        val OPEN_ROMS = listOf("kernal_generic.rom", "basic_generic.rom", "chargen_openroms.rom")
    }

    override fun stateError(code: Int) = when (code) {
        2 -> "the emulator could not read or write the state"
        3 -> "the emulator refused the disk image"
        else -> "code $code"
    }
}
