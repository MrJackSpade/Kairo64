// The C64's keys for typed text: which key in the keyboard matrix types a character, numbered as
// C64Keys numbers them (row × 8 + column + 1, 1 to 64; RESTORE is 65). The app's startup commands
// (c64_core.cpp) and host tools that type at BASIC (core/test/cmdhd_install_run.cpp) use it.
#pragma once

#include <cctype>
#include <string>

namespace kairo64 {

constexpr int kKeyReturn = 2;
constexpr int kKeyF1 = 5;
constexpr int kKeyF3 = 6;
constexpr int kKeyF5 = 7;
constexpr int kKeyShift = 16;
constexpr int kKeyCtrl = 59;
constexpr int kKeyRestore = 65;

// The key that types c, and whether Shift goes with it; false for a character no key types.
inline bool typed_key(char c, int &code, bool &shift) {
    static const std::string letters = "abcdefghijklmnopqrstuvwxyz";
    // Each letter's matrix place: A is row 1 column 2, B row 3 column 4, and so on.
    static const int letter_codes[26] = {11, 29, 21, 19, 15, 22, 27, 30, 34, 35, 38, 43, 37,
                                         40, 39, 42, 63, 18, 14, 23, 31, 32, 10, 24, 26, 13};
    static const std::string digits = "1234567890", shifted_digits = "!\"#$%&'()";
    static const int digit_codes[10] = {57, 60, 9, 12, 17, 20, 25, 28, 33, 36};
    shift = false;
    const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (auto at = letters.find(lower); at != std::string::npos) { code = letter_codes[at]; return true; }
    if (auto at = digits.find(c); at != std::string::npos) { code = digit_codes[at]; return true; }
    if (auto at = shifted_digits.find(c); at != std::string::npos) { code = digit_codes[at]; shift = true; return true; }
    switch (c) {
    case ' ': code = 61; return true;
    case '+': code = 41; return true;
    case '-': code = 44; return true;
    case '.': code = 45; return true;
    case ':': code = 46; return true;
    case '@': code = 47; return true;
    case ',': code = 48; return true;
    case '*': code = 50; return true;
    case ';': code = 51; return true;
    case '=': code = 54; return true;
    case '^': code = 55; return true;
    case '/': code = 56; return true;
    case '_': code = 58; return true;
    case '[': code = 46; shift = true; return true;
    case ']': code = 51; shift = true; return true;
    case '>': code = 45; shift = true; return true;
    case '<': code = 48; shift = true; return true;
    case '?': code = 56; shift = true; return true;
    default: return false;
    }
}

} // namespace kairo64
