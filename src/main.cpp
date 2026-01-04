#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include "nes/emulator.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: nesemu path/to/game.nes [steps]\n";
        return 1;
    }
    const char* path = argv[1];
    int steps = (argc > 2) ? std::stoi(argv[2]) : 1000;
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        std::cerr << "Failed to open ROM: " << path << "\n";
        return 2;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    nes::Emulator emu;
    try {
        emu.load_rom_bytes(data);
    } catch (const std::exception& e) {
        std::cerr << "ROM load error: " << e.what() << "\n";
        return 3;
    }
    emu.reset();
    std::cout << "Loaded ROM. Exporting pattern tables and frame...\n";

    // Export pattern tables
    for (int tbl = 0; tbl < 2; ++tbl) {
        std::vector<uint8_t> pixels;
        emu.ppu().render_pattern_table(tbl, pixels);
        std::string fname = "pattern" + std::to_string(tbl) + ".pgm";
        std::ofstream ofs(fname, std::ios::binary);
        if (!ofs) { std::cerr << "Failed to open " << fname << "\n"; continue; }
        ofs << "P5\n128 128\n255\n";
        for (auto v : pixels) ofs.put(static_cast<char>(v * 85)); // 0..3 -> 0,85,170,255
        ofs.close();
        std::cout << "Wrote " << fname << "\n";
    }

    // Export full 256x240 frame (PPM P6)
    std::vector<uint8_t> frame;
    emu.ppu().render_frame(frame);
    std::ofstream ofs("frame.ppm", std::ios::binary);
    if (ofs) {
        ofs << "P6\n256 240\n255\n";
        ofs.write(reinterpret_cast<const char*>(frame.data()), static_cast<std::streamsize>(frame.size()));
        ofs.close();
        std::cout << "Wrote frame.ppm\n";
    } else {
        std::cerr << "Failed to write frame.ppm\n";
    }

    std::cout << "Stepping up to " << steps << " steps\n";
    for (int i = 0; i < steps; ++i) {
        try {
            emu.step();
        } catch (const std::exception& e) {
            std::cout << "Execution stopped: " << e.what() << "\n";
            break;
        }
        if ((i % 100) == 0) {
            std::cout << "Step " << i << " " << emu.cpu().state() << "\n";
        }
    }
    std::cout << "Final CPU state: " << emu.cpu().state() << "\n";
    return 0;
}