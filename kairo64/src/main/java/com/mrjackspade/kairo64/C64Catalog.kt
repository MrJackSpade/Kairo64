package com.mrjackspade.kairo64

import com.mrjackspade.kairo.frontend.ChoiceOption
import com.mrjackspade.kairo.frontend.LayeredGameCatalog
import com.mrjackspade.kairo.frontend.MachineOption
import org.json.JSONArray

/**
 * Kairo64's catalog records beyond the common fields: the C64 model a game runs on (`model`, by
 * VICE's name for it), its system ROMs, its drive and what is in its control ports.
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
     * The KERNAL, BASIC and character ROMs: the MEGA65 Open ROMs, which come with the app, or the
     * player's Commodore ROMs, which games that call the KERNAL's insides need.
     */
    val system = MachineOption("system", "System", listOf(
        ChoiceOption(OPEN_ROMS, "Open ROMs"),
        ChoiceOption(OWN_ROMS, "Your C64 firmware")), OPEN_ROMS)

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

    override val fields: Map<String, Set<String>?> = mapOf(MachineOption.SECTION to options.map { it.id }.toSet())
    override val singleMediaRoles = emptySet<String>()

    override fun validMedia(role: String, contentId: String) = false

    override fun validField(field: String, value: Any): Boolean =
        field == MachineOption.SECTION && MachineOption.validSection(options, value)

    override fun validBindings(bindings: JSONArray) = C64Keys.bindings.valid(bindings)
}
