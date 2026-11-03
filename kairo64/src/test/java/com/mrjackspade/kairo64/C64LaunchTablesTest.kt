package com.mrjackspade.kairo64

import com.mrjackspade.kairo.LaunchTablesTest
import java.io.File

/** What Kairo64 builds as it launches (Kairo#125, Kairo64#59). */
class C64LaunchTablesTest : LaunchTablesTest() {
    override fun tables() = LaunchTables(listOf(C64Media.format), listOf(C64Keys.layout),
        listOf(C64Firmware.set(File("firmware"))), mapOf("controller bindings" to { C64Keys.bindings }))
}
