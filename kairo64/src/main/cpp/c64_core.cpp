// The C64 machine (core/c64_machine.h) as the MachineCore the shared frame host runs
// (shared/frontend/src/main/cpp/kairo/machine_core.h).

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "c64_machine.h"
#include "kairo/machine_core.h"

namespace {

using kairo64::C64Machine;

// Media slots, as MainActivity numbers them: drive 8.
static_assert(kairo64::kDrive8 == 0);
// MainActivity.machine.stateFile.
constexpr const char *kStateFile = "/state.vsf";
// MainActivity.stateError reads these.
constexpr int kStateFailed = 2;
constexpr int kMediaRefused = 3;
// C64Keys numbers a key by its place in the matrix, row × 8 + column + 1 (1 to 64), and RESTORE 65.
constexpr int kRestore = 65;
constexpr int kKeyCodes = kRestore + 1;

class C64Core : public kairo::MachineCore {
public:
    explicit C64Core(std::unique_ptr<C64Machine> machine) : machine_(std::move(machine)) {}

    bool run_frame() override { return machine_->run_frame(error_); }
    // Flat out for as long as a load physically runs (C64Machine::loading).
    bool booting() override { return machine_->loading(); }
    std::string error() override { return error_; }
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
        return machine_->save_state(directory + kStateFile, error_) ? 0 : kStateFailed;
    }

    int load_state(const std::string &directory) override {
        return machine_->load_state(directory + kStateFile, error_) ? 0 : kStateFailed;
    }

    int media(int slot, const std::string &path) override {
        if (slot < 0 || slot >= kairo64::kDrives) return kUnsupported;
        return machine_->insert(kairo64::Drive(slot), path, error_) ? 0 : kMediaRefused;
    }

    std::vector<std::string> media_paths() override {
        std::vector<std::string> paths;
        for (int drive = 0; drive < kairo64::kDrives; ++drive) paths.push_back(machine_->media_path(kairo64::Drive(drive)));
        return paths;
    }

private:
    std::unique_ptr<C64Machine> machine_;
    std::string error_;
    std::array<bool, kKeyCodes> keys_{};
};

} // namespace

// The launch's home holds "openroms", the Open ROMs MainActivity copies there, and "vice", VICE's
// own files; its firmware folder holds the player's ROMs under C64Firmware's names. Media: the
// game to start, then the disk in drive 8 ("" when the start is not a disk).
std::unique_ptr<kairo::MachineCore> kairo::create_core(const kairo::Launch &launch, std::string &error) {
    kairo64::Config config;
    config.home = launch.home + "/vice";
    if (launch.media.size() != 2) {
        error = "A C64 launch names the game and drive 8's disk; this one has " + std::to_string(launch.media.size()) + " media";
        return nullptr;
    }
    config.start = launch.media[0];
    config.drive8 = launch.media[1];
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
        config.dos1541 = launch.firmware + "/dos1541.bin";
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
