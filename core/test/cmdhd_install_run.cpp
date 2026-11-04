// The CMD HD image maker's runner (tools/make_cmdhd_image.py): a C64 with a CMD HD in drive 8 on its
// boot ROM, holding the image to set up, and VICE's virtual drive 9 holding CMD's HD Utilities
// disk, run through a script of typed lines and waits, as a person installs the HD's system
// (CREATE SYS) and partitions it (HD-TOOLS) at the keyboard.
//
// Usage: cmdhd_install_run <home> <kernal> <basic> <chargen> <cmdhd rom> <image.dhd> <utilities.d64> <script>
//
// The script has one step a line ("#" starts a comment):
//   type <text>           types text; {RETURN}, {F1}, {F3}, {F5} are those keys, {DOWN} and
//                         {RIGHT} the cursor keys, {UP} and {LEFT} them with Shift
//   wait <frames> <text>  runs until the text screen shows text, at most frames frames
//   run <frames>          runs frames frames
//   reset <unit> <bits>   resets the drive of unit with the buttons bits held (VICE's
//                         DRIVE_BUTTON_*: 1 Write Protect, 2 Swap 8, 4 Swap 9; 0 a plain reset)
//   screen                prints the text screen
// It prints the text screen after each wait and at the end, and fails (exit 1) when a wait's text
// never shows, printing the screen it got. Exit 2 is a bad argument or a machine that cannot start.

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "c64_keys.h"
#include "c64_machine.h"
#include "kairo/kairo_c64.h"

namespace {

using kairo64::C64Machine;

[[noreturn]] void fail(int code, const std::string &message) {
    std::fprintf(stderr, "%s\n", message.c_str());
    std::exit(code);
}

void run(C64Machine &machine, int frames) {
    std::string error;
    for (int i = 0; i < frames; ++i)
        if (!machine.run_frame(error)) fail(1, "the machine stopped: " + error);
}

void print_screen(C64Machine &machine) {
    std::printf("----\n%s\n", machine.screen_text().c_str());
    std::fflush(stdout);
}

// A key by its C64Keys code (row × 8 + column + 1), held then let go, with Shift if asked.
void press(C64Machine &machine, int code, bool shift) {
    const auto set = [&](int key, bool down) { machine.key((key - 1) / 8, (key - 1) % 8, down); };
    if (shift) set(kairo64::kKeyShift, true);
    set(code, true);
    run(machine, 3);
    set(code, false);
    if (shift) set(kairo64::kKeyShift, false);
    run(machine, 3);
}

// The cursor keys: Down and Right, Up and Left with Shift.
constexpr int kKeyCursorDown = 8, kKeyCursorRight = 3;

void type(C64Machine &machine, const std::string &text) {
    for (size_t at = 0; at < text.size(); ++at) {
        if (text[at] == '{') {
            const size_t end = text.find('}', at);
            if (end == std::string::npos) fail(2, "an unclosed { in: " + text);
            const std::string name = text.substr(at + 1, end - at - 1);
            if (name == "RETURN") press(machine, kairo64::kKeyReturn, false);
            else if (name == "F1") press(machine, kairo64::kKeyF1, false);
            else if (name == "F3") press(machine, kairo64::kKeyF3, false);
            else if (name == "F5") press(machine, kairo64::kKeyF5, false);
            else if (name == "DOWN") press(machine, kKeyCursorDown, false);
            else if (name == "UP") press(machine, kKeyCursorDown, true);
            else if (name == "RIGHT") press(machine, kKeyCursorRight, false);
            else if (name == "LEFT") press(machine, kKeyCursorRight, true);
            else fail(2, "no key {" + name + "}");
            at = end;
            continue;
        }
        int code = 0;
        bool shift = false;
        if (!kairo64::typed_key(text[at], code, shift)) fail(2, std::string("no key types '") + text[at] + "'");
        press(machine, code, shift);
    }
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 9) fail(2, "usage: cmdhd_install_run <home> <kernal> <basic> <chargen> <cmdhd rom> <image.dhd> <utilities.d64> <script>");
    kairo64::Config config;
    config.home = argv[1];
    // The same image from the same script each time.
    config.repeatable = true;
    config.kernal = argv[2];
    config.basic = argv[3];
    config.chargen = argv[4];
    config.cmdhd_rom = argv[5];
    config.hard_disk = argv[6];
    std::string error;
    auto machine = C64Machine::create(config, error);
    if (!machine) fail(2, "the machine did not start: " + error);
    // Drive 9: VICE's virtual drive, as drive 8 is without a 1541 (C64Machine's configure).
    const KairoC64Api &vice = machine->vice();
    for (const auto &[name, value] : {std::pair<const char *, int>{"TrapDevice9", 0}, {"Drive9TrueEmulation", 0},
                                      {"Drive9Type", 0}, {"BusDevice9", 1}})
        if (!vice.set_int(name, value)) fail(2, std::string("VICE refused ") + name + " = " + std::to_string(value));
    if (!vice.attach_disk(9, argv[7])) fail(2, std::string("VICE refused the utilities disk ") + argv[7]);

    std::ifstream script(argv[8]);
    if (!script) fail(2, std::string("no script ") + argv[8]);
    std::string line;
    int number = 0;
    while (std::getline(script, line)) {
        ++number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        std::istringstream words(line);
        std::string step;
        words >> step;
        const auto rest = [&]() {
            std::string text;
            std::getline(words, text);
            return text.empty() ? text : text.substr(1);
        };
        if (step == "type") {
            type(*machine, rest());
        } else if (step == "run") {
            int frames = 0;
            words >> frames;
            run(*machine, frames);
        } else if (step == "wait") {
            int frames = 0;
            words >> frames;
            const std::string text = rest();
            int waited = 0;
            while (machine->screen_text().find(text) == std::string::npos) {
                if (waited++ >= frames) {
                    print_screen(*machine);
                    fail(1, "line " + std::to_string(number) + ": \"" + text + "\" did not show in " + std::to_string(frames) + " frames");
                }
                run(*machine, 1);
            }
            std::printf("line %d: \"%s\" after %d frames\n", number, text.c_str(), waited);
            print_screen(*machine);
        } else if (step == "reset") {
            int unit = 0;
            unsigned bits = 0;
            words >> unit >> bits;
            if (!vice.reset_drive(unit, bits)) fail(2, "line " + std::to_string(number) + ": no drive " + std::to_string(unit));
        } else if (step == "screen") {
            print_screen(*machine);
        } else {
            fail(2, "line " + std::to_string(number) + ": no step " + step);
        }
    }
    print_screen(*machine);
    return 0;
}
