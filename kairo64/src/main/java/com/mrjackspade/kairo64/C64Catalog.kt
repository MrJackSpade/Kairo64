package com.mrjackspade.kairo64

import com.mrjackspade.kairo.frontend.CatalogGame
import com.mrjackspade.kairo.frontend.CatalogSetting
import com.mrjackspade.kairo.frontend.ChoiceOption
import com.mrjackspade.kairo.frontend.LayeredGameCatalog
import com.mrjackspade.kairo.frontend.MachineOption
import org.json.JSONArray

/**
 * Kairo64's catalog records beyond the common fields: the C64 model a game runs on (`model`, by
 * VICE's name for it), its system ROMs, its drive and what is in its control ports, and the program
 * its disk or tape starts (`launch.program`).
 */
object C64Catalog : LayeredGameCatalog.Schema {
    override val platform = "c64"
    override val updateUrl = "https://raw.githubusercontent.com/MrJackSpade/Kairo64/main/catalog/core-v3.json"
    override val metadataUrl = "https://raw.githubusercontent.com/MrJackSpade/Kairo64/main/catalog/core-v3.meta.json"
    override val coreVersion = 3
    override val selfHostedArt = "/MrJackSpade/Kairo64/main/catalog/artwork/art/catalog/"
    override val contentId = Regex("sha256-(?:disk|tape|prg|cart)-v1:[0-9a-f]{64}")
    override val mediaExtensions =
        Regex("\\.(?:zip|7z|d64|g64|d71|d81|t64|tap|prg|p00|crt)$", RegexOption.IGNORE_CASE)

    /**
     * The model, by VICE's name for it: VIC-II, SID and CIA revisions and the TV standard. Most
     * games are PAL; the C64C has the 8580 SID, whose filters some later music is written for.
     */
    val model = MachineOption("model", "Model", listOf(
        ChoiceOption("c64", "C64 (PAL)"),
        ChoiceOption("c64c", "C64C (PAL, 8580 SID)"),
        ChoiceOption("c64ntsc", "C64 (NTSC)"),
        ChoiceOption("c64cntsc", "C64C (NTSC, 8580 SID)"),
        ChoiceOption("c64old", "Early C64 (PAL)"),
        ChoiceOption("c64oldntsc", "Early C64 (NTSC)"),
        ChoiceOption("drean", "Drean C64 (PAL-N)")), "c64")

    const val OPEN_ROMS = "openroms"
    const val OWN_ROMS = "own"

    /**
     * The KERNAL, BASIC and character ROMs: the player's Commodore ROMs once they have imported
     * them, else the MEGA65 Open ROMs, which come with the app (games that call the KERNAL's
     * insides need the player's).
     */
    val system = MachineOption("system", "System", listOf(
        ChoiceOption(OPEN_ROMS, "Free C64 firmware"),
        ChoiceOption(OWN_ROMS, "Your C64 firmware")), OPEN_ROMS, ownFirmware = OWN_ROMS)

    const val TRUE_DRIVE = "1541"
    const val VIRTUAL_DRIVE = "virtual"

    /**
     * Drive 8: an emulated 1541 down to its own CPU, on the player's 1541 ROM, which fast loaders
     * and copy protection need; or VICE's virtual drive, which loads files the standard way and
     * needs no ROM. A game set to the 1541 does not start until the ROM is imported.
     */
    val drive = MachineOption("drive", "Drive", listOf(
        ChoiceOption(VIRTUAL_DRIVE, "Virtual drive"),
        ChoiceOption(TRUE_DRIVE, "1541")), VIRTUAL_DRIVE)

    /** What the host's joystick and mouse are: a joystick in port 2 or 1, or the 1351 mouse in port 1. */
    val ports = MachineOption("ports", "Controls", listOf(
        ChoiceOption("2", "Joystick in port 2"),
        ChoiceOption("1", "Joystick in port 1"),
        ChoiceOption("mouse", "Mouse in port 1, joystick in port 2")), "2")

    val options = listOf(model, system, drive, ports)

    const val LAUNCH = "launch"
    const val PROGRAM = "program"

    /**
     * The program on a disk or tape that autostart loads and runs, by its name (CBM DOS patterns
     * such as SPACE* match too), for a disk whose first file is not the game; "" for its first.
     */
    val program = CatalogSetting.Text(LAUNCH, PROGRAM, "Start program", "Name on the disk or tape",
        Regex("[ !#-~]{1,16}"), "First program", { it.trim().uppercase() }) {
        it.startsWith("sha256-disk-v1:") || it.startsWith("sha256-tape-v1:")
    }

    /** The program [game] starts: its own, or "" for its media's first. */
    fun program(game: CatalogGame) = program.value(game)

    override val settings: List<CatalogSetting> = options.map(CatalogSetting::Choice) + program
    override val fields = CatalogSetting.fields(settings)

    override fun validMedia(role: String, contentId: String) = false

    override fun validField(field: String, value: Any): Boolean =
        field in fields && CatalogSetting.validSection(settings, field, value)

    override fun validBindings(bindings: JSONArray) = C64Keys.bindings.valid(bindings)
}
