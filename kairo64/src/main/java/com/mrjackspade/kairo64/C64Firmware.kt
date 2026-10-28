package com.mrjackspade.kairo64

import com.mrjackspade.kairo.frontend.R as FrontendR
import com.mrjackspade.kairo.frontend.FirmwareSet
import com.mrjackspade.kairo.frontend.UiText
import java.io.File

/**
 * The player's Commodore firmware: the C64's KERNAL, BASIC and character ROMs, which a game set
 * to "Your C64 firmware" starts on (C64Catalog.system), and the 1541's DOS ROM, which a game set
 * to the 1541 drive runs (C64Catalog.drive). None is required: games start on the Open ROMs and
 * the virtual drive. A folder import finds each by the names dumps go by or by the SHA-1 of a
 * known dump.
 */
object C64Firmware {
    /** A part's description: its file [name], and what it is. */
    private fun description(name: String, what: UiText) = UiText.res(FrontendR.string.kairo_firmware_part, name, what)

    val kernal = FirmwareSet.Part("kernal.bin", UiText.fixed("KERNAL"),
        description("kernal.bin", UiText.res(R.string.firmware_kernal_description, "C64")),
        setOf("kernal.bin", "kernal", "kernal.rom", "kernal rom", "c64_kernal.bin", "kernal-901227-03.bin",
            "kernal-901227-02.bin", "kernal-901227-01.bin"), false, sizes = setOf(8192L),
        sha1 = setOf(
            "1d503e56df85a62fee696e7618dc5b4e781df1bb",  // 901227-03
            "0e2e4ee3f2d41f00bed72f9ab588b83e306fdb13",  // 901227-02
            "87cc04d61fc748b82df09856847bb5c2754a2033")) // 901227-01

    val basic = FirmwareSet.Part("basic.bin", UiText.fixed("BASIC"), description("basic.bin", UiText.fixed("BASIC V2")),
        setOf("basic.bin", "basic", "basic.rom", "basic rom", "c64_basic.bin", "basic-901226-01.bin"), false,
        sizes = setOf(8192L), sha1 = setOf("79015323128650c742a3694c9429aa91f355905e")) // 901226-01

    val chargen = FirmwareSet.Part("chargen.bin", UiText.res(R.string.firmware_character_set),
        description("chargen.bin", UiText.res(R.string.firmware_chargen_description, "C64")),
        setOf("chargen.bin", "chargen", "chargen.rom", "char rom", "c64_chargen.bin", "chargen-901225-01.bin"), false,
        sizes = setOf(4096L), sha1 = setOf("adc7c31e18c7c7413d54802ef2f4193da14711aa")) // 901225-01

    val dos1541 = FirmwareSet.Part("dos1541.bin", UiText.res(R.string.firmware_drive, "1541"),
        description("dos1541.bin", UiText.res(R.string.firmware_dos_description, "1541")),
        setOf("dos1541.bin", "dos1541", "1541 rom", "c64_drive-1541.bin", "dos1541-325302-01+901229-05.bin",
            "d1541ii", "c64_drive-1541ii.bin", "dos1541ii-251968-03.bin"), false, sizes = setOf(16384L),
        sha1 = setOf(
            "ab16f56989b27d89babe5f89c5a8cb3da71a82f0",  // 1541: 325302-01 + 901229-05
            "d3b78c3dbac55f5199f33f3fe0036439811f7fb3")) // 1541-II: 251968-03

    /** The ROMs a game set to the player's firmware starts on. */
    val system = listOf(kernal, basic, chargen)

    fun set(directory: File) = FirmwareSet(directory, system + dos1541)
}
