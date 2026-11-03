// The firmware and model passes' runner (tools/firmware_pass.py, tools/model_pass.py): boots a C64
// game on the host for a while, pressing keys after its load, and reports what the passes judge the
// boot by. Not a boot test: those are the shared PC boot test's, on core/test/boot-tests.json
// (Kairo64#65).
//
// Usage: firmware_pass_run <firmware folder> <work folder> <game> <seconds> <system> <keys> <model>
//
// <system> picks the ROMs and the drive, as a game's System and Drive settings do: "own" is the
// player's firmware in <firmware folder> (kernal, basic and chargen, as the retro firmware sets name
// them), "open" the MEGA65 Open ROMs the app ships (third_party/open-roms); either runs VICE's
// virtual drive, and with "+1541" ("own+1541", "open+1541") an emulated 1541 on the player's
// dos1541. A hard disk image (.dhd, .hdd, .cfa) starts at BASIC's prompt on its interface: a CMD HD
// on the firmware folder's cmdhd.bin, or the IDE64 on its ide64.bin.
//
// <keys> is a list of "+frame:row:column" presses (comma separated) in the keyboard matrix, timed
// from the end of the load and each held for five frames; a row of "j" is a joystick control
// instead (column names it). None is pressed if the load never ends. <model> is a VICE model name.
//
// Runs the machine for <seconds> of guest time, writes its last picture to <work folder>/frame.bmp
// and its text screen to <work folder>/screen.txt, and prints one JSON line:
//   loading        guest seconds the app would run flat out (C64Machine::loading)
//   loadingAtEnd   whether that was still going at the end
//   stillLoading   whether the autostart never finished
//   range          how far the sound moved after the load (the SID rests at a level of its own)
//   pictures       distinct pictures after the load, one taken each guest second
//   colours        colours in the last picture's display window, up to 17
//   raster         the highest raster line an enabled raster interrupt waited on after the load,
//                  or null for none (263 or later exists only on a PAL C64, Kairo64#14)
//   error          why the machine could not start or stopped, or null
// The runner itself fails (exit 2) only on bad arguments or missing firmware.

#include <algorithm>
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

// How many colours the VIC-II's 320 x 200 display window has (inside the border), up to 17: one
// for a blank screen, two for text on its background. The window starts 32 pixels in and, below
// VICE's normal top border, 35 lines down on a PAL frame (272 lines) or 23 on an NTSC one (247).
int window_colours(const uint8_t *rgba, uint32_t width, uint32_t height) {
    const uint32_t top = height == 272 ? 35 : height == 247 ? 23 : UINT32_MAX;
    if (width != 384 || top == UINT32_MAX) {
        std::fprintf(stderr, "no display window known for a %ux%u frame\n", width, height);
        std::exit(2);
    }
    std::vector<uint32_t> seen;
    for (uint32_t y = top; y < top + 200 && seen.size() <= 16; ++y) {
        for (uint32_t x = 32; x < 352 && seen.size() <= 16; ++x) {
            const uint8_t *p = rgba + (size_t(y) * width + x) * 4;
            const uint32_t pixel = uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16;
            if (std::find(seen.begin(), seen.end(), pixel) == seen.end()) seen.push_back(pixel);
        }
    }
    return int(seen.size());
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
    // Frames after the end of the load.
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
        const auto first = item.find(':');
        const auto second = item.find(':', first + 1);
        if (item.empty() || item[0] != '+' || first == std::string::npos || second == std::string::npos) {
            std::fprintf(stderr, "bad key press %s\n", item.c_str());
            std::exit(2);
        }
        const std::string row = item.substr(first + 1, second - first - 1);
        presses.push_back({std::stol(item.substr(1, first - 1)), row == "j" ? -1 : std::stoi(row),
                           std::stoi(item.substr(second + 1))});
    }
    return presses;
}

std::string require(const std::filesystem::path &path) {
    if (!std::filesystem::is_regular_file(path)) {
        std::fprintf(stderr, "missing firmware %s\n", path.string().c_str());
        std::exit(2);
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
    } else if (system != roms) {
        return false;
    }
    return true;
}

std::string lower_extension(const std::string &path) {
    std::string extension = std::filesystem::path(path).extension().string();
    for (char &c : extension) c = char(std::tolower(static_cast<unsigned char>(c)));
    return extension;
}

std::string json_text(const std::string &text) {
    std::string out = "\"";
    for (const char c : text) {
        if (c == '"' || c == '\\') out += '\\';
        if (static_cast<unsigned char>(c) < 0x20) {
            char escaped[8];
            std::snprintf(escaped, sizeof(escaped), "\\u%04x", c);
            out += escaped;
            continue;
        }
        out += c;
    }
    return out + "\"";
}

struct Result {
    double loading = 0;
    bool loading_at_end = false;
    bool still_loading = false;
    int range = 0;
    size_t pictures = 0;
    int colours = 0;
    // The highest raster interrupt line, -1 for none.
    int raster = -1;
    std::string error;
};

void report(const Result &result) {
    std::printf("{\"loading\": %.2f, \"loadingAtEnd\": %s, \"stillLoading\": %s, \"range\": %d, \"pictures\": %zu, "
                "\"colours\": %d, \"raster\": %s, \"error\": %s}\n",
                result.loading, result.loading_at_end ? "true" : "false", result.still_loading ? "true" : "false",
                result.range, result.pictures, result.colours,
                result.raster < 0 ? "null" : std::to_string(result.raster).c_str(),
                result.error.empty() ? "null" : json_text(result.error).c_str());
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 8) {
        std::fprintf(stderr, "usage: firmware_pass_run <firmware folder> <work folder> <game> <seconds> <system> <keys> <model>\n");
        return 2;
    }
    const std::filesystem::path firmware = argv[1];
    kairo64::Config config;
    config.home = argv[2];
    std::filesystem::create_directories(config.home);
    config.start = argv[3];
    const double seconds = std::atof(argv[4]);
    if (!set_system(config, argv[5], firmware)) {
        std::fprintf(stderr, "unknown system %s\n", argv[5]);
        return 2;
    }
    const std::vector<Press> presses = *argv[6] ? parse_keys(argv[6]) : std::vector<Press>{};
    config.model = argv[7];

    const std::string extension = lower_extension(config.start);
    if (extension == ".dhd" || extension == ".hdd" || extension == ".cfa") {
        config.hard_disk = config.start;
        (extension == ".dhd" ? config.cmdhd_rom : config.ide64_rom) =
            require(firmware / (extension == ".dhd" ? "cmdhd.bin" : "ide64.bin"));
        config.start.clear();
    }
    if (extension == ".d64" || extension == ".g64" || extension == ".d71" || extension == ".d81") config.drive8 = config.start;

    Result result;
    if (!config.start.empty() && !std::ifstream(config.start, std::ios::binary)) {
        result.error = "cannot open " + config.start;
        report(result);
        return 0;
    }
    std::string error;
    auto machine = kairo64::C64Machine::create(config, error);
    if (!machine) {
        result.error = error;
        report(result);
        return 0;
    }
    std::vector<int16_t> audio(48000 * 2);
    double elapsed = 0;
    long frames = 0;
    bool booting = machine->autostarting();
    // The frame the load ended on, for the presses; -1 while it runs.
    long loaded_frame = booting ? -1 : 0;
    bool was_loading = machine->loading();
    std::vector<uint64_t> pictures;
    int lowest = 32767, highest = -32768;
    while (elapsed < seconds) {
        for (const auto &press : presses) {
            if (loaded_frame < 0) break;
            for (const bool down : {true, false}) {
                if (frames != loaded_frame + press.frame + (down ? 0 : 5)) continue;
                if (press.row < 0) machine->joystick(press.column, down);
                else machine->key(press.row, press.column, down);
            }
        }
        if (!machine->run_frame(error)) {
            result.error = error;
            report(result);
            return 0;
        }
        // Every line a raster interrupt waited on during the frame; those of the load are dropped.
        for (const unsigned line : machine->raster_irq_lines())
            if (!booting) result.raster = std::max(result.raster, int(line));
        was_loading = machine->loading();
        if (was_loading) result.loading += machine->frame_seconds();
        if (booting && !machine->autostarting()) {
            booting = false;
            loaded_frame = frames + 1;
        }
        const double before = elapsed;
        elapsed += machine->frame_seconds();
        // A fingerprint of the picture each second after the load, to tell a game that moves from
        // one stuck on a still picture.
        if (!booting && int(elapsed) != int(before)) {
            uint32_t width, height;
            const uint8_t *rgba = machine->frame(width, height);
            pictures.push_back(fingerprint(rgba, width, height));
        }
        ++frames;
        const uint32_t got = machine->read_audio(audio.data(), uint32_t(audio.size() / 2));
        if (!booting) {
            for (uint32_t i = 0; i < got * 2; ++i) {
                lowest = std::min<int>(lowest, audio[i]);
                highest = std::max<int>(highest, audio[i]);
            }
        }
    }
    result.loading_at_end = was_loading;
    result.still_loading = booting;
    result.range = booting ? 0 : std::max(0, highest - lowest);
    std::sort(pictures.begin(), pictures.end());
    result.pictures = size_t(std::unique(pictures.begin(), pictures.end()) - pictures.begin());
    uint32_t width, height;
    const uint8_t *rgba = machine->frame(width, height);
    result.colours = window_colours(rgba, width, height);
    write_bmp(config.home + "/frame.bmp", rgba, width, height);
    // The text screen at the end, where BASIC's messages ("?FILE NOT FOUND", "READY.") show.
    std::ofstream(config.home + "/screen.txt", std::ios::binary) << machine->screen_text();
    report(result);
    return 0;
}
