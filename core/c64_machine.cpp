#include "c64_machine.h"

#include <cctype>
#include <cstdio>
#include <utility>
#include <vector>

#include "kairo/emulator_turns.h"
#include "kairo/fresh_library.h"
#include "kairo_c64.h"

#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace kairo64 {

namespace {

void notice(const char *text) {
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_ERROR, "Kairo64", "VICE: %s", text);
#else
    std::fprintf(stderr, "VICE: %s\n", text);
#endif
}

// A fresh copy of the VICE library and its functions; null with error set when it cannot load.
std::unique_ptr<kairo::FreshLibrary> load_vice(const KairoC64Api *&api, std::string &error) {
    auto library = kairo::FreshLibrary::load(KAIRO64_VICE_FILE, error);
    if (!library) return nullptr;
    auto entry = reinterpret_cast<const KairoC64Api *(*)()>(library->symbol(KAIRO_C64_API_SYMBOL));
    if (!entry) {
        error = std::string(KAIRO64_VICE_FILE) + " has no " + KAIRO_C64_API_SYMBOL;
        return nullptr;
    }
    api = entry();
    return library;
}

// Whether path ends in extension (".t64"), in any case.
bool has_extension(const std::string &path, const std::string &extension) {
    if (path.size() < extension.size()) return false;
    std::string end = path.substr(path.size() - extension.size());
    for (char &c : end) c = char(std::tolower(static_cast<unsigned char>(c)));
    return end == extension;
}

bool is_t64(const std::string &path) { return has_extension(path, ".t64"); }

} // namespace

// This machine's VICE: its own copy of the library, running on its own thread and taking turns with
// the host's (kairo/emulator_turns.h): the host lets VICE run until the end of its next frame, and
// VICE waits there until the host lets it run again.
struct C64Machine::Session {
    // Unloaded after VICE's thread ends (the members go in reverse order).
    std::unique_ptr<kairo::FreshLibrary> library;
    const KairoC64Api *vice = nullptr;
    KairoC64Host host{};
    kairo::EmulatorTurns turns;
    // VICE's folder for its cache, configuration and state, for VICE's whole run.
    std::string user_dir;
    // Why VICE ended (fatal), on VICE's thread before its turn ends.
    std::string fatal;
    // The last error VICE logged, which says more than an exit code.
    std::string last_error;
    // The CPU stopped (jam), during its frame.
    std::string jam;
    // VICE is starting or the machine is being set up, through its first frame: VICE logs errors
    // for the ROMs its defaults and models name before Kairo64 names its own, and for the KERNAL
    // traps it cannot install on a KERNAL other than Commodore's as the machine resets, so its log
    // lines stay in its log file and in last_error, which a failure reports.
    bool quiet = true;

    std::string error() const {
        std::string message = fatal.empty() ? std::string("VICE stopped") : fatal;
        if (!last_error.empty()) message += ": " + last_error;
        return message;
    }

    // Ends VICE's thread: asks it to quit and lets it run to the end.
    ~Session() {
        if (!vice) return;
        if (!turns.ended()) vice->quit();
        turns.finish();
    }
};

namespace {

// VICE's thread's calls (KairoC64Host), with the session as their context.
using Session = C64Machine::Session;

void host_vbl(void *context) { static_cast<Session *>(context)->turns.frame_end(); }

void host_fatal(void *context, const char *message) {
    auto *session = static_cast<Session *>(context);
    // The first is the cause; later ones follow from it.
    if (session->fatal.empty()) session->fatal = message;
    notice(message);
}

void host_jam(void *context, const char *message) {
    auto *session = static_cast<Session *>(context);
    if (session->jam.empty()) session->jam = message;
}

// VICE's log goes to its log file; its errors and warnings also go where the platform's logs are
// read, at error level, as some devices keep no other.
void host_log(void *context, const char *level, const char *text) {
    auto *session = static_cast<Session *>(context);
    const std::string prefix = level;
    const bool error = prefix.find("Error") != std::string::npos;
    if (!error && prefix.find("Warning") == std::string::npos) return;
    if (error) session->last_error = prefix + text;
    if (!session->quiet) notice((prefix + text).c_str());
}

// Sets up the machine for config: model, ROMs, drive, ports and the settings Kairo64 runs every
// machine with. VICE is waiting at the end of a frame.
bool configure(const KairoC64Api &vice, const Config &config, std::string &error) {
    auto set_int = [&](const char *name, int value) {
        if (vice.set_int(name, value)) return true;
        error = std::string("VICE refused ") + name + " = " + std::to_string(value);
        return false;
    };
    auto set_string = [&](const char *name, const std::string &value, const char *what) {
        if (vice.set_string(name, value.c_str())) return true;
        error = std::string("VICE could not load the ") + what + " " + value;
        return false;
    };
    if (!vice.set_model(config.model.c_str())) {
        error = "VICE has no C64 model " + config.model;
        return false;
    }
    // After the model, which names its own ROMs.
    if (!set_string("KernalName", config.kernal, "KERNAL ROM") || !set_string("BasicName", config.basic, "BASIC ROM") ||
        !set_string("ChargenName", config.chargen, "character ROM")) return false;
    // Drive 8: a CMD HD on its boot ROM, an emulated 1541 on its own DOS ROM, or VICE's virtual
    // drive on the serial bus. Never KERNAL traps, which patch Commodore's KERNAL and so fit only it.
    const bool cmdhd = !config.cmdhd_rom.empty();
    if (cmdhd && !config.dos1541.empty()) {
        error = "Drive 8 cannot be both a CMD HD and a 1541";
        return false;
    }
    const bool true_drive = !config.dos1541.empty();
    if (true_drive && !set_string("DosName1541", config.dos1541, "1541 drive ROM")) return false;
    if (cmdhd && !set_string("DosNameCMDHD", config.cmdhd_rom, "CMD HD boot ROM")) return false;
    // True emulation before the type: VICE puts a drive of a new type on the bus only while true
    // emulation is on, so a 1541 set first would never answer ("?DEVICE NOT PRESENT").
    if (!set_int("TrapDevice8", 0) || !set_int("Drive8TrueEmulation", true_drive || cmdhd ? 1 : 0) ||
        !set_int("Drive8Type", cmdhd ? 4844 : true_drive ? 1541 : 0) || !set_int("BusDevice8", true_drive || cmdhd ? 0 : 1))
        return false;
    // The IDE64's first drive holds the hard disk; its cartridge goes in once the machine is set up.
    if (!config.ide64_rom.empty() && !set_string("IDE64Image1", config.hard_disk, "IDE64 hard disk image")) return false;
    // This machine's ROMs, after the model named its own; the 1541's once drive 8 is one.
    if (!vice.load_roms(config.kernal.c_str(), config.basic.c_str(), config.chargen.c_str(), true_drive)) {
        error = "VICE could not load the ROMs " + config.kernal + ", " + config.basic + ", " + config.chargen +
                (true_drive ? ", " + config.dos1541 : std::string());
        return false;
    }
    // A program starts from a disk VICE writes it to, in the machine's own folder (VICE's default
    // is a per-user cache folder an app does not have).
    if (!vice.set_string("AutostartPrgDiskImage", (config.home + "/program.d64").c_str())) {
        error = "VICE refused its program disk " + config.home + "/program.d64";
        return false;
    }
    // Port 1: the joystick, or the 1351 mouse; port 2: the joystick. VICE's joyport IDs:
    // 1 is the joystick, 3 the 1351 mouse (joyport.h).
    if (!set_int("Mouse", config.mouse ? 1 : 0) || !set_int("JoyPort1Device", config.mouse ? 3 : 1) ||
        !set_int("JoyPort2Device", 1)) return false;
    // Raw pixels at the VIC-II's own size with the normal borders, for the host's shaders; a JAM
    // reaches the host's jam through the headless UI's dialog (local change vice-jam); the host
    // runs the load at full speed itself, so autostart does not warp.
    // reSID samples the SID's output directly (0, SID_RESID_SAMPLING_FAST). VICE's default
    // resampling filter kept the C64 below real time on the RG DS (0.9x flat out); interpolation
    // reached 1.05x, too little headroom for play; direct sampling runs a frame in 13.8 of its
    // 20 ms there (#1).
    return set_int("VICIIFilter", 0) && set_int("VICIIDoubleSize", 0) && set_int("VICIIDoubleScan", 0) &&
           set_int("VICIIBorderMode", 0) && set_int("JAMAction", 0) && set_int("AutostartWarp", 0) &&
           set_int("SidResidSampling", 0);
}

} // namespace

C64Machine::C64Machine(const Config &config, std::unique_ptr<Session> session)
    : config_(config), session_(std::move(session)), vice_(*session_->vice) {}

C64Machine::~C64Machine() = default;

std::unique_ptr<C64Machine> C64Machine::create(const Config &config, std::string &error) {
    auto session = std::make_unique<Session>();
    session->library = load_vice(session->vice, error);
    if (!session->library) return nullptr;
    session->host = {session.get(), host_vbl, host_fatal, host_log, host_jam};
    session->user_dir = config.home;
    // VICE's command line: no configuration file, Kairo64's sound device, and the machine's ROMs,
    // which VICE needs to start. The drive starts empty: VICE loads a drive ROM only when its name
    // changes after VICE is up, so configure names it. Everything else is set as VICE waits at
    // the end of its first frame (configure).
    std::vector<std::string> args = {"x64sc"};
    // A machine that must repeat seeds VICE's random numbers (the random autostart delay, the RAM's
    // power-on pattern) as it starts, before it draws any; VICE seeds them from the time otherwise.
    if (config.repeatable) args.insert(args.end(), {"-seed", "1"});
    args.insert(args.end(), {
        "-default",
        "-logfile", config.home + "/vice.log",
        "-sounddev", "kairo",
        "-soundrate", std::to_string(config.sample_rate),
        "-kernal", config.kernal,
        "-basic", config.basic,
        "-chargen", config.chargen,
        "-drive8type", "0",
    });
    Session *running = session.get();
    const bool up = session->turns.start([running, args = std::move(args)]() mutable {
        std::vector<char *> argv;
        for (auto &arg : args) argv.push_back(arg.data());
        argv.push_back(nullptr);
        running->vice->run(running->user_dir.c_str(), int(args.size()), argv.data(), &running->host);
    });
    if (!up) {
        error = session->error();
        return nullptr;
    }
    const KairoC64Api &vice = *session->vice;
    if (!configure(vice, config, error)) return nullptr;
    session->last_error.clear();
    vice.power_cycle();
    std::unique_ptr<C64Machine> machine(new C64Machine(config, std::move(session)));
    std::string start = config.start;
    if (config.t64_on_disk && is_t64(start)) {
        const std::string disk = config.home + "/tape.d64";
        char message[256];
        if (!vice.tape_to_disk(start.c_str(), disk.c_str(), message, sizeof message)) {
            error = std::string("Could not put the tape's programs on a disk: ") + message;
            return nullptr;
        }
        start = disk;
        machine->config_.drive8 = disk;
    }
    if (!start.empty() && !vice.autostart(start.c_str(), config.program.c_str())) {
        error = "VICE could not start " + start + (config.program.empty() ? "" : " (program " + config.program + ")") +
                (machine->session_->last_error.empty() ? "" : ": " + machine->session_->last_error);
        return nullptr;
    }
    // A tape or cartridge the game starts from is in the datasette or the port: autostart put it there.
    const bool from_tape = start == config.start && (is_t64(start) || has_extension(start, ".tap"));
    if (from_tape) machine->tape_ = start;
    // Disks and programs (VICE writes a program to its program disk) load through drive 8.
    if (!start.empty() && !has_extension(start, ".crt"))
        machine->autostart_from_ = from_tape ? AutostartFrom::kTape : AutostartFrom::kDrive8;
    if (has_extension(start, ".crt")) machine->cartridge_ = start;
    // A hard disk: the CMD HD's image in drive 8, or the IDE64 cartridge in the port.
    if (!config.cmdhd_rom.empty()) {
        if (!machine->insert(kDrive8, config.hard_disk, error)) return nullptr;
        machine->hard_disk_ = config.hard_disk;
    } else if (!config.ide64_rom.empty()) {
        if (!vice.attach_ide64(config.ide64_rom.c_str())) {
            error = "VICE refused the IDE64 ROM " + config.ide64_rom;
            return nullptr;
        }
        machine->hard_disk_ = config.hard_disk;
    }
    // A disk the game starts from is already in drive 8: autostart put it there.
    const std::string &drive8 = machine->config_.drive8;
    if (drive8 == start) {
        machine->drive_[kDrive8] = drive8;
    } else if (!drive8.empty() && !machine->insert(kDrive8, drive8, error)) {
        return nullptr;
    }
    return machine;
}

bool C64Machine::create_blank_disk(const std::string &path, std::string &error) {
    // VICE's image creator needs no running machine: a copy of the library for the call alone.
    const KairoC64Api *vice = nullptr;
    auto library = load_vice(vice, error);
    if (!library) return false;
    if (!vice->create_blank_disk(path.c_str())) {
        error = "VICE could not write a blank disk at " + path;
        return false;
    }
    FILE *file = std::fopen(path.c_str(), "rb");
    long size = -1;
    if (file) {
        std::fseek(file, 0, SEEK_END);
        size = std::ftell(file);
        std::fclose(file);
    }
    if (size != 174848) {
        error = "VICE wrote " + std::to_string(size) + " bytes for a blank D64 at " + path + ", not 174848";
        return false;
    }
    return true;
}

bool C64Machine::autostarting() const { return vice_.autostarting(); }
bool C64Machine::waits_for_line() const { return vice_.waits_for_line(); }
unsigned long C64Machine::mouse_reads() const { return vice_.mouse_reads(); }
std::vector<unsigned> C64Machine::raster_irq_lines()
{
    uint8_t bits[64];
    vice_.raster_irq_lines(bits);
    std::vector<unsigned> lines;
    for (unsigned line = 0; line < 512; ++line)
        if (bits[line / 8] & (1 << (line % 8))) lines.push_back(line);
    return lines;
}

bool C64Machine::tape_moving() const { return vice_.tape_moving(); }

bool C64Machine::disk_loading() const { return vice_.disk_loading(); }

bool C64Machine::run_frame(std::string &error) {
    pace_motion();
    if (!session_->turns.run_frame()) {
        error = session_->error();
        return false;
    }
    if (!session_->jam.empty()) {
        error = "The C64 stopped: " + session_->jam;
        return false;
    }
    vice_.frame_done();
    session_->quiet = false;
    return true;
}

double C64Machine::frame_seconds() const { return vice_.frame_seconds(); }

const uint8_t *C64Machine::frame(uint32_t &width, uint32_t &height) {
    int w, h;
    const uint8_t *pixels = vice_.screen(&w, &h);
    width = uint32_t(w);
    height = uint32_t(h);
    return pixels;
}

int C64Machine::sample_rate() const { return vice_.sample_rate(); }

uint32_t C64Machine::read_audio(int16_t *stereo, uint32_t max_frames) { return vice_.read_audio(stereo, max_frames); }

void C64Machine::key(int row, int column, bool down) { vice_.key(row, column, down); }

void C64Machine::restore(bool down) { vice_.restore(down); }

void C64Machine::joystick(int control, bool down) {
    static_assert(kJoystickControls == KAIRO_C64_JOYSTICK_CONTROLS);
    if (control >= 0 && control < kJoystickControls) vice_.joystick(config_.joystick_port, control, down);
}

void C64Machine::mouse_move(int dx, int dy) {
    // A drag after a direct tap goes with the warp's counts until they are given.
    if (warp_.hold(dx, dy)) return;
    vice_.mouse_move(dx, dy);
}

// The screen is 320 by 200 pixels inside VICE's borders, centred in the picture.
void C64Machine::mouse_warp(int x, int y) {
    uint32_t width = 0, height = 0;
    if (!frame(width, height)) return;
    warp_.start(x - (int(width) - 320) / 2, y - (int(height) - 200) / 2, 320, 200);
    paced_x_ = paced_y_ = 0;
    motion_read_ = true;
}

bool C64Machine::mouse_warp_done() const { return warp_.done(); }

// Before each frame: the warp's next motion, and 31 counts of what is still to give once the
// program has read the mouse since the last.
void C64Machine::pace_motion() {
    const unsigned long reads = vice_.mouse_reads();
    if (reads != reads_at_motion_) motion_read_ = true;
    if (warp_.running()) {
        int dx, dy;
        warp_.step(paced_x_ == 0 && paced_y_ == 0 && motion_read_, dx, dy);
        paced_x_ += dx;
        paced_y_ += dy;
    }
    if ((paced_x_ || paced_y_) && motion_read_) {
        const int step_x = paced_x_ > 31 ? 31 : paced_x_ < -31 ? -31 : paced_x_;
        const int step_y = paced_y_ > 31 ? 31 : paced_y_ < -31 ? -31 : paced_y_;
        vice_.mouse_move(step_x, step_y);
        paced_x_ -= step_x;
        paced_y_ -= step_y;
        reads_at_motion_ = reads;
        motion_read_ = false;
    }
}

void C64Machine::mouse_button(int button, bool down) {
    if (button == 1 || button == 2) vice_.mouse_button(button, down);
}

void C64Machine::reset() { vice_.power_cycle(); }

// A save asked while VICE's autostart is loading the game, or while the game loads through VICE's
// virtual drive, waits for the load's end (Kairo64#66): autostart and the virtual drive work
// through host-side traps that VICE's snapshots do not hold, so a state saved then does not load
// back to the same machine. An emulated 1541 or CMD HD is in the snapshot, down to its CPU. The
// frames run meanwhile are the load's, run as the app runs a load, flat out; their sound is
// dropped, as nothing plays it. A load whose medium went has ended (medium_changed: the autostart
// is cancelled; the virtual drive stops talking), so nothing waits for one that cannot finish.
void C64Machine::medium_changed(AutostartFrom slot) {
    if (slot != autostart_from_ || !autostarting()) return;
    vice_.cancel_autostart();
    autostart_from_ = AutostartFrom::kNothing;
}

bool C64Machine::loading_through_traps() const {
    const bool virtual_drive = config_.dos1541.empty() && config_.cmdhd_rom.empty();
    // The virtual drive's part of a load is a transaction on the bus or a channel it has open, which
    // end; a guest that keeps polling the bus (a KERNAL retrying an emptied drive) leaves gaps between
    // its transactions, and holding a save until the bus fell silent held it forever (Kairo64#69).
    return autostarting() || (virtual_drive && vice_.virtual_drive_busy());
}

bool C64Machine::save_state(const std::string &file, std::string &error) {
    int16_t discarded[512 * 2];
    while (loading_through_traps()) {
        if (!run_frame(error)) return false;
        while (read_audio(discarded, 512) > 0) {}
    }
    vice_.request_save(file.c_str());
    if (!run_frame(error)) return false;
    if (vice_.snapshot_failed()) {
        error = "VICE could not save the state to " + file;
        return false;
    }
    return true;
}

bool C64Machine::load_state(const std::string &file, std::string &error) {
    vice_.request_load(file.c_str());
    if (!run_frame(error)) return false;
    if (vice_.snapshot_failed()) {
        error = "VICE could not load the state from " + file;
        return false;
    }
    return true;
}

bool C64Machine::insert(Drive drive, const std::string &path, std::string &error) {
    if (!vice_.attach_disk(8 + drive, path.c_str())) {
        error = "VICE refused the disk image " + path;
        return false;
    }
    drive_[drive] = path;
    if (drive == kDrive8) medium_changed(AutostartFrom::kDrive8);
    return true;
}

std::string C64Machine::media_path(Drive drive) { return drive_[drive]; }

bool C64Machine::insert_tape(const std::string &path, std::string &error) {
    if (!vice_.attach_tape(path.c_str())) {
        error = "VICE refused the tape image " + path;
        return false;
    }
    tape_ = path;
    medium_changed(AutostartFrom::kTape);
    return true;
}

std::string C64Machine::tape_path() const { return tape_; }

bool C64Machine::insert_cartridge(const std::string &path, std::string &error) {
    if (!vice_.attach_cartridge(path.c_str())) {
        error = "VICE refused the cartridge image " + path;
        return false;
    }
    cartridge_ = path;
    return true;
}

std::string C64Machine::cartridge_path() const { return cartridge_; }

bool C64Machine::insert_hard_disk(const std::string &path, std::string &error) {
    if (!config_.cmdhd_rom.empty()) {
        if (!insert(kDrive8, path, error)) return false;
    } else if (!config_.ide64_rom.empty()) {
        if (!vice_.set_string("IDE64Image1", path.c_str()) || !vice_.attach_ide64(config_.ide64_rom.c_str())) {
            error = "VICE refused the IDE64 hard disk image " + path;
            return false;
        }
    } else {
        error = "This C64 started without a hard disk interface; start the hard disk as a game instead";
        return false;
    }
    hard_disk_ = path;
    return true;
}

std::string C64Machine::hard_disk_path() const { return hard_disk_; }

std::string C64Machine::screen_text() {
    char cells[KAIRO_C64_SCREEN_CELLS + 1];
    vice_.screen_text(cells);
    std::string text;
    for (int row = 0; row < 25; ++row) text.append(cells + row * 40, 40).push_back('\n');
    return text;
}

} // namespace kairo64
