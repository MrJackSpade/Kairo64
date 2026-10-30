package com.mrjackspade.kairo64

import com.mrjackspade.kairo.frontend.R as FrontendR
import com.mrjackspade.kairo.frontend.CatalogGame
import com.mrjackspade.kairo.frontend.CatalogSetting
import com.mrjackspade.kairo.frontend.ChoiceOption
import com.mrjackspade.kairo.frontend.LayeredGameCatalog
import com.mrjackspade.kairo.frontend.MachineOption
import com.mrjackspade.kairo.frontend.UiText
import org.json.JSONArray

/**
 * Kairo64's catalog records beyond the common fields: the C64 model a game runs on (`model`, by
 * VICE's name for it), its system ROMs, its drive and what is in its control ports, and the program
 * its disk or tape starts (`launch.program`).
 */
object C64Catalog : LayeredGameCatalog.Schema {
    override val platform = "c64"
    override val catalogUrl = "https://raw.githubusercontent.com/MrJackSpade/Kairo64/main/catalog/"
    override val selfHostedArt = "/MrJackSpade/Kairo64/main/catalog/artwork/art/catalog/"
    override val contentId = Regex("sha256-(?:disk|tape|prg|cart|cmdhd|ide64)-v1:[0-9a-f]{64}")
    /** The C64 has no Esc key; its Return is the Enter. */
    override val escapeKeys = emptySet<Int>()
    override val enterKeys = setOf(C64Keys.RETURN)
    override val mediaExtensions =
        Regex("\\.(?:zip|7z|d64|g64|d71|d81|t64|tap|prg|p00|crt)$", RegexOption.IGNORE_CASE)

    /**
     * The model, by VICE's name for it: VIC-II, SID and CIA revisions and the TV standard. Most
     * games are PAL; the C64C has the 8580 SID, whose filters some later music is written for.
     */
    val model = MachineOption("model", UiText.res(R.string.option_model), listOf(
        ChoiceOption("c64", "C64 (PAL)"),
        ChoiceOption("c64c", "C64C (PAL, 8580 SID)"),
        ChoiceOption("c64ntsc", "C64 (NTSC)"),
        ChoiceOption("c64cntsc", "C64C (NTSC, 8580 SID)"),
        ChoiceOption("c64old", UiText.res(R.string.model_early, "C64 (PAL)")),
        ChoiceOption("c64oldntsc", UiText.res(R.string.model_early, "C64 (NTSC)")),
        ChoiceOption("drean", "Drean C64 (PAL-N)")), "c64")

    const val OPEN_ROMS = "openroms"
    const val OWN_ROMS = "own"

    /**
     * The KERNAL, BASIC and character ROMs: the player's Commodore ROMs once they have imported
     * them, else the MEGA65 Open ROMs, which come with the app (games that call the KERNAL's
     * insides need the player's).
     */
    val system = MachineOption("system", UiText.res(FrontendR.string.kairo_option_system), listOf(
        ChoiceOption(OPEN_ROMS, UiText.res(R.string.system_free, "C64")),
        ChoiceOption(OWN_ROMS, UiText.res(R.string.system_own, "C64"))), OPEN_ROMS, ownFirmware = OWN_ROMS)

    const val TRUE_DRIVE = "1541"
    const val VIRTUAL_DRIVE = "virtual"

    /**
     * Drive 8: an emulated 1541 down to its own CPU, on the player's 1541 ROM, which fast loaders
     * and copy protection need; or VICE's virtual drive, which loads files the standard way and
     * needs no ROM. A game set to the 1541 does not start until the ROM is imported.
     */
    val drive = MachineOption("drive", UiText.res(R.string.option_drive), listOf(
        ChoiceOption(VIRTUAL_DRIVE, UiText.res(R.string.drive_virtual)),
        ChoiceOption(TRUE_DRIVE, "1541")), VIRTUAL_DRIVE)

    /** What the host's joystick and mouse are: a joystick in port 2 or 1, or the 1351 mouse in port 1. */
    val ports = MachineOption("ports", UiText.res(R.string.option_ports), listOf(
        ChoiceOption("2", UiText.res(R.string.ports_joystick, 2)),
        ChoiceOption("1", UiText.res(R.string.ports_joystick, 1)),
        ChoiceOption("mouse", UiText.res(R.string.ports_mouse))), "2")

    val options = listOf(model, system, drive, ports)

    const val LAUNCH = "launch"
    const val PROGRAM = "program"

    /**
     * The program on a disk or tape that autostart loads and runs, by its name (CBM DOS patterns
     * such as SPACE* match too), for a disk whose first file is not the game; "" for its first.
     */
    val program = CatalogSetting.Text(LAUNCH, PROGRAM, UiText.res(R.string.start_program),
        UiText.res(R.string.start_program_hint), Regex("[ !#-~]{1,16}"), UiText.res(R.string.start_program_first),
        { it.trim().uppercase() }) {
        it.startsWith("sha256-disk-v1:") || it.startsWith("sha256-tape-v1:")
    }

    /** The program [game] starts: its own, or "" for its media's first. */
    fun program(game: CatalogGame) = program.value(game)

    const val COMMANDS = "commands"

    /** The lines typed at BASIC's READY. prompt to start the game, such as `SYS 49152` after a load. */
    val startupCommands = CatalogSetting.Lines(LAUNCH, COMMANDS, UiText.res(FrontendR.string.kairo_startup_commands),
        UiText.res(FrontendR.string.kairo_startup_commands_hint), Regex("[ -~]{1,80}"), 8) { _, _ -> emptyList() }

    override val settings: List<CatalogSetting> = options.map(CatalogSetting::Choice) + program + startupCommands +
        // A game's startup menus and disk swaps (shared StartupAutomation).
        listOf(CatalogSetting.StartupChoices,
        // Drive 8 and the datasette (MainActivity.mediaSlots).
        CatalogSetting.DiskSwaps(2) { drive, id -> id.startsWith(if (drive == 0) "sha256-disk-v1:" else "sha256-tape-v1:") })
    override val fields = CatalogSetting.fields(settings)
    /** A 1351 game's pointer goes to a point by a push to the corner and counts (C64Machine::mouse_warp). */
    override val directTouch = true

    override fun validMedia(role: String, contentId: String) = false

    override fun validField(field: String, value: Any): Boolean =
        field in fields && CatalogSetting.valid(settings, contentId, field, value)

    override fun validBindings(bindings: JSONArray) = C64Keys.bindings.valid(bindings)
}
