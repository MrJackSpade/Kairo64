package com.mrjackspade.kairo64

import com.mrjackspade.kairo.frontend.DiskImageLibrary
import com.mrjackspade.kairo.frontend.UiText
import com.mrjackspade.kairo.frontend.R as FrontendR

/** The images Kairo64's library reads. */
object C64Media {
    /**
     * Disk, tape, program and cartridge images, which VICE's autostart starts (c64_machine.h), and
     * the hard disk images of a CMD HD (.dhd) and of IDE64 (.hdd, .cfa).
     */
    val format = DiskImageLibrary.Format(
        kinds = listOf("d64", "g64", "d71", "d81").associateWith { "disk" } +
            listOf("t64", "tap").associateWith { "tape" } +
            listOf("prg", "p00").associateWith { "prg" } +
            mapOf("crt" to "cart") + mapOf("dhd" to "cmdhd", "hdd" to "ide64", "cfa" to "ide64"),
        labels = mapOf("disk" to UiText.res(R.string.disk), "tape" to UiText.res(R.string.tape),
            "prg" to UiText.res(FrontendR.string.kairo_program), "cart" to UiText.res(FrontendR.string.kairo_cartridge),
            "cmdhd" to UiText.res(FrontendR.string.kairo_hard_disk), "ide64" to UiText.res(FrontendR.string.kairo_hard_disk)),
        maxBytes = mapOf("disk" to 2L * 1024 * 1024, "tape" to 64L * 1024 * 1024, "prg" to 64L * 1024,
            "cart" to 16L * 1024 * 1024, "cmdhd" to 4L * 1024 * 1024 * 1024, "ide64" to 4L * 1024 * 1024 * 1024),
        guestName = "C64")
}
