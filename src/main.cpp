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
    std::cout << "Loaded ROM. Exporting pattern tables...\n";

    // Export pattern table 0 and 1 as 128x128 PGM images (values 0..3 -> scaled to 0..255)
    for (int tbl = 0; tbl < 2; ++tbl) {
        std::vector<uint8_t> pixels;
        emu.ppu().render_pattern_table(tbl, pixels);
        std::string fname = "pattern" + std::to_string(tbl) + ".pgm";
        std::ofstream ofs(fname, std::ios::binary);
        if (!ofs) {
            std::cerr << "Failed to open " << fname << " for writing\n";
            continue;
        }
        ofs << "P5\n128 128\n255\n";
        for (auto v : pixels) ofs.put(static_cast<char>(v * 85)); // 0..3 -> 0,85,170,255
        ofs.close();
        std::cout << "Wrote " << fname << "\n";
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