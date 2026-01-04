#include "nes/ppu.h"
#include <stdexcept>

using namespace nes;

PPU::PPU(const ROM* rom) : rom_(rom) {
    has_chr_rom_ = !rom_->chr().empty();
    if (!has_chr_rom_) {
        // Provide 8 KB CHR RAM as fallback
        chr_ram_.assign(8 * 1024, 0);
    }
}

uint8_t PPU::read_chr(uint16_t addr) const {
    addr &= 0x1FFF;
    if (has_chr_rom_) {
        const auto& chr = rom_->chr();
        if (chr.empty()) return 0;
        // Mirror if smaller than 8KB
        size_t idx = static_cast<size_t>(addr) % chr.size();
        return chr[idx];
    } else {
        size_t idx = static_cast<size_t>(addr) % chr_ram_.size();
        return chr_ram_[idx];
    }
}

void PPU::write_chr(uint16_t addr, uint8_t value) {
    addr &= 0x1FFF;
    if (has_chr_rom_) {
        // Writes ignored to CHR ROM
        return;
    } else {
        size_t idx = static_cast<size_t>(addr) % chr_ram_.size();
        chr_ram_[idx] = value;
    }
}

void PPU::render_pattern_table(int table_index, std::vector<uint8_t>& pixels) const {
    if (table_index < 0 || table_index > 1) throw std::out_of_range("pattern table index must be 0 or 1");
    const size_t table_size = 0x1000; // 4 KB per pattern table
    const size_t table_offset = static_cast<size_t>(table_index) * table_size;
    pixels.assign(128 * 128, 0);

    // 256 tiles per table, each tile 16 bytes (8 bytes plane0, 8 bytes plane1)
    for (int tile = 0; tile < 256; ++tile) {
        size_t tile_base = table_offset + static_cast<size_t>(tile) * 16;
        int tile_x = (tile % 16) * 8;
        int tile_y = (tile / 16) * 8;
        for (int row = 0; row < 8; ++row) {
            uint8_t plane0 = read_chr(static_cast<uint16_t>(tile_base + row));
            uint8_t plane1 = read_chr(static_cast<uint16_t>(tile_base + row + 8));
            for (int col = 0; col < 8; ++col) {
                int bit = ((plane0 >> (7 - col)) & 1) | (((plane1 >> (7 - col)) & 1) << 1);
                int px = tile_x + col;
                int py = tile_y + row;
                pixels[py * 128 + px] = static_cast<uint8_t>(bit);
            }
        }
    }
}

size_t PPU::chr_size() const noexcept {
    if (has_chr_rom_) return rom_->chr().size();
    return chr_ram_.size();
}