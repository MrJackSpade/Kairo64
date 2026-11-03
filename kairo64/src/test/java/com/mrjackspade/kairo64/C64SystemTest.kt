package com.mrjackspade.kairo64

import com.mrjackspade.kairo.frontend.MachineOptionValues
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder

/**
 * A game's System, the free Open ROMs or the player's C64 firmware (Kairo#112): the default follows
 * the player's imported ROMs, a game's own choice wins, and a game on the player's firmware without
 * it fails to start, naming the missing files, rather than running the Open ROMs.
 */
class C64SystemTest {
    @get:Rule val folder = TemporaryFolder()
    private val set get() = C64Firmware.set(folder.root)

    private fun system(own: String? = null, stored: String? = null) =
        MachineOptionValues.values(listOf(C64Catalog.system), { own }, { stored }) { _, _ -> C64Firmware.systemInstalled(set) }
            .getValue(C64Catalog.system.id)

    private fun import(vararg parts: com.mrjackspade.kairo.frontend.FirmwareSet.Part) {
        for (part in parts) set.file(part).writeBytes(ByteArray(part.sizes.first().toInt()))
    }

    @Test
    fun theDefaultIsThePlayersFirmwareOnceImported() {
        assertEquals("before an import", C64Catalog.OPEN_ROMS, system())
        import(C64Firmware.kernal, C64Firmware.basic)
        assertEquals("with a ROM still missing", C64Catalog.OPEN_ROMS, system())
        import(C64Firmware.chargen)
        assertEquals("after the import", C64Catalog.OWN_ROMS, system())
    }

    @Test
    fun aGamesOwnChoiceWinsOverTheAllGamesChoice() {
        import(*C64Firmware.system.toTypedArray())
        assertEquals(C64Catalog.OPEN_ROMS, system(own = C64Catalog.OPEN_ROMS, stored = C64Catalog.OWN_ROMS))
        assertEquals("an all-games choice over the imported ROMs", C64Catalog.OPEN_ROMS, system(stored = C64Catalog.OPEN_ROMS))
    }

    @Test
    fun aStartOnThePlayersFirmwareWithoutItFailsNamingTheFiles() {
        assertEquals(C64Firmware.system, C64Firmware.missingForStart(C64Catalog.OWN_ROMS, set))
        assertEquals("the Open ROMs need nothing", emptyList<Any>(), C64Firmware.missingForStart(C64Catalog.OPEN_ROMS, set))
        import(C64Firmware.kernal)
        assertEquals(listOf(C64Firmware.basic, C64Firmware.chargen), C64Firmware.missingForStart(C64Catalog.OWN_ROMS, set))
        import(C64Firmware.basic, C64Firmware.chargen)
        assertEquals(emptyList<Any>(), C64Firmware.missingForStart(C64Catalog.OWN_ROMS, set))
    }
}
