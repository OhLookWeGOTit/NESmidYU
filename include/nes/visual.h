#pragma once
#include "nes/rom.h"
#include <cstdint>
#include <vector>
#include <array>

namespace nes {
class CPU6502;

class PPU {
public:
    explicit PPU(const ROM* rom, CPU6502* cpu);

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

    uint8_t read_register(uint8_t reg);
    void write_register(uint8_t reg, uint8_t value);
    void step();
    bool nmi_triggered() const;
    void render_scanline(); // For real-time rendering

private:
    const ROM* rom_;                 // not owned
    std::vector<uint8_t> chr_ram_;   // used if ROM has no CHR ROM
    bool has_chr_rom_;

    // PPU internal memory
    std::array<uint8_t, 0x800> vram_;     // nametables + attribute table area
    std::array<uint8_t, 0x20> palette_;   // 32 bytes palette RAM
    std::array<uint8_t, 0x100> oam_;      // sprite OAM
    uint8_t mirroring_;                 // 0=horizontal, 1=vertical

    // Registers
    uint8_t ppuctrl_, ppumask_, ppustatus_, oamaddr_, ppuscroll_, ppuaddr_, ppudata_;
    uint16_t vram_addr_, temp_addr_;
    uint8_t fine_x_;
    bool write_toggle_;

    // New: Scrolling and rendering state
    uint16_t coarse_x_, coarse_y_, fine_y_;
    bool sprite_zero_hit_, sprite_overflow_;
    std::array<uint8_t, 256> scanline_pixels_; // For scanline rendering
    std::array<uint8_t, 256> scanline_palettes_;
    std::vector<std::array<uint8_t, 4>> sprite_buffer_; // Up to 8 sprites per scanline

    static const std::array<std::array<uint8_t,3>, 64> nes_palette_;
    uint16_t mirror_vram_addr(uint16_t addr) const;

    void fetch_background();
    void fetch_sprites();
    void render_pixel(int x, uint8_t bg_pixel, uint8_t bg_pal, uint8_t sp_pixel, uint8_t sp_pal, bool sp_priority);
};

}