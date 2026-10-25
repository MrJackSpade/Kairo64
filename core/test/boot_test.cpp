// Boots a C64 game on the host and runs it for a while without Android.
//
// Usage: boot_test <firmware folder> <work folder> <game> <seconds> [system [runs [keys [model [state [program]]]]]]
//
// <system> picks the ROMs and the drive, as a game's System and Drive settings do: "own" (the
// default) is the player's firmware in <firmware folder> (kernal, basic and chargen, as the retro
// firmware sets name them), "open" the MEGA65 Open ROMs the app ships (third_party/open-roms);
// either runs VICE's virtual drive, and with "+1541" ("own+1541", "open+1541") an emulated 1541 on
// the player's dos1541. A comma-separated list ("open,own+1541") gives each run its own, in turn,
// as a player starts games with different settings one after another in the app.
//
// Runs the machine for <seconds> of guest time, writing a frame every five seconds to
// <work folder>/frame-<run>-<seconds>s.bmp and the last to <work folder>/frame-<run>.bmp, and
// reports the frame size and how much sound it made, and writes the text screen to
// <work folder>/screen-<run>.txt. With <runs> above 1 it starts a new machine
// after each, in the same process, as the app does for the next game. It reports when the load
// ends (C64Machine::autostarting) and writes that frame to <work folder>/frame-<run>-booted.bmp,
// and how long the app would run it flat out (C64Machine::loading: the autostart, a moving tape,
// a disk load).
// <keys> is a list of "frame:row:column" presses (comma separated) in the keyboard matrix, each
// held for five frames; a row of "j" is a joystick control instead, held as long. <model> is a
// VICE model name ("" for the default). With <state> "state", it saves the machine halfway between
// the end of the load and the end of the run, notes the frame after, and at the end loads the
// state and checks that the same frame follows. With "state@<seconds>" it saves at that guest
// second, loading or not, and at the end loads the state and runs to the same guest time again:
// the last picture must be the one the run ended on (a state saved during a tape or disk load
// finishes the load). <program> names the program on the game's disk or tape to start instead of
// its first, as a game's Start program setting does.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "c64_machine.h"

namespace {

// A picture's fingerprint (FNV-1a over its pixels), to tell pictures apart.
uint64_t fingerprint(const uint8_t *rgba, uint32_t width, uint32_t height) {
    uint64_t hash = 14695981039346656037ull;
    for (size_t i = 0, n = size_t(width) * height * 4; i < n; ++i) hash = (hash ^ rgba[i]) * 1099511628211ull;
    return hash;
}

// How many colours a picture has, up to 17: a blank screen has one or two (the border and the
// background), a game's picture more.
int colours(const uint8_t *rgba, uint32_t width, uint32_t height, uint32_t left = 0, uint32_t top = 0,
            uint32_t right = UINT32_MAX, uint32_t bottom = UINT32_MAX) {
    std::vector<uint32_t> seen;
    for (uint32_t y = top; y < std::min(bottom, height) && seen.size() <= 16; ++y) {
        for (uint32_t x = left; x < std::min(right, width) && seen.size() <= 16; ++x) {
            const uint8_t *p = rgba + (size_t(y) * width + x) * 4;
            uint32_t pixel = uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16;
            if (std::find(seen.begin(), seen.end(), pixel) == seen.end()) seen.push_back(pixel);
        }
    }
    return int(seen.size());
}

// How many colours the VIC-II's 320 x 200 display window has (inside the border), up to 17: one
// for a blank screen, two for text on its background. The window starts 32 pixels in and, below
// VICE's normal top border, 35 lines down on a PAL frame (272 lines) or 23 on an NTSC one (247).
int window_colours(const uint8_t *rgba, uint32_t width, uint32_t height) {
    const uint32_t top = height == 272 ? 35 : height == 247 ? 23 : UINT32_MAX;
    if (width != 384 || top == UINT32_MAX) {
        std::fprintf(stderr, "no display window known for a %ux%u frame\n", width, height);
        std::exit(2);
    }
    return colours(rgba, width, height, 32, top, 352, top + 200);
}

void write_bmp(const std::string &path, const uint8_t *rgba, uint32_t width, uint32_t height) {
    const uint32_t row = width * 3 + (4 - width * 3 % 4) % 4;
    const uint32_t size = 54 + row * height;
    std::vector<uint8_t> file(size, 0);
    auto put32 = [&](size_t at, uint32_t value) {
        for (int i = 0; i < 4; ++i) file[at + i] = uint8_t(value >> (8 * i));
    };
    file[0] = 'B';
    file[1] = 'M';
    put32(2, size);
    put32(10, 54);
    put32(14, 40);
    put32(18, width);
    put32(22, height);
    file[26] = 1;
    file[28] = 24;
    put32(34, row * height);
    for (uint32_t y = 0; y < height; ++y) {
        const uint8_t *in = rgba + size_t(height - 1 - y) * width * 4;
        uint8_t *out = file.data() + 54 + size_t(y) * row;
        for (uint32_t x = 0; x < width; ++x, in += 4) {
            *out++ = in[2];
            *out++ = in[1];
            *out++ = in[0];
        }
    }
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char *>(file.data()), file.size());
}

struct Press {
    long frame;
    // -1 for a joystick control, which column then names.
    int row;
    int column;
};

std::vector<Press> parse_keys(const std::string &text) {
    std::vector<Press> presses;
    std::stringstream list(text);
    std::string item;
    while (std::getline(list, item, ',')) {
        auto first = item.find(':');
        auto second = item.find(':', first + 1);
        if (first == std::string::npos || second == std::string::npos) {
            std::fprintf(stderr, "bad key press %s\n", item.c_str());
            std::exit(2);
        }
        const std::string row = item.substr(first + 1, second - first - 1);
        presses.push_back({std::stol(item.substr(0, first)), row == "j" ? -1 : std::stoi(row),
                           std::stoi(item.substr(second + 1))});
    }
    return presses;
}

std::string require(const std::filesystem::path &path) {
    if (!std::filesystem::is_regular_file(path)) {
        std::fprintf(stderr, "missing firmware %s\n", path.string().c_str());
        std::exit(1);
    }
    // Absolute and without "..", as the app passes them: VICE finds a relative drive ROM name
    // only on its own search path, and opens no path with ".." in it.
    return std::filesystem::weakly_canonical(std::filesystem::absolute(path)).string();
}

// Sets the ROMs and drive of a <system> ("open", "own+1541", ...); false for an unknown one.
bool set_system(kairo64::Config &config, const std::string &system, const std::filesystem::path &firmware) {
    // The Open ROMs, beside this source in the checkout.
    const std::filesystem::path open = std::filesystem::path(__FILE__).parent_path() / "../../third_party/open-roms/bin";
    const std::string roms = system.substr(0, system.find('+'));
    if (roms == "own") {
        config.kernal = require(firmware / "kernal");
        config.basic = require(firmware / "basic");
        config.chargen = require(firmware / "chargen");
        config.t64_on_disk = false;
    } else if (roms == "open") {
        config.kernal = require(open / "kernal_generic.rom");
        config.basic = require(open / "basic_generic.rom");
        config.chargen = require(open / "chargen_openroms.rom");
        config.t64_on_disk = true;
    } else {
        return false;
    }
    if (system == roms + "+1541") {
        config.dos1541 = require(firmware / "dos1541");
    } else if (system == roms) {
        config.dos1541.clear();
    } else {
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 5) {
        std::fprintf(stderr, "usage: boot_test <firmware folder> <work folder> <game> <seconds> [system [runs [keys [model [state]]]]]\n");
        return 2;
    }
    const std::filesystem::path firmware = argv[1];
    kairo64::Config config;
    config.home = argv[2];
    std::filesystem::create_directories(config.home);
    config.start = argv[3];
    const double seconds = std::atof(argv[4]);
    std::vector<std::string> systems;
    {
        std::stringstream list(argc > 5 && *argv[5] ? argv[5] : "own");
        for (std::string item; std::getline(list, item, ',');) systems.push_back(item);
    }
    const int runs = argc > 6 ? std::atoi(argv[6]) : 1;
    const std::vector<Press> presses = argc > 7 && *argv[7] ? parse_keys(argv[7]) : std::vector<Press>{};
    if (argc > 8 && *argv[8]) config.model = argv[8];
    const bool state = argc > 9 && std::string(argv[9]) == "state";
    // "state@<seconds>": the guest second to save at; below 0 when not asked for.
    const double save_at = argc > 9 && std::string(argv[9]).rfind("state@", 0) == 0 ? std::atof(argv[9] + 6) : -1;
    if (argc > 10) config.program = argv[10];

    for (const auto &system : systems) {
        if (!set_system(config, system, firmware)) {
            std::fprintf(stderr, "unknown system %s\n", system.c_str());
            return 2;
        }
    }
    if (!std::ifstream(config.start, std::ios::binary)) {
        std::fprintf(stderr, "run 0: could not start: cannot open %s\n", config.start.c_str());
        return 1;
    }
    std::string extension = std::filesystem::path(config.start).extension().string();
    for (char &c : extension) c = char(std::tolower(static_cast<unsigned char>(c)));
    if (extension == ".d64" || extension == ".g64" || extension == ".d71" || extension == ".d81") config.drive8 = config.start;

    std::vector<int16_t> audio(48000 * 2);
    for (int run = 0; run < runs; ++run) {
        std::string error;
        const std::string &system = systems[run % systems.size()];
        set_system(config, system, firmware);
        std::printf("run %d: %s\n", run, system.c_str());
        auto machine = kairo64::C64Machine::create(config, error);
        if (!machine) {
            std::fprintf(stderr, "run %d: could not start: %s\n", run, error.c_str());
            return 1;
        }
        double elapsed = 0;
        long frames = 0;
        const auto started = std::chrono::steady_clock::now();
        std::vector<uint8_t> after_save;
        double saved_at = -1;
        auto picture = [&] {
            uint32_t width, height;
            const uint8_t *rgba = machine->frame(width, height);
            return std::vector<uint8_t>(rgba, rgba + size_t(width) * height * 4);
        };
        bool booting = machine->autostarting();
        double loaded = 0;
        // Guest time the app runs flat out (C64Machine::loading), and when that last ended.
        double loading = 0, loading_ended = 0;
        std::vector<uint64_t> pictures;
        bool was_loading = machine->loading();
        uint64_t samples = 0;
        int64_t loudest = 0;
        int lowest_played = 32767, highest_played = -32768;
        while (elapsed < seconds) {
            for (const auto &press : presses) {
                for (bool down : {true, false}) {
                    if (frames != press.frame + (down ? 0 : 5)) continue;
                    if (press.row < 0) machine->joystick(press.column, down);
                    else machine->key(press.row, press.column, down);
                }
            }
            if (!machine->run_frame(error)) {
                std::fprintf(stderr, "run %d: stopped after %ld frames: %s\n", run, frames, error.c_str());
                return 1;
            }
            const bool now_loading = machine->loading();
            if (now_loading) loading += machine->frame_seconds();
            if (was_loading && !now_loading) loading_ended = elapsed;
            was_loading = now_loading;
            if (booting && !machine->autostarting()) {
                booting = false;
                loaded = elapsed;
                std::printf("run %d: load over after %ld frames (%.1f s)\n", run, frames, elapsed);
                uint32_t width, height;
                const uint8_t *rgba = machine->frame(width, height);
                write_bmp(config.home + "/frame-" + std::to_string(run) + "-booted.bmp", rgba, width, height);
            }
            const double before = elapsed;
            elapsed += machine->frame_seconds();
            // A fingerprint of the picture each second after the load, to tell a game that moves
            // from one stuck on a still picture.
            if (!booting && int(elapsed) != int(before)) {
                uint32_t width, height;
                const uint8_t *rgba = machine->frame(width, height);
                pictures.push_back(fingerprint(rgba, width, height));
            }
            if (int(elapsed / 5) != int(before / 5)) {
                uint32_t width, height;
                const uint8_t *rgba = machine->frame(width, height);
                write_bmp(config.home + "/frame-" + std::to_string(run) + "-" + std::to_string(int(elapsed)) + "s.bmp",
                          rgba, width, height);
            }
            ++frames;
            // Halfway between the end of the load and the end of the run: VICE's states hold no
            // virtual drive in the middle of a transfer, so a state saved during a load cannot
            // finish it.
            if (state && after_save.empty() && !booting && elapsed >= (loaded + seconds) / 2) {
                const std::string file = config.home + "/state.vsf";
                if (!machine->save_state(file, error) || !machine->run_frame(error)) {
                    std::fprintf(stderr, "run %d: save failed: %s\n", run, error.c_str());
                    return 1;
                }
                after_save = picture();
            }
            if (save_at >= 0 && saved_at < 0 && elapsed >= save_at) {
                if (!machine->save_state(config.home + "/state.vsf", error)) {
                    std::fprintf(stderr, "run %d: save failed: %s\n", run, error.c_str());
                    return 1;
                }
                saved_at = elapsed;
                std::printf("run %d: state saved at %.1f s, %s\n", run, elapsed, now_loading ? "loading" : "not loading");
            }
            uint32_t got = machine->read_audio(audio.data(), uint32_t(audio.size() / 2));
            samples += got;
            for (uint32_t i = 0; i < got * 2; ++i) {
                loudest = std::max<int64_t>(loudest, std::abs(int(audio[i])));
                // The SID's output rests at a level of its own, so a game's sound is how far its
                // samples move after the load.
                if (!booting) {
                    lowest_played = std::min<int>(lowest_played, audio[i]);
                    highest_played = std::max<int>(highest_played, audio[i]);
                }
            }
        }
        const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        std::printf("run %d: %.1f s of guest time in %.1f s (%.1fx real time)\n", run, elapsed, wall, elapsed / wall);
        std::printf("run %d: loading (autostart, tape or disk) for %.1f s of guest time, %s\n", run, loading,
                    was_loading ? "still at the end" : ("last ending at " + std::to_string(loading_ended) + " s").c_str());
        if (state) {
            if (after_save.empty()) {
                std::fprintf(stderr, "run %d: no state saved: the game was still loading at the end\n", run);
                return 1;
            }
            const std::string file = config.home + "/state.vsf";
            if (!machine->load_state(file, error) || !machine->run_frame(error)) {
                std::fprintf(stderr, "run %d: load failed: %s\n", run, error.c_str());
                return 1;
            }
            const std::vector<uint8_t> after_load = picture();
            const bool same = after_load == after_save;
            std::printf("run %d: state %s\n", run, same ? "restored: the frame after it matches" : "MISMATCH after load");
            if (!same) {
                uint32_t width, height;
                machine->frame(width, height);
                write_bmp(config.home + "/state-" + std::to_string(run) + "-saved.bmp", after_save.data(), width, height);
                write_bmp(config.home + "/state-" + std::to_string(run) + "-loaded.bmp", after_load.data(), width, height);
                return 1;
            }
        }
        if (save_at >= 0) {
            if (saved_at < 0) {
                std::fprintf(stderr, "run %d: no state saved: the run ended before %.1f s\n", run, save_at);
                return 1;
            }
            const std::vector<uint8_t> ended = picture();
            if (!machine->load_state(config.home + "/state.vsf", error)) {
                std::fprintf(stderr, "run %d: load failed: %s\n", run, error.c_str());
                return 1;
            }
            for (double again = saved_at; again < elapsed; again += machine->frame_seconds()) {
                if (!machine->run_frame(error)) {
                    std::fprintf(stderr, "run %d: stopped after the state: %s\n", run, error.c_str());
                    return 1;
                }
                machine->read_audio(audio.data(), uint32_t(audio.size() / 2));
            }
            const bool same = picture() == ended;
            std::printf("run %d: state from %.1f s %s\n", run, saved_at,
                        same ? "ran on to the same last picture" : "MISMATCH: a different last picture");
            if (!same) {
                uint32_t width, height;
                machine->frame(width, height);
                write_bmp(config.home + "/state-" + std::to_string(run) + "-ended.bmp", ended.data(), width, height);
                write_bmp(config.home + "/state-" + std::to_string(run) + "-again.bmp", picture().data(), width, height);
                return 1;
            }
        }
        uint32_t width, height;
        const uint8_t *rgba = machine->frame(width, height);
        std::string bmp = config.home + "/frame-" + std::to_string(run) + ".bmp";
        write_bmp(bmp, rgba, width, height);
        std::vector<uint64_t> distinct = pictures;
        std::sort(distinct.begin(), distinct.end());
        distinct.erase(std::unique(distinct.begin(), distinct.end()), distinct.end());
        std::printf("run %d: pictures after load: %zu distinct of %zu seconds; the last has %d colours, %d in "
                    "its display window\n", run, distinct.size(), pictures.size(), colours(rgba, width, height),
                    window_colours(rgba, width, height));
        // The text screen at the end, where BASIC's messages ("?FILE NOT FOUND", "READY.") show.
        std::ofstream(config.home + "/screen-" + std::to_string(run) + ".txt", std::ios::binary) << machine->screen_text();
        std::printf("run %d: %ld frames (%.4f s each), %ux%u, %llu sound frames at %d Hz, peak %lld, range after load %d, "
                    "%s%s\n", run, frames, machine->frame_seconds(), width, height, (unsigned long long) samples,
                    machine->sample_rate(), (long long) loudest, booting ? 0 : std::max(0, highest_played - lowest_played),
                    bmp.c_str(), booting ? " (still loading)" : "");
    }
    return 0;
}
