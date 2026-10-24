package com.mrjackspade.kairo64

import android.view.KeyEvent
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

class C64KeysTest {
    /** The 64 matrix places and RESTORE, each a key of its own. */
    @Test
    fun everyMatrixPlaceHasOneLabelledKey() {
        for (code in 1..C64Keys.MAX_CODE) assertNotNull("code $code", C64Keys.label(code))
        assertEquals((1..C64Keys.MAX_CODE).toList(), C64Keys.bindableCodes)
    }

    @Test
    fun lettersAndDigitsTypeDistinctKeys() {
        val letters = (KeyEvent.KEYCODE_A..KeyEvent.KEYCODE_Z).map { C64Keys.fromAndroid(it)!! }
        val digits = (KeyEvent.KEYCODE_0..KeyEvent.KEYCODE_9).map { C64Keys.fromAndroid(it)!! }
        assertEquals(26, letters.toSet().size)
        assertEquals(10, digits.toSet().size)
        assertTrue((letters + digits).toSet().size == 36)
        assertTrue((letters + digits).all { it in 1..64 })
    }

    /** The matrix as the C64's keyboard is wired: A at row 1, column 2; Q at row 7, column 6. */
    @Test
    fun matrixPlacesMatchTheC64() {
        assertEquals(1 * 8 + 2 + 1, C64Keys.fromAndroid(KeyEvent.KEYCODE_A))
        assertEquals(7 * 8 + 6 + 1, C64Keys.fromAndroid(KeyEvent.KEYCODE_Q))
        assertEquals(7 * 8 + 0 + 1, C64Keys.fromAndroid(KeyEvent.KEYCODE_1))
        assertEquals(C64Keys.RUN_STOP, 7 * 8 + 7 + 1)
        assertEquals(C64Keys.RETURN, 0 * 8 + 1 + 1)
    }

    @Test
    fun defaultBindingsAreValid() {
        assertTrue(C64Keys.defaults().all { binding -> binding.keys.all { it in C64Keys.bindableCodes } })
    }
}
