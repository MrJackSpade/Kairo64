package com.mrjackspade.kairo64

import android.view.KeyEvent
import com.mrjackspade.kairo.frontend.ControllerBinding
import com.mrjackspade.kairo.frontend.ControllerBindingsCodec
import com.mrjackspade.kairo.frontend.ControllerLayout
import com.mrjackspade.kairo.frontend.GuestKeyboardLayout
import com.mrjackspade.kairo.frontend.GuestKeyboardPage
import com.mrjackspade.kairo.frontend.JoystickInputRouter
import com.mrjackspade.kairo.frontend.KeyboardKey
import com.mrjackspade.kairo.frontend.SymbolPage

/**
 * The C64 keyboard by its place in the keyboard matrix: a key's code is row × 8 + column + 1
 * (1 to 64, the core's rows and columns, c64_core.cpp), and RESTORE, which is wired to the NMI
 * instead, is 65. The C64 has two cursor keys, ↓ and →; Shift turns them up and left, as it turns
 * F1, F3, F5 and F7 into F2, F4, F6 and F8.
 */
object C64Keys {
    private fun key(row: Int, column: Int) = row * 8 + column + 1

    const val DELETE = 1          // INST/DEL: row 0, column 0
    const val RETURN = 2
    const val CURSOR_RIGHT = 3
    const val F7 = 4
    const val F1 = 5
    const val F3 = 6
    const val F5 = 7
    const val CURSOR_DOWN = 8
    const val LEFT_SHIFT = 16     // row 1, column 7
    const val PLUS = 41           // row 5
    const val MINUS = 44
    const val PERIOD = 45
    const val COLON = 46
    const val AT = 47
    const val COMMA = 48
    const val POUND = 49          // row 6
    const val ASTERISK = 50
    const val SEMICOLON = 51
    const val CLR_HOME = 52
    const val RIGHT_SHIFT = 53
    const val EQUALS = 54
    const val UP_ARROW = 55
    const val SLASH = 56
    const val LEFT_ARROW = 58     // row 7
    const val CONTROL = 59
    const val SPACE = 61
    const val COMMODORE = 62
    const val RUN_STOP = 64
    const val RESTORE = 65
    const val MAX_CODE = 65

    /** Letters and digits by their places in the matrix. */
    private val letterCodes: Map<Char, Int> = mapOf(
        'w' to key(1, 1), 'a' to key(1, 2), 'z' to key(1, 4), 's' to key(1, 5), 'e' to key(1, 6),
        'r' to key(2, 1), 'd' to key(2, 2), 'c' to key(2, 4), 'f' to key(2, 5), 't' to key(2, 6), 'x' to key(2, 7),
        'y' to key(3, 1), 'g' to key(3, 2), 'b' to key(3, 4), 'h' to key(3, 5), 'u' to key(3, 6), 'v' to key(3, 7),
        'i' to key(4, 1), 'j' to key(4, 2), 'm' to key(4, 4), 'k' to key(4, 5), 'o' to key(4, 6), 'n' to key(4, 7),
        'p' to key(5, 1), 'l' to key(5, 2), 'q' to key(7, 6))
    private val digitCodes: Map<Char, Int> = mapOf(
        '1' to key(7, 0), '2' to key(7, 3), '3' to key(1, 0), '4' to key(1, 3), '5' to key(2, 0),
        '6' to key(2, 3), '7' to key(3, 0), '8' to key(3, 3), '9' to key(4, 0), '0' to key(4, 3))
    private fun letter(char: Char) = letterCodes.getValue(char.lowercaseChar())
    private fun digit(char: Char) = digitCodes.getValue(char)

    private const val DIGITS = "1234567890"
    /** What Shift types on each digit key: the C64's own legends. */
    private const val SHIFTED_DIGITS = "!\"#$%&'()0"

    val layout = GuestKeyboardLayout(
        brand = "KAIRO64",
        pages = listOf(
            GuestKeyboardPage("ABC", alphabetRows()),
            GuestKeyboardPage("?123", symbolRows(), pickerLabel = "Symbols"),
            GuestKeyboardPage("C64", c64Rows())
        ),
        modifiers = setOf(LEFT_SHIFT, RIGHT_SHIFT, CONTROL, COMMODORE),
        shiftCodes = setOf(LEFT_SHIFT, RIGHT_SHIFT),
        capsCode = null,
        chordShiftCode = LEFT_SHIFT
    )

    /** The C64's own arrangement: digits with ← and + − £ above, @ * ↑ by P, : ; = by L. */
    private fun alphabetRows(): List<List<KeyboardKey>> = listOf(
        listOf(KeyboardKey("←", LEFT_ARROW)) +
            DIGITS.zip(SHIFTED_DIGITS).map { (char, shifted) -> KeyboardKey("$char", digit(char), shifted = "$shifted") } +
            listOf(KeyboardKey("+", PLUS), KeyboardKey("−", MINUS), KeyboardKey("£", POUND),
                KeyboardKey("Del", DELETE, 1.5f, shifted = "Inst")),
        listOf(KeyboardKey("Ctrl", CONTROL, 1.3f)) + "qwertyuiop".map { KeyboardKey("${it.uppercaseChar()}", letter(it)) } +
            listOf(KeyboardKey("@", AT), KeyboardKey("*", ASTERISK), KeyboardKey("↑", UP_ARROW, shifted = "π")),
        listOf(KeyboardKey("Stop", RUN_STOP, 1.4f, shifted = "Run")) +
            "asdfghjkl".map { KeyboardKey("${it.uppercaseChar()}", letter(it)) } +
            listOf(KeyboardKey(":", COLON, shifted = "["), KeyboardKey(";", SEMICOLON, shifted = "]"),
                KeyboardKey("=", EQUALS), KeyboardKey("Return", RETURN, 1.6f)),
        listOf(KeyboardKey("C=", COMMODORE), KeyboardKey("Shift", LEFT_SHIFT, 1.5f)) +
            "zxcvbnm".map { KeyboardKey("${it.uppercaseChar()}", letter(it)) } +
            listOf(KeyboardKey(",", COMMA, shifted = "<"), KeyboardKey(".", PERIOD, shifted = ">"),
                KeyboardKey("/", SLASH, shifted = "?"), KeyboardKey("Shift", RIGHT_SHIFT, 1.5f)),
        listOf(KeyboardKey("Home", CLR_HOME, shifted = "Clr"), KeyboardKey("Space", SPACE, 6f),
            KeyboardKey("←", CURSOR_RIGHT, chordShift = true), KeyboardKey("↑", CURSOR_DOWN, chordShift = true),
            KeyboardKey("↓", CURSOR_DOWN), KeyboardKey("→", CURSOR_RIGHT))
    )

    /** Each symbol on its own key, Shift chorded where the C64 types it shifted (SymbolPage). */
    private fun symbolRows(): List<List<KeyboardKey>> = SymbolPage.rows(
        digits = DIGITS.map { KeyboardKey("$it", digit(it)) },
        delete = KeyboardKey("Del", DELETE, 1.5f),
        shiftedDigits = DIGITS.zip(SHIFTED_DIGITS).dropLast(1)
            .map { (char, symbol) -> KeyboardKey("$symbol", digit(char), chordShift = true) },
        symbols = listOf("+" to PLUS, "−" to MINUS, "£" to POUND, "@" to AT, "*" to ASTERISK, "↑" to UP_ARROW,
            ":" to COLON, ";" to SEMICOLON, "=" to EQUALS, "," to COMMA, "." to PERIOD, "/" to SLASH)
            .map { (label, code) -> KeyboardKey(label, code) },
        shiftedSymbols = listOf("[" to COLON, "]" to SEMICOLON, "<" to COMMA, ">" to PERIOD, "?" to SLASH, "π" to UP_ARROW)
            .map { (label, code) -> KeyboardKey(label, code, chordShift = true) } +
            KeyboardKey("←", LEFT_ARROW),
        space = KeyboardKey("Space", SPACE, 5f),
        enter = KeyboardKey("Return", RETURN, 1.5f)
    )

    /** The function keys, RUN/STOP, RESTORE and the cursor keys. */
    private fun c64Rows(): List<List<KeyboardKey>> = listOf(
        listOf(KeyboardKey("F1", F1), KeyboardKey("F2", F1, chordShift = true), KeyboardKey("F3", F3),
            KeyboardKey("F4", F3, chordShift = true), KeyboardKey("F5", F5), KeyboardKey("F6", F5, chordShift = true),
            KeyboardKey("F7", F7), KeyboardKey("F8", F7, chordShift = true)),
        listOf(KeyboardKey("Run/Stop", RUN_STOP, 1.5f), KeyboardKey("Restore", RESTORE, 1.5f),
            KeyboardKey("Clr/Home", CLR_HOME, 1.5f), KeyboardKey("Inst/Del", DELETE, 1.5f)),
        listOf(KeyboardKey("Ctrl", CONTROL), KeyboardKey("C=", COMMODORE), KeyboardKey("Shift", LEFT_SHIFT),
            KeyboardKey("←", CURSOR_RIGHT, chordShift = true), KeyboardKey("↑", CURSOR_DOWN, chordShift = true),
            KeyboardKey("↓", CURSOR_DOWN), KeyboardKey("→", CURSOR_RIGHT))
    )

    /** Every key a controller binding can send. */
    val bindableCodes: List<Int> = (1..MAX_CODE).filter { code -> label(code) != null }

    fun label(code: Int): String? = when (code) {
        DELETE -> "Inst/Del"
        RETURN -> "Return"
        CURSOR_RIGHT -> "Cursor →"
        F7 -> "F7"
        F1 -> "F1"
        F3 -> "F3"
        F5 -> "F5"
        CURSOR_DOWN -> "Cursor ↓"
        LEFT_SHIFT -> "Left Shift"
        PLUS -> "+"
        MINUS -> "−"
        PERIOD -> "."
        COLON -> ":"
        AT -> "@"
        COMMA -> ","
        POUND -> "£"
        ASTERISK -> "*"
        SEMICOLON -> ";"
        CLR_HOME -> "Clr/Home"
        RIGHT_SHIFT -> "Right Shift"
        EQUALS -> "="
        UP_ARROW -> "↑"
        SLASH -> "/"
        LEFT_ARROW -> "←"
        CONTROL -> "Ctrl"
        SPACE -> "Space"
        COMMODORE -> "C="
        RUN_STOP -> "Run/Stop"
        RESTORE -> "Restore"
        in letterCodes.values -> "${letterCodes.entries.first { it.value == code }.key.uppercaseChar()}"
        in digitCodes.values -> "${digitCodes.entries.first { it.value == code }.key}"
        else -> null
    }

    /**
     * The C64 key an Android key types, by its legend: Escape is RUN/STOP, Tab is CTRL, the
     * Windows or Alt keys are C=, Page Up is RESTORE, Home is CLR/HOME, and Backspace and Insert
     * are INST/DEL. Up and left are the C64's ↓ and → keys with Shift, a chord one key code cannot
     * send: the panel's and controllers' ↑ and ← keys type them.
     */
    fun fromAndroid(key: Int): Int? {
        if (key in KeyEvent.KEYCODE_A..KeyEvent.KEYCODE_Z) return letter('a' + (key - KeyEvent.KEYCODE_A))
        if (key in KeyEvent.KEYCODE_0..KeyEvent.KEYCODE_9) return digit('0' + (key - KeyEvent.KEYCODE_0))
        return when (key) {
            KeyEvent.KEYCODE_ESCAPE -> RUN_STOP
            KeyEvent.KEYCODE_TAB, KeyEvent.KEYCODE_CTRL_LEFT, KeyEvent.KEYCODE_CTRL_RIGHT -> CONTROL
            KeyEvent.KEYCODE_ALT_LEFT, KeyEvent.KEYCODE_ALT_RIGHT, KeyEvent.KEYCODE_META_LEFT,
            KeyEvent.KEYCODE_META_RIGHT -> COMMODORE
            KeyEvent.KEYCODE_SHIFT_LEFT -> LEFT_SHIFT
            KeyEvent.KEYCODE_SHIFT_RIGHT -> RIGHT_SHIFT
            KeyEvent.KEYCODE_ENTER, KeyEvent.KEYCODE_NUMPAD_ENTER -> RETURN
            KeyEvent.KEYCODE_DEL, KeyEvent.KEYCODE_INSERT, KeyEvent.KEYCODE_FORWARD_DEL -> DELETE
            KeyEvent.KEYCODE_SPACE -> SPACE
            KeyEvent.KEYCODE_MINUS, KeyEvent.KEYCODE_NUMPAD_SUBTRACT -> MINUS
            KeyEvent.KEYCODE_EQUALS -> EQUALS
            KeyEvent.KEYCODE_PLUS, KeyEvent.KEYCODE_NUMPAD_ADD -> PLUS
            KeyEvent.KEYCODE_LEFT_BRACKET -> AT
            KeyEvent.KEYCODE_RIGHT_BRACKET, KeyEvent.KEYCODE_NUMPAD_MULTIPLY -> ASTERISK
            KeyEvent.KEYCODE_SEMICOLON -> COLON
            KeyEvent.KEYCODE_APOSTROPHE -> SEMICOLON
            KeyEvent.KEYCODE_BACKSLASH -> POUND
            KeyEvent.KEYCODE_GRAVE -> LEFT_ARROW
            KeyEvent.KEYCODE_COMMA -> COMMA
            KeyEvent.KEYCODE_PERIOD, KeyEvent.KEYCODE_NUMPAD_DOT -> PERIOD
            KeyEvent.KEYCODE_SLASH, KeyEvent.KEYCODE_NUMPAD_DIVIDE -> SLASH
            KeyEvent.KEYCODE_MOVE_HOME -> CLR_HOME
            KeyEvent.KEYCODE_PAGE_UP -> RESTORE
            KeyEvent.KEYCODE_PAGE_DOWN -> UP_ARROW
            KeyEvent.KEYCODE_F1 -> F1
            KeyEvent.KEYCODE_F3 -> F3
            KeyEvent.KEYCODE_F5 -> F5
            KeyEvent.KEYCODE_F7 -> F7
            KeyEvent.KEYCODE_DPAD_DOWN -> CURSOR_DOWN
            KeyEvent.KEYCODE_DPAD_RIGHT -> CURSOR_RIGHT
            else -> null
        }
    }

    /**
     * The joystick's controls in the router's order. The core's joystick (c64_machine.h) has the
     * first five; the C64 joystick has one fire button, so "button2" reaches nothing.
     */
    val joystick = JoystickInputRouter.DEFAULT_CONTROLS
    val joystickLabels = listOf("Up", "Down", "Left", "Right", "Fire", "Fire 2 (unused)")

    val bindings = ControllerBindingsCodec(::defaults, { it in bindableCodes }, joystick,
        setOf("menu", "pause", "restart", "exit", "fastForward"))

    fun defaults(layout: ControllerLayout): List<ControllerBinding> =
        if (layout == ControllerLayout.WITH_STICKS) defaults()
        else defaults().filterNot { it.input.startsWith("virtual:ls") || it.input.startsWith("virtual:rs") }

    /**
     * C64 games read the joystick and a few keys: the D-pad is the joystick and the cursor keys
     * (up and left are Shift with CRSR down and right, as on the C64), the left stick is the
     * joystick, A and B are Return and Space, X and L1 fire, Y is RUN/STOP, Start is F1 (many
     * games' start), Select is F3, R1 is F7, L2 is RESTORE and R2 is F5; the right stick moves
     * the 1351 mouse.
     */
    fun defaults() = listOf(
        ControllerBinding("virtual:a", listOf(RETURN)),
        ControllerBinding("virtual:b", listOf(SPACE)),
        ControllerBinding("virtual:x", joystick = "button1"),
        ControllerBinding("virtual:y", listOf(RUN_STOP)),
        ControllerBinding("virtual:l1", joystick = "button1"),
        ControllerBinding("virtual:r1", listOf(F7)),
        ControllerBinding("virtual:l2", listOf(RESTORE)),
        ControllerBinding("virtual:r2", listOf(F5)),
        ControllerBinding("virtual:start", listOf(F1)),
        ControllerBinding("virtual:select", listOf(F3)),
        ControllerBinding("virtual:menu", action = "menu"),
        ControllerBinding("virtual:up", listOf(LEFT_SHIFT, CURSOR_DOWN), joystick = "up"),
        ControllerBinding("virtual:down", listOf(CURSOR_DOWN), joystick = "down"),
        ControllerBinding("virtual:left", listOf(LEFT_SHIFT, CURSOR_RIGHT), joystick = "left"),
        ControllerBinding("virtual:right", listOf(CURSOR_RIGHT), joystick = "right"),
        ControllerBinding("virtual:lsup", joystick = "up"),
        ControllerBinding("virtual:lsdown", joystick = "down"),
        ControllerBinding("virtual:lsleft", joystick = "left"),
        ControllerBinding("virtual:lsright", joystick = "right"),
        ControllerBinding("virtual:rsup", mouse = "moveUp"),
        ControllerBinding("virtual:rsdown", mouse = "moveDown"),
        ControllerBinding("virtual:rsleft", mouse = "moveLeft"),
        ControllerBinding("virtual:rsright", mouse = "moveRight")
    )
}
