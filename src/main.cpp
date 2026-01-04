#include <iostream>
#include <fstream>
#include <vector>
#include "nes/emulator.h"

int main(int argc, char** argv) {
    if (argc < 2) { std::cout << "Usage: nesemu path/to/game.nes [steps]\n"; return 1; }
    std::string path = argv[1];
    int steps = (argc > 2) ? std::stoi(argv[2]) : 1000;
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) { std::cerr << "Failed to open ROM\n"; return 2; }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    nes::Emulator emu;
    try { emu.load_rom_bytes(data); } catch (const std::exception& e) { std::cerr << "ROM error: " << e.what() << "\n"; return 3; }
    emu.reset();
    std::cout << "Running " << steps << " steps...\n";
    for (int i = 0; i < steps; ++i) {
        try { emu.step(); } catch (const std::exception& e) { std::cout << "Stopped: " << e.what() << "\n"; break; }
        if (i % 100 == 0) std::cout << "Step " << i << ": " << emu.cpu().state() << "\n";
    }
    // Export frame
    std::vector<uint8_t> frame;
    emu.ppu().render_frame(frame);
    std::ofstream ofs("frame.ppm", std::ios::binary);
    if (ofs) {
        ofs << "P6\n256 240\n255\n";
        ofs.write((char*)frame.data(), frame.size());
        std::cout << "Exported frame.ppm\n";
    }
    return 0;
}