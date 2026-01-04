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
    const auto& header = emu.cpu(); // just to ensure cpu exists
    std::cout << "Loaded ROM. Stepping up to " << steps << " steps\n";
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