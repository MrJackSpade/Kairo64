#include "c64_machine.h"

#include <cctype>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "kairo_c64.h"

#ifdef __ANDROID__
#include <android/log.h>
#else
#include <cstdio>
#endif

namespace kairo64 {

namespace {

// VICE, for the life of the process. It runs on its own thread and the host on the caller's;
// exactly one of them runs at a time: the host lets VICE run until the end of its next frame, and
// VICE waits there until the host lets it run again. The mutex hand-off orders every access to
// VICE's state.
struct Vice {
    enum class Turn { kVice, kHost };

    std::mutex mutex;
    std::condition_variable changed;
    Turn turn = Turn::kVice;
    bool started = false;
    // VICE's thread has returned from KairoC64_Run: VICE cannot run again in this process.
    bool ended = false;
    // Why VICE ended (KairoC64Host_Fatal), on VICE's thread before it returns.
    std::string fatal;
    // The last error VICE logged, which says more than an exit code.
    std::string last_error;
    // The running machine's CPU stopped (KairoC64Host_Jam), during its frame.
    std::string jam;
    // A machine is running.
    bool machine = false;
    // VICE's folder for its cache, configuration and state: the home folder of the machine it
    // started with, kept for VICE's life.
    std::string user_dir;
    // VICE is starting or a machine is being set up, through its first frame: VICE logs errors
    // for the ROMs its defaults and models name before Kairo64 names its own, and for the KERNAL
    // traps it cannot install on a KERNAL other than Commodore's as the machine resets, so its log
    // lines stay in its log file (KairoC64Host_Log) and in last_error, which a failure reports.
    bool quiet = true;

    // Lets VICE run until the end of its next frame or until it ends; false when it ended.
    bool run() {
        std::unique_lock lock(mutex);
        turn = Turn::kVice;
        changed.notify_all();
        changed.wait(lock, [&] { return turn == Turn::kHost || ended; });
        return !ended;
    }

    // On VICE's thread, at the end of a frame.
    void at_vbl() {
        std::unique_lock lock(mutex);
        turn = Turn::kHost;
        changed.notify_all();
        changed.wait(lock, [&] { return turn == Turn::kVice; });
    }

    std::string error() const {
        std::string message = fatal.empty() ? std::string("VICE stopped") : fatal;
        if (!last_error.empty()) message += ": " + last_error;
        return message;
    }

    // Starts VICE with its user folder and command line and waits for the end of its first frame;
    // false with error set when it could not start.
    bool start(const std::string &home, std::vector<std::string> args, std::string &error) {
        started = true;
        user_dir = home;
        std::thread([this, args = std::move(args)]() mutable {
            std::vector<char *> argv;
            for (auto &arg : args) argv.push_back(arg.data());
            argv.push_back(nullptr);
            KairoC64_Run(user_dir.c_str(), int(args.size()), argv.data());
            std::lock_guard lock(mutex);
            ended = true;
            changed.notify_all();
        }).detach();
        std::unique_lock lock(mutex);
        changed.wait(lock, [&] { return turn == Turn::kHost || ended; });
        if (ended) {
            error = this->error();
            return false;
        }
        return true;
    }
};

Vice vice;

bool is_t64(const std::string &path) {
    if (path.size() < 4) return false;
    std::string extension = path.substr(path.size() - 4);
    for (char &c : extension) c = char(std::tolower(static_cast<unsigned char>(c)));
    return extension == ".t64";
}

void notice(const char *text) {
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_ERROR, "Kairo64", "VICE: %s", text);
#else
    std::fprintf(stderr, "VICE: %s\n", text);
#endif
}

// Sets up the machine for config: model, ROMs, drive, ports and the settings Kairo64 runs every
// machine with. VICE is waiting at the end of a frame.
bool configure(const Config &config, std::string &error) {
    auto set_int = [&](const char *name, int value) {
        if (KairoC64_SetInt(name, value)) return true;
        error = std::string("VICE refused ") + name + " = " + std::to_string(value);
        return false;
    };
    auto set_string = [&](const char *name, const std::string &value, const char *what) {
        if (KairoC64_SetString(name, value.c_str())) return true;
        error = std::string("VICE could not load the ") + what + " " + value;
        return false;
    };
    if (!KairoC64_SetModel(config.model.c_str())) {
        error = "VICE has no C64 model " + config.model;
        return false;
    }
    // After the model, which names its own ROMs.
    if (!set_string("KernalName", config.kernal, "KERNAL ROM") || !set_string("BasicName", config.basic, "BASIC ROM") ||
        !set_string("ChargenName", config.chargen, "character ROM")) return false;
    // Drive 8: an emulated 1541 on its own DOS ROM, or VICE's virtual drive on the serial bus.
    // Never KERNAL traps, which patch Commodore's KERNAL and so fit only it.
    const bool true_drive = !config.dos1541.empty();
    if (true_drive && !set_string("DosName1541", config.dos1541, "1541 drive ROM")) return false;
    // True emulation before the type: VICE puts a drive of a new type on the bus only while true
    // emulation is on, so after a machine on the virtual drive (true emulation off) a 1541 set
    // first would never answer ("?DEVICE NOT PRESENT").
    if (!set_int("TrapDevice8", 0) || !set_int("Drive8TrueEmulation", true_drive ? 1 : 0) ||
        !set_int("Drive8Type", true_drive ? 1541 : 0) || !set_int("BusDevice8", true_drive ? 0 : 1))
        return false;
    // A program starts from a disk VICE writes it to, in the machine's own folder (VICE's default
    // is a per-user cache folder an app does not have).
    if (!KairoC64_SetString("AutostartPrgDiskImage", (config.home + "/program.d64").c_str())) {
        error = "VICE refused its program disk " + config.home + "/program.d64";
        return false;
    }
    // Port 1: the joystick, or the 1351 mouse; port 2: the joystick. VICE's joyport IDs:
    // 1 is the joystick, 3 the 1351 mouse (joyport.h).
    if (!set_int("Mouse", config.mouse ? 1 : 0) || !set_int("JoyPort1Device", config.mouse ? 3 : 1) ||
        !set_int("JoyPort2Device", 1)) return false;
    // Raw pixels at the VIC-II's own size with the normal borders, for the host's shaders; a JAM
    // reaches KairoC64Host_Jam through the headless UI's dialog (local change vice-jam); the host
    // runs the load at full speed itself, so autostart does not warp.
    return set_int("VICIIFilter", 0) && set_int("VICIIDoubleSize", 0) && set_int("VICIIDoubleScan", 0) &&
           set_int("VICIIBorderMode", 0) && set_int("JAMAction", 0) && set_int("AutostartWarp", 0);
}

} // namespace

C64Machine::C64Machine(const Config &config) : config_(config) {}

C64Machine::~C64Machine() {
    if (!vice.ended) {
        KairoC64_ReleaseInputs();
        KairoC64_EjectAll();
        // Sound left over from this machine is not the next one's.
        int16_t discarded[512 * 2];
        while (KairoC64_ReadAudio(discarded, 512) > 0) {}
    }
    vice.machine = false;
}

std::unique_ptr<C64Machine> C64Machine::create(const Config &config, std::string &error) {
    if (vice.machine) {
        error = "Another machine is still running";
        return nullptr;
    }
    if (vice.ended) {
        error = "VICE stopped earlier in this run of the app (" + vice.error() + "); restart the app";
        return nullptr;
    }
    if (!vice.started) {
        // VICE's command line: no configuration file, Kairo64's sound device, and the machine's
        // ROMs, which VICE needs to start. The drive starts empty: VICE loads a drive ROM only
        // when its name changes after VICE is up, so configure names it. Everything else is set
        // per machine (configure).
        std::vector<std::string> args = {
            "x64sc", "-default",
            "-logfile", config.home + "/vice.log",
            "-sounddev", "kairo",
            "-soundrate", std::to_string(config.sample_rate),
            "-kernal", config.kernal,
            "-basic", config.basic,
            "-chargen", config.chargen,
            "-drive8type", "0",
        };
        if (!vice.start(config.home, std::move(args), error)) return nullptr;
    }
    vice.jam.clear();
    vice.quiet = true;
    KairoC64_EjectAll();
    if (!configure(config, error)) return nullptr;
    vice.last_error.clear();
    KairoC64_PowerCycle();
    std::unique_ptr<C64Machine> machine(new C64Machine(config));
    std::string start = config.start;
    if (config.t64_on_disk && is_t64(start)) {
        const std::string disk = config.home + "/tape.d64";
        char message[256];
        if (!KairoC64_TapeToDisk(start.c_str(), disk.c_str(), message, sizeof message)) {
            error = std::string("Could not put the tape's programs on a disk: ") + message;
            return nullptr;
        }
        start = disk;
        machine->config_.drive8 = disk;
    }
    if (!start.empty() && !KairoC64_Autostart(start.c_str())) {
        error = "VICE could not start " + start + (vice.last_error.empty() ? "" : ": " + vice.last_error);
        return nullptr;
    }
    // A disk the game starts from is already in drive 8: autostart put it there.
    const std::string &drive8 = machine->config_.drive8;
    if (drive8 == start) {
        machine->drive_[kDrive8] = drive8;
    } else if (!drive8.empty() && !machine->insert(kDrive8, drive8, error)) {
        return nullptr;
    }
    vice.machine = true;
    return machine;
}

bool C64Machine::booting() const { return KairoC64_Booting(); }

bool C64Machine::run_frame(std::string &error) {
    if (vice.ended || !vice.run()) {
        error = vice.error();
        return false;
    }
    if (!vice.jam.empty()) {
        error = "The C64 stopped: " + vice.jam;
        return false;
    }
    vice.quiet = false;
    return true;
}

double C64Machine::frame_seconds() const { return KairoC64_FrameSeconds(); }

const uint8_t *C64Machine::frame(uint32_t &width, uint32_t &height) {
    int w, h;
    const uint8_t *pixels = KairoC64_Screen(&w, &h);
    width = uint32_t(w);
    height = uint32_t(h);
    return pixels;
}

int C64Machine::sample_rate() const { return KairoC64_SampleRate(); }

uint32_t C64Machine::read_audio(int16_t *stereo, uint32_t max_frames) { return KairoC64_ReadAudio(stereo, max_frames); }

void C64Machine::key(int row, int column, bool down) { KairoC64_Key(row, column, down); }

void C64Machine::restore(bool down) { KairoC64_Restore(down); }

void C64Machine::joystick(int control, bool down) {
    static_assert(kJoystickControls == KAIRO_C64_JOYSTICK_CONTROLS);
    if (control >= 0 && control < kJoystickControls) KairoC64_Joystick(config_.joystick_port, control, down);
}

void C64Machine::mouse_move(int dx, int dy) { KairoC64_MouseMove(dx, dy); }

void C64Machine::mouse_button(int button, bool down) {
    if (button == 1 || button == 2) KairoC64_MouseButton(button, down);
}

void C64Machine::reset() { KairoC64_PowerCycle(); }

bool C64Machine::save_state(const std::string &file, std::string &error) {
    KairoC64_RequestSave(file.c_str());
    if (!run_frame(error)) return false;
    if (KairoC64_SnapshotFailed()) {
        error = "VICE could not save the state to " + file;
        return false;
    }
    return true;
}

bool C64Machine::load_state(const std::string &file, std::string &error) {
    KairoC64_RequestLoad(file.c_str());
    if (!run_frame(error)) return false;
    if (KairoC64_SnapshotFailed()) {
        error = "VICE could not load the state from " + file;
        return false;
    }
    return true;
}

bool C64Machine::insert(Drive drive, const std::string &path, std::string &error) {
    if (!KairoC64_AttachDisk(8 + drive, path.c_str())) {
        error = "VICE refused the disk image " + path;
        return false;
    }
    drive_[drive] = path;
    return true;
}

std::string C64Machine::media_path(Drive drive) { return drive_[drive]; }

std::string C64Machine::screen_text() {
    char cells[KAIRO_C64_SCREEN_CELLS + 1];
    KairoC64_ScreenText(cells);
    std::string text;
    for (int row = 0; row < 25; ++row) text.append(cells + row * 40, 40).push_back('\n');
    return text;
}

} // namespace kairo64

// VICE's thread.

extern "C" void KairoC64Host_Vbl(void) { kairo64::vice.at_vbl(); }

extern "C" void KairoC64Host_Fatal(const char *message) {
    // The first is the cause; later ones follow from it.
    if (kairo64::vice.fatal.empty()) kairo64::vice.fatal = message;
    kairo64::notice(message);
}

extern "C" void KairoC64Host_Jam(const char *message) {
    if (kairo64::vice.jam.empty()) kairo64::vice.jam = message;
}

// VICE's log goes to its log file; its errors and warnings also go where the platform's logs are
// read, at error level, as some devices keep no other.
extern "C" void KairoC64Host_Log(const char *level, const char *text) {
    const std::string prefix = level;
    const bool error = prefix.find("Error") != std::string::npos;
    if (!error && prefix.find("Warning") == std::string::npos) return;
    if (error) kairo64::vice.last_error = prefix + text;
    if (!kairo64::vice.quiet) kairo64::notice((prefix + text).c_str());
}
