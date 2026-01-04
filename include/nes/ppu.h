#pragma once
#include "nes/rom.h"
#include <cstdint>
#include <vector>

namespace nes {

class PPU {
public:
    explicit PPU(const ROM* rom);

    // Read CHR memory (0x0000 - 0x1FFF)
    uint8_t read_chr(uint16_t addr) const;

    // If ROM has no CHR (CHR RAM), allow writes
    void write_chr(uint16_t addr, uint8_t value);

    // Render a pattern table (0 or 1) into a 128x128 pixel buffer
    // Each pixel is a 0..3 value (2-bit output combining two bitplanes)
    void render_pattern_table(int table_index, std::vector<uint8_t>& pixels) const;

    // Get CHR size in bytes (0 if no CHR ROM)
    size_t chr_size() const noexcept;

private:
    const ROM* rom_;                 // not owned
    std::vector<uint8_t> chr_ram_;   // used if ROM has no CHR ROM
    bool has_chr_rom_;
};

}