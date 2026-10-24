// Boots a C64 game on the host and runs it for a while without Android.
//
// Usage: boot_test <firmware folder> <work folder> <game> <seconds> [system [runs [keys [model [state]]]]]
//
// <system> picks the ROMs and the drive, as a game's System and Drive settings do: "own" (the
// default) is the player's firmware in <firmware folder> (kernal, basic and chargen, as the retro
// firmware sets name them), "open" the MEGA65 Open ROMs the app ships (third_party/open-roms);
// either runs VICE's virtual drive, and with "+1541" ("own+1541", "open+1541") an emulated 1541 on
// the player's dos1541.
//
// Runs the machine for <seconds> of guest time, writing a frame every five seconds to
// <work folder>/frame-<run>-<seconds>s.bmp and the last to <work folder>/frame-<run>.bmp, and
// reports the frame size and how much sound it made, and writes the text screen to
// <work folder>/screen-<run>.txt. With <runs> above 1 it starts a new machine
// after each, in the same process, as the app does for the next game. It reports when the load
// ends (C64Machine::booting) and writes that frame to <work folder>/frame-<run>-booted.bmp.
// <keys> is a list of "frame:row:column" presses (comma separated) in the keyboard matrix, each
// held for five frames; a row of "j" is a joystick control instead, held as long. <model> is a
// VICE model name ("" for the default). With <state> "state", it saves the machine halfway between
// the end of the load and the end of the run, notes the frame after, and at the end loads the
// state and checks that the same frame follows.

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
    const std::string system = argc > 5 && *argv[5] ? argv[5] : "own";
    const int runs = argc > 6 ? std::atoi(argv[6]) : 1;
    const std::vector<Press> presses = argc > 7 && *argv[7] ? parse_keys(argv[7]) : std::vector<Press>{};
    if (argc > 8 && *argv[8]) config.model = argv[8];
    const bool state = argc > 9 && std::string(argv[9]) == "state";

    // The Open ROMs, beside this source in the checkout.
    const std::filesystem::path open = std::filesystem::path(__FILE__).parent_path() / "../../third_party/open-roms/bin";
    const std::string roms = system.substr(0, system.find('+'));
    if (roms == "own") {
        config.kernal = require(firmware / "kernal");
        config.basic = require(firmware / "basic");
        config.chargen = require(firmware / "chargen");
    } else if (roms == "open") {
        config.kernal = require(open / "kernal_generic.rom");
        config.basic = require(open / "basic_generic.rom");
        config.chargen = require(open / "chargen_openroms.rom");
        config.t64_on_disk = true;
    } else {
        std::fprintf(stderr, "unknown system %s\n", system.c_str());
        return 2;
    }
    if (system == roms + "+1541") {
        config.dos1541 = require(firmware / "dos1541");
    } else if (system != roms) {
        std::fprintf(stderr, "unknown system %s\n", system.c_str());
        return 2;
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
        auto machine = kairo64::C64Machine::create(config, error);
        if (!machine) {
            std::fprintf(stderr, "run %d: could not start: %s\n", run, error.c_str());
            return 1;
        }
        double elapsed = 0;
        long frames = 0;
        const auto started = std::chrono::steady_clock::now();
        std::vector<uint8_t> after_save;
        auto picture = [&] {
            uint32_t width, height;
            const uint8_t *rgba = machine->frame(width, height);
            return std::vector<uint8_t>(rgba, rgba + size_t(width) * height * 4);
        };
        bool booting = machine->booting();
        double loaded = 0;
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
            if (booting && !machine->booting()) {
                booting = false;
                loaded = elapsed;
                std::printf("run %d: load over after %ld frames (%.1f s)\n", run, frames, elapsed);
                uint32_t width, height;
                const uint8_t *rgba = machine->frame(width, height);
                write_bmp(config.home + "/frame-" + std::to_string(run) + "-booted.bmp", rgba, width, height);
            }
            const double before = elapsed;
            elapsed += machine->frame_seconds();
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
        uint32_t width, height;
        const uint8_t *rgba = machine->frame(width, height);
        std::string bmp = config.home + "/frame-" + std::to_string(run) + ".bmp";
        write_bmp(bmp, rgba, width, height);
        // The text screen at the end, where BASIC's messages ("?FILE NOT FOUND", "READY.") show.
        std::ofstream(config.home + "/screen-" + std::to_string(run) + ".txt", std::ios::binary) << machine->screen_text();
        std::printf("run %d: %ld frames (%.4f s each), %ux%u, %llu sound frames at %d Hz, peak %lld, range after load %d, "
                    "%s%s\n", run, frames, machine->frame_seconds(), width, height, (unsigned long long) samples,
                    machine->sample_rate(), (long long) loudest, booting ? 0 : std::max(0, highest_played - lowest_played),
                    bmp.c_str(), booting ? " (still loading)" : "");
    }
    return 0;
}
