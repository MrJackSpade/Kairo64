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
#include <vector>

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
    // The program on the start's disk or tape to load and run, by its name; "" for its first.
    std::string program;
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
    // A hard disk image, "" for none: a CMD HD in drive 8 on its boot ROM cmdhd_rom (a .dhd image),
    // or the IDE64 cartridge's first drive on its IDEDOS ROM ide64_rom (a .hdd or .cfa image). The
    // ROMs are the player's. The machine starts at BASIC's prompt with it; its startup commands
    // start the game.
    std::string hard_disk;
    std::string cmdhd_rom;
    std::string ide64_rom;
    // The audio sample rate.
    int sample_rate = 48000;
};

#include "kairo/relative_warp.h"

class C64Machine {
public:
    // A machine running config's game; null with error set when it cannot start.
    static std::unique_ptr<C64Machine> create(const Config &config, std::string &error);
    // Writes a new, unformatted 1541 disk (D64, 174,848 bytes) at path with VICE's own image creator.
    // False with error set when it could not.
    static bool create_blank_disk(const std::string &path, std::string &error);
    ~C64Machine();

    // Whether VICE's autostart is still loading the game's first program. It ends when that
    // program runs, which for a game with its own loader (most tapes, many disks) is when the
    // loader takes over, long before the game is in.
    bool autostarting() const;
    // Whether the screen editor waits for a line, as at BASIC's READY. prompt (KairoC64_WaitsForLine).
    bool waits_for_line() const;
    // How many times the guest has read the mouse's position (KairoC64_MouseReads).
    unsigned long mouse_reads() const;
    // The raster lines the game's enabled raster interrupts waited on since the last call
    // (KairoC64_RasterIrqLines): a line of 263 or later exists only on a PAL C64.
    std::vector<unsigned> raster_irq_lines();
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
    // Direct touch: moves a mouse game's pointer to point (x, y) of the picture over the next frames,
    // by a push past the screen's top-left corner and the counts from there (kairo::RelativeWarp),
    // for a game that moves its pointer a pixel a count and stops it there. The 1351 tells a program
    // its position modulo 64, so the motion goes 31 counts at a time, each after the program read
    // the mouse; without the mouse plugged in nothing reads it and the warp does not finish.
    void mouse_warp(int x, int y);
    bool mouse_warp_done() const;
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
    // The tape in the datasette, or none for "". A tape the game started from is in it, unless it
    // started from a disk written from its T64 (Config::t64_on_disk).
    bool insert_tape(const std::string &path, std::string &error);
    std::string tape_path() const;
    // The cartridge in the port, or none for "": changing it starts the machine again.
    bool insert_cartridge(const std::string &path, std::string &error);
    std::string cartridge_path() const;
    // The hard disk on the interface the machine started with (Config::hard_disk), or another
    // image there; an IDE64 one starts the machine again. "" when it has none.
    bool insert_hard_disk(const std::string &path, std::string &error);
    std::string hard_disk_path() const;

    // The text screen as 25 lines of 40 characters, for tests that look for BASIC's messages.
    std::string screen_text();

private:
    C64Machine(const Config &config);
    Config config_;
    std::string drive_[kDrives];
    std::string tape_;
    std::string cartridge_;
    std::string hard_disk_;
    // Direct touch: the warp, and its motion still to give the 1351, with the mouse reads when the
    // last part went.
    kairo::RelativeWarp warp_;
    int paced_x_ = 0, paced_y_ = 0;
    unsigned long reads_at_motion_ = 0;
    bool motion_read_ = true;
    void pace_motion();
};

} // namespace kairo64
