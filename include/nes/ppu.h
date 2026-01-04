#pragma once
#include "nes/rom.h"
#include <cstdint>
#include <vector>
#include <array>

namespace nes {

class PPU {
public:
    explicit PPU(const ROM* rom);

    // Read CHR memory (0x0000 - 0x1FFF)
    uint8_t read_chr(uint16_t addr) const;
    void write_chr(uint16_t addr, uint8_t value); // only for CHR-RAM

    // VRAM (nametables 0x000-0x7FF)
    uint8_t read_vram(uint16_t addr) const;
    void write_vram(uint16_t addr, uint8_t value);

    // Palette RAM (32 bytes)
    uint8_t read_palette(uint16_t addr) const;
    void write_palette(uint16_t addr, uint8_t value);

    // Render helpers
    void render_pattern_table(int table_index, std::vector<uint8_t>& pixels) const; // 128x128 2-bit pixels
    void render_frame(std::vector<uint8_t>& rgb_pixels) const; // full 256x240 RGB (3 bytes per pixel)

    size_t chr_size() const noexcept;

private:
    const ROM* rom_;                 // not owned
    std::vector<uint8_t> chr_ram_;   // used if ROM has no CHR ROM
    bool has_chr_rom_;

    // PPU internal memory
    std::array<uint8_t, 0x800> vram_;     // nametables + attribute table area
    std::array<uint8_t, 0x20> palette_;   // 32 bytes palette RAM

    static const std::array<std::array<uint8_t,3>, 64> nes_palette_;
};

}