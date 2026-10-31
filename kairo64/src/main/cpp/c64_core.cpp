// The C64 machine (core/c64_machine.h) as the MachineCore the shared frame host runs
// (shared/frontend/src/main/cpp/kairo/machine_core.h).

#include <array>
#include <cctype>
#include <memory>
#include <string>
#include <vector>

#include "c64_machine.h"
#include "kairo/machine_core.h"

namespace {

// Whether path ends in extension (".dhd"), in any case.
bool has_extension(const std::string &path, const std::string &extension) {
    if (path.size() < extension.size()) return false;
    for (size_t i = 0; i < extension.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(path[path.size() - extension.size() + i])) != extension[i]) return false;
    return true;
}

using kairo64::C64Machine;

// Media slots, as MainActivity numbers them: drive 8.
static_assert(kairo64::kDrive8 == 0);
// MainActivity.machine.stateFile.
constexpr const char *kStateFile = "/state.vsf";
// MainActivity.stateError reads these.
// C64Keys numbers a key by its place in the matrix, row × 8 + column + 1 (1 to 64), and RESTORE 65.
constexpr int kRestore = 65;
constexpr int kKeyCodes = kRestore + 1;

class C64Core : public kairo::MachineCore {
public:
    explicit C64Core(std::unique_ptr<C64Machine> machine) : machine_(std::move(machine)) {}

    bool run_frame() override { return machine_->run_frame(error_); }
    // Flat out for as long as a load physically runs (C64Machine::loading).
    bool loading() override { return machine_->loading(); }
    std::string error() override { return error_; }
    // The emulator's reason, which save_state and load_state leave in error_.
    std::string state_error() override { return error_; }
    // What is loading, for tests and diagnostics: "load=autostart", "tape", "disk" or "none".
    std::string details() override {
        return std::string("load=") + (machine_->autostarting() ? "autostart"
                                       : machine_->tape_moving() ? "tape"
                                       : machine_->disk_loading() ? "disk" : "none");
    }
    const uint8_t *frame(uint32_t &width, uint32_t &height) override { return machine_->frame(width, height); }
    double frame_seconds() override { return machine_->frame_seconds(); }
    int32_t audio_rate() override { return machine_->sample_rate(); }
    uint32_t read_audio(int16_t *stereo, uint32_t max_frames) override { return machine_->read_audio(stereo, max_frames); }

    void release_inputs() override {
        for (int code = 1; code < kKeyCodes; ++code)
            if (keys_[code]) key(code, false);
        for (int control = 0; control < kairo64::kJoystickControls; ++control) machine_->joystick(control, false);
        machine_->mouse_button(1, false);
        machine_->mouse_button(2, false);
    }

    // The startup commands go in when the screen editor waits for a line (startup_typer.h), as
    // the C64's keys type them: key codes are row * 8 + column + 1 in the keyboard matrix (C64Keys).
    // Auto touch mode (Kairo#78): the screen editor waiting for a line is the keyboard; the guest
    // reading the 1351's position is the mouse. Each sample that finds the editor waiting counts as a wait.
    bool guest_input(GuestInput &input) override {
        input.keyboard_waiting = machine_->waits_for_line();
        if (input.keyboard_waiting) ++keyboard_waits_;
        input.keyboard_waits = keyboard_waits_;
        input.mouse_reads = machine_->mouse_reads();
        return true;
    }

    kairo::Prompt prompt() override { return machine_->waits_for_line() ? kairo::Prompt::Line : kairo::Prompt::None; }

    bool typed_key(char c, int &code, bool &shift) override {
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

    int special_key(kairo::SpecialKey key) override {
        switch (key) {
        case kairo::SpecialKey::Return: return 2;
        case kairo::SpecialKey::Shift: return 16;
        case kairo::SpecialKey::Ctrl: return 59;
        case kairo::SpecialKey::F1: return 5;
        case kairo::SpecialKey::F3: return 6;
        case kairo::SpecialKey::F5: return 7;
        // F2 and F4 are Shift with F1 and F3: a single key cannot type them.
        default: return -1;
        }
    }

    void key(int code, bool down) override {
        if (code <= 0 || code >= kKeyCodes || keys_[code] == down) return;
        keys_[code] = down;
        if (code == kRestore) {
            machine_->restore(down);
        } else {
            const int place = code - 1;
            machine_->key(place / kairo64::kMatrixColumns, place % kairo64::kMatrixColumns, down);
        }
    }

    // Controls in C64Keys.joystick order, whose first five are the core's.
    void joystick(int control, bool down) override { machine_->joystick(control, down); }
    // The 1351 mouse in port 1, for games set to it.
    void mouse_move(int dx, int dy) override { machine_->mouse_move(dx, dy); }
    void mouse_button(int button, bool down) override { machine_->mouse_button(button, down); }

    void reset() override { machine_->reset(); }

    int save_state(const std::string &directory) override {
        return machine_->save_state(directory + kStateFile, error_) ? 0 : kFailed;
    }

    // The emulator's state holds no disk image: the host records and restores them (machine_jni.cpp).
    bool media_in_state() override { return false; }
    int load_state(const std::string &directory) override {
        return machine_->load_state(directory + kStateFile, error_) ? 0 : kFailed;
    }

    void mouse_warp(int x, int y) override { machine_->mouse_warp(x, y); }
    bool mouse_warp_done() override { return machine_->mouse_warp_done(); }

    // Slots (MainActivity.mediaSlots): drive 8, the datasette, the cartridge port.
    int media(int slot, const std::string &path) override {
        if (slot == kTapeSlot) return machine_->insert_tape(path, error_) ? 0 : kFailed;
        if (slot == kCartridgeSlot) return machine_->insert_cartridge(path, error_) ? 0 : kFailed;
        if (slot == kHardDiskSlot) return machine_->insert_hard_disk(path, error_) ? 0 : kFailed;
        if (slot < 0 || slot >= kairo64::kDrives) return kUnsupported;
        return machine_->insert(kairo64::Drive(slot), path, error_) ? 0 : kFailed;
    }

    std::vector<std::string> media_paths() override {
        std::vector<std::string> paths;
        for (int drive = 0; drive < kairo64::kDrives; ++drive) paths.push_back(machine_->media_path(kairo64::Drive(drive)));
        paths.push_back(machine_->tape_path());
        paths.push_back(machine_->cartridge_path());
        paths.push_back(machine_->hard_disk_path());
        return paths;
    }

    static constexpr int kTapeSlot = kairo64::kDrives;
    static constexpr int kCartridgeSlot = kairo64::kDrives + 1;
    static constexpr int kHardDiskSlot = kairo64::kDrives + 2;

private:
    std::unique_ptr<C64Machine> machine_;
    std::string error_;
    std::array<bool, kKeyCodes> keys_{};
    // Samples that found the screen editor waiting for a line (guest_input).
    uint64_t keyboard_waits_ = 0;
};

} // namespace

// The launch's home holds "openroms", the Open ROMs MainActivity copies there, and "vice", VICE's
// own files; its firmware folder holds the player's ROMs under C64Firmware's names. Media: the
// game to start, then the disk in drive 8 ("" when the start is not a disk).
std::unique_ptr<kairo::MachineCore> kairo::create_core(const kairo::Launch &launch, std::string &error) {
    kairo64::Config config;
    config.home = launch.home + "/vice";
    if (launch.media.size() != 3) {
        error = "A C64 launch names the game, drive 8's disk and a hard disk; this one has " +
                std::to_string(launch.media.size()) + " media";
        return nullptr;
    }
    config.start = launch.media[0];
    config.drive8 = launch.media[1];
    // A hard disk on the interface its image is for, on the player's ROM for it (C64Firmware).
    config.hard_disk = launch.media[2];
    if (!config.hard_disk.empty()) {
        if (has_extension(config.hard_disk, ".dhd")) {
            config.cmdhd_rom = launch.firmware + "/cmdhd.bin";
        } else if (has_extension(config.hard_disk, ".hdd") || has_extension(config.hard_disk, ".cfa")) {
            config.ide64_rom = launch.firmware + "/ide64.bin";
        } else {
            error = "The hard disk " + config.hard_disk + " is not a CMD HD (.dhd) or IDE64 (.hdd, .cfa) image";
            return nullptr;
        }
    }
    // The program the start loads (C64Catalog.program): "" for its first.
    config.program = launch.settings.at("program");
    // The settings C64Catalog's options name; every launch passes them.
    config.model = launch.settings.at("model");
    const std::string &system = launch.settings.at("system");
    if (system == "openroms") {
        const std::string open = launch.home + "/openroms/";
        config.kernal = open + "kernal_generic.rom";
        config.basic = open + "basic_generic.rom";
        config.chargen = open + "chargen_openroms.rom";
        config.t64_on_disk = true;
    } else if (system == "own") {
        config.kernal = launch.firmware + "/kernal.bin";
        config.basic = launch.firmware + "/basic.bin";
        config.chargen = launch.firmware + "/chargen.bin";
    } else {
        error = "System is \"" + system + "\", not openroms or own";
        return nullptr;
    }
    const std::string &drive = launch.settings.at("drive");
    if (drive == "1541") {
        // Drive 8 is the CMD HD when the game is on one, whatever its drive setting.
        if (config.cmdhd_rom.empty()) config.dos1541 = launch.firmware + "/dos1541.bin";
    } else if (drive != "virtual") {
        error = "Drive is \"" + drive + "\", not 1541 or virtual";
        return nullptr;
    }
    const std::string &ports = launch.settings.at("ports");
    if (ports == "mouse") {
        config.mouse = true;
        config.joystick_port = 2;
    } else if (ports == "1" || ports == "2") {
        config.joystick_port = std::stoi(ports);
    } else {
        error = "Controls are \"" + ports + "\", not 1, 2 or mouse";
        return nullptr;
    }
    auto machine = C64Machine::create(config, error);
    if (!machine) return nullptr;
    return std::make_unique<C64Core>(std::move(machine));
}

// The blank disks MainActivity.blankDisks lists: "d64", a 1541 floppy.
std::string kairo::create_blank_disk(const std::string &kind, const std::string &path) {
    if (kind != "d64") return "Kairo64 makes no blank disk " + kind;
    std::string error;
    return C64Machine::create_blank_disk(path, error) ? "" : error;
}
