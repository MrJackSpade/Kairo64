package com.mrjackspade.kairo64

import org.junit.Assert.assertEquals
import org.junit.Test

class C64MediaTest {
    /** The format the app builds as it starts: each kind needs a label and a size, or the app can't open. */
    @Test
    fun everyKindIsComplete() {
        val format = C64Media.format
        assertEquals("disk", format.kind("Game.d64"))
        assertEquals("cmdhd", format.kind("Game.dhd"))
        assertEquals("ide64", format.kind("Game.hdd"))
        assertEquals("ide64", format.kind("Game.cfa"))
        for (kind in format.kinds.values) {
            assertEquals(kind, true, kind in format.labels)
            assertEquals(kind, true, kind in format.maxBytes)
        }
    }
}
