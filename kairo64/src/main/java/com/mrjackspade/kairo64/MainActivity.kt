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
import com.mrjackspade.kairo.frontend.UiText
import com.mrjackspade.kairo.frontend.text
import com.mrjackspade.kairo.frontend.R as FrontendR

import java.io.File
import java.util.concurrent.atomic.AtomicBoolean

/** Kairo64: the Commodore 64 on VICE, in the shared Kairo frontend. */
class MainActivity : MachineActivity<DiskImageEntry>() {
    override val identity = Identity("KAIRO64", 0xff006BFD.toInt(), "Kairo64", "C64", "kairo64",
        LibraryStrings("KAIRO64", "Kairo64"),
        UiText.res(R.string.credits, "Kairo64", "VICE", "Commodore 64", "MEGA65", "C64", "MrJackSpade/Kairo64"),
        UiText.res(R.string.library_empty), UiText.res(R.string.library_expired),
        MachineFirstRun.Folder(UiText.res(R.string.folder_title), UiText.res(R.string.folder_description, "C64", "Kairo64"),
            UiText.res(R.string.folder_button)),
        UiText.res(R.string.saves_group), "MrJackSpade/Kairo64")
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
        C64Keys.joystick.zip(C64Keys.joystickLabels), UiText.res(R.string.joystick_help),
        ControllerGuestSpec.STANDARD_ACTIONS, C64Keys.layout)
    override val bindingsCodec = C64Keys.bindings
    override val artworkDownloadsEnabled: Boolean get() = resources.getBoolean(R.bool.catalog_art_download_enabled)

    /** Disk, tape, program and cartridge images, which VICE's autostart starts (c64_machine.h). */
    private val format = DiskImageLibrary.Format(
        kinds = listOf("d64", "g64", "d71", "d81").associateWith { "disk" } +
            listOf("t64", "tap").associateWith { "tape" } +
            listOf("prg", "p00").associateWith { "prg" } +
            mapOf("crt" to "cart"),
        labels = mapOf("disk" to UiText.res(R.string.disk), "tape" to UiText.res(R.string.tape),
            "prg" to UiText.res(R.string.program), "cart" to UiText.res(R.string.cartridge)),
        maxBytes = mapOf("disk" to 2L * 1024 * 1024, "tape" to 64L * 1024 * 1024, "prg" to 64L * 1024,
            "cart" to 16L * 1024 * 1024),
        guestName = "C64")
    private val images by lazy { DiskImageLibrary(this, format) }
    override fun createCatalog() = LayeredGameCatalog(this, C64Catalog)
    override fun createLibrary(catalog: LayeredGameCatalog) = images

    override val machineOptions = C64Catalog.options
    private fun isDisk(entry: DiskImageEntry) = format.kind(entry.displayName) == "disk"

    // In the core's drive order (c64_machine.h Drive).
    override val mediaSlots = listOf(MediaSlot<DiskImageEntry>(UiText.res(R.string.drive_8), ::isDisk))

    private val firmwareDirectory by lazy { File(filesDir, "firmware") }
    override val firmwareSets by lazy { listOf(UiText.res(R.string.firmware_set, "C64") to C64Firmware.set(firmwareDirectory)) }
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
                getString(R.string.own_firmware_missing, "C64",
                    missing.joinToString(getString(FrontendR.string.kairo_list_separator)) { text(it.label) })
            }
        }
        if (options.getValue(C64Catalog.drive.id) == C64Catalog.TRUE_DRIVE) {
            check(set.installed(C64Firmware.dos1541)) {
                getString(R.string.drive_firmware_missing, "1541", text(C64Firmware.dos1541.label))
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
            progress(getString(R.string.preparing_emulator))
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
}
