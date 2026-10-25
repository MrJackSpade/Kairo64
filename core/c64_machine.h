// A Commodore 64 on VICE's x64sc (third_party/vice), run one video frame at a time by its host
// instead of VICE's own real-time loop. VICE runs on a thread of its own and hands control back
// at the end of each frame (core/kairo/kairo_c64.h); between frames the host takes the picture and
// sound and sends input.
//
// VICE keeps process-wide state and cannot start twice in a process: it starts with the first
// machine and stays, and each later machine is the same VICE set up again and switched on anew.
// A process runs one machine at a time.

#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace kairo64 {

// The disk drive: 8. Games with several disks or sides swap them in it.
enum Drive { kDrive8, kDrives };

// The joystick's controls: up, down, left, right, fire.
constexpr int kJoystickControls = 5;

// The keyboard matrix: 8 rows of 8 keys.
constexpr int kMatrixRows = 8;
constexpr int kMatrixColumns = 8;

// What a machine starts with.
struct Config {
    // A private folder for VICE's own files (its log).
    std::string home;
    // The model, by VICE's -model name: c64 (PAL, 6581 SID), c64c (PAL, 8580 SID), c64ntsc and so on.
    std::string model = "c64";
    // The system ROMs: KERNAL, BASIC and character set.
    std::string kernal;
    std::string basic;
    std::string chargen;
    // The 1541 drive's DOS ROM, or "" for none. With it, drive 8 is an emulated 1541 down
    // to its own CPU (VICE's true drive emulation), which fast loaders and copy protection need.
    // Without, it is VICE's virtual drive on the serial bus, which loads files the standard way.
    std::string dos1541;
    // The game to start: a disk, tape or program, loaded and run as VICE's autostart does; "" for
    // none (the machine starts at BASIC's prompt).
    std::string start;
    // The KERNAL has no Commodore tape routines for VICE to trap (the Open ROMs): a T64 tape's
    // programs then start from a disk written from it (home/tape.d64) in drive 8.
    bool t64_on_disk = false;
    // The disk in drive 8: the start itself when that is a disk, another disk the game needs
    // there, or "" for none.
    std::string drive8;
    // The joystick port the host's joystick is in: 1 or 2.
    int joystick_port = 2;
    // The 1351 mouse in port 1 instead of a joystick.
    bool mouse = false;
    // The audio sample rate.
    int sample_rate = 48000;
};

class C64Machine {
public:
    // A machine running config's game; null with error set when it cannot start.
    static std::unique_ptr<C64Machine> create(const Config &config, std::string &error);
    ~C64Machine();

    // Whether VICE's autostart is still loading the game's first program. It ends when that
    // program runs, which for a game with its own loader (most tapes, many disks) is when the
    // loader takes over, long before the game is in.
    bool autostarting() const;
    // Whether the datasette's tape is moving under its motor.
    bool tape_moving() const;
    // Whether drive 8 is loading: the serial bus has moved within the last second of guest time,
    // with an emulated 1541's motor on. A 1541 that only spins (some games leave the motor on
    // while they play) is not loading.
    bool disk_loading() const;
    // Whether a load is physically running: the autostart, a moving tape or a disk load. The
    // host runs the machine flat out meanwhile; normal speed comes back when the load stops.
    bool loading() const { return autostarting() || tape_moving() || disk_loading(); }
    // Runs one guest video frame; false with error set when the machine stops.
    bool run_frame(std::string &error);
    // The guest time the last frame lasted.
    double frame_seconds() const;
    // The latest picture: RGBA rows of width pixels.
    const uint8_t *frame(uint32_t &width, uint32_t &height);
    int sample_rate() const;
    // Takes up to max_frames interleaved stereo frames produced so far.
    uint32_t read_audio(int16_t *stereo, uint32_t max_frames);

    // A key by its place in the keyboard matrix.
    void key(int row, int column, bool down);
    // RESTORE, which is outside the matrix.
    void restore(bool down);
    // A control of the joystick in the configured port: kJoystickControls in their order.
    void joystick(int control, bool down);
    // The 1351 mouse: motion in C64 pixels, and button 1 (left) or 2 (right).
    void mouse_move(int dx, int dy);
    void mouse_button(int button, bool down);

    // A reset, as the C64's power switch does, with the disks left in.
    void reset();
    // Saves or loads the whole machine; false with error set when VICE could not. VICE does
    // either between CPU instructions, so the call runs one more frame.
    bool save_state(const std::string &file, std::string &error);
    bool load_state(const std::string &file, std::string &error);

    // Inserts an image in a drive, or ejects it for ""; false with error set when VICE refused it.
    bool insert(Drive drive, const std::string &path, std::string &error);
    // The image in a drive, as it was inserted; "" when it is empty.
    std::string media_path(Drive drive);

    // The text screen as 25 lines of 40 characters, for tests that look for BASIC's messages.
    std::string screen_text();

private:
    C64Machine(const Config &config);
    Config config_;
    std::string drive_[kDrives];
};

} // namespace kairo64
