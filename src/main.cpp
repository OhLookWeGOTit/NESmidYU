#include <iostream>
#include <fstream>
#include <vector>
#include "nes/emulator.h"
#include "nes/vulkan_renderer.h"

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
        try {
            emu.step(); // Now steps CPU + PPU
        } catch (const std::exception& e) {
            std::cout << "Execution stopped: " << e.what() << "\n";
            break;
        }
        if ((i % 100) == 0) {
            std::cout << "Step " << i << " " << emu.cpu().state() << "\n";
        }
    }
    std::cout << "Final CPU state: " << emu.cpu().state() << "\n";

    // Initialize Vulkan renderer
    VulkanRenderer renderer;
    renderer.init(256, 240);

    // Main loop
    while (true) {
        emu.step();
        std::vector<uint8_t> frame;
        emu.ppu().render_frame(frame);
        renderer.update_frame(frame);
        renderer.render();
        // Handle input/window events...
    }

    // Export audio (1 second at 44.1kHz)
    std::vector<int16_t> audio_buffer;
    emu.apu().generate_audio(44100, audio_buffer);
    std::ofstream audio_file("audio.wav", std::ios::binary);
    if (audio_file) {
        // Simple WAV header
        audio_file.write("RIFF", 4);
        uint32_t file_size = 36 + audio_buffer.size() * 2;
        audio_file.write((char*)&file_size, 4);
        audio_file.write("WAVE", 4);
        audio_file.write("fmt ", 4);
        uint32_t fmt_size = 16;
        audio_file.write((char*)&fmt_size, 4);
        uint16_t format = 1, channels = 1, bits = 16;
        uint32_t sample_rate = 44100, byte_rate = sample_rate * channels * bits / 8;
        uint16_t block_align = channels * bits / 8;
        audio_file.write((char*)&format, 2);
        audio_file.write((char*)&channels, 2);
        audio_file.write((char*)&sample_rate, 4);
        audio_file.write((char*)&byte_rate, 4);
        audio_file.write((char*)&block_align, 2);
        audio_file.write((char*)&bits, 2);
        audio_file.write("data", 4);
        uint32_t data_size = audio_buffer.size() * 2;
        audio_file.write((char*)&data_size, 4);
        audio_file.write((char*)audio_buffer.data(), data_size);
        std::cout << "Exported audio.wav\n";
    }
    return 0;
}