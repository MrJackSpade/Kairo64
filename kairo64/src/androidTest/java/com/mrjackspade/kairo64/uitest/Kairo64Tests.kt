package com.mrjackspade.kairo64.uitest

import com.mrjackspade.kairo.uitest.AppUnderTest
import com.mrjackspade.kairo.uitest.AppUnderTest.BiosFile
import com.mrjackspade.kairo.uitest.AppUnderTest.MediaSwap
import com.mrjackspade.kairo.uitest.KairoUiTest
import com.mrjackspade.kairo.uitest.suites.ControllerTests
import com.mrjackspade.kairo.uitest.suites.FirstRunTests
import com.mrjackspade.kairo.uitest.suites.GameTests
import com.mrjackspade.kairo.uitest.suites.LaunchTests
import com.mrjackspade.kairo.uitest.suites.LibraryManagementTests
import com.mrjackspade.kairo.uitest.suites.LibraryTests
import com.mrjackspade.kairo.uitest.suites.MediaTests
import com.mrjackspade.kairo.uitest.suites.MenuTests
import com.mrjackspade.kairo.uitest.suites.SearchTests
import com.mrjackspade.kairo.uitest.suites.SettingsTests
import com.mrjackspade.kairo.uitest.suites.StateTests
import com.mrjackspade.kairo.uitest.suites.TouchTests
import com.mrjackspade.kairo.uitest.suites.WordingTests
import com.mrjackspade.kairo64.C64Catalog
import com.mrjackspade.kairo64.C64Keys
import org.junit.Test

/**
 * Kairo64 as the shared UI tests see it. Its test resources are two single-disk games in D64
 * images, which start on the Open ROMs and the virtual drive, and the player's C64 and 1541 ROMs;
 * the Open ROMs ship with the app. The C64 has two cursor keys: the D-pad's up and left send them
 * with Shift.
 */
object Kairo64App : AppUnderTest() {
    override val name = "Kairo64"
    override val brand = "KAIRO64"
    override val biosTitle = "C64 BIOS"
    override val biosFiles = listOf(BiosFile("kernal.bin", false), BiosFile("basic.bin", false),
        BiosFile("chargen.bin", false), BiosFile("dos1541.bin", false))
    override val bootGame = "Klax"
    override val bootGameFile = "games/Klax (Europe).D64"
    override val otherGame = "Deflektor"
    override val defaultOption = "model"
    override val searchTerm = "Klax"
    override val arrowKeys = listOf(C64Keys.CURSOR_DOWN, C64Keys.CURSOR_DOWN, C64Keys.CURSOR_RIGHT, C64Keys.CURSOR_RIGHT)
    override val returnKey = C64Keys.RETURN
    override val spaceKey = C64Keys.SPACE
    override val mediaSwap = MediaSwap("Klax", "Drive 8", "Deflektor", filler = "d64")
}

class Kairo64FirstRunTests : FirstRunTests() { override val app = Kairo64App }
class Kairo64LibraryTests : LibraryTests() { override val app = Kairo64App }
class Kairo64SearchTests : SearchTests() { override val app = Kairo64App }
class Kairo64GameTests : GameTests() { override val app = Kairo64App }
class Kairo64WordingTests : WordingTests() { override val app = Kairo64App }
class Kairo64MediaTests : MediaTests() { override val app = Kairo64App }
class Kairo64MenuTests : MenuTests() { override val app = Kairo64App }
class Kairo64ControllerTests : ControllerTests() { override val app = Kairo64App }
class Kairo64TouchTests : TouchTests() { override val app = Kairo64App }
class Kairo64LaunchTests : LaunchTests() { override val app = Kairo64App }
class Kairo64LibraryManagementTests : LibraryManagementTests() { override val app = Kairo64App }
class Kairo64SettingsTests : SettingsTests() { override val app = Kairo64App }
class Kairo64StateTests : StateTests() { override val app = Kairo64App }

/** The C64's own behavior: a game's System and Drive settings start it on the player's firmware. */
class C64Tests : KairoUiTest() {
    override val app = Kairo64App

    /** Starts Klax with one of its settings changed, and checks the machine took it and draws. */
    private fun startsWith(option: String, value: String) {
        val game = screens.completeFirstRun().open(app.bootGame)
        val settings = game.openSettings()
        screens.choose(settings, option, value)
        game.closeSettings()
        game.play()
        expect.equal("Klax's $option after choosing $value for it", value, driver.probe().details["machine.$option"])
        driver.waitFrames(600)
        val picture = driver.pictures.game("klax-$option-$value")
        expect.that("Klax draws on $option $value", picture.drawn()) { "lit ${picture.litFraction()}" }
    }

    @Test
    fun aGameSetToThePlayersFirmwareStartsOnIt() = startsWith("system", C64Catalog.OWN_ROMS)

    @Test
    fun aGameSetToThe1541RunsTheDrive() = startsWith("drive", C64Catalog.TRUE_DRIVE)
}
