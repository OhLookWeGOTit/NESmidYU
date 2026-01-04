#include "nes/ppu.h"
#include <stdexcept>

using namespace nes;

const std::array<std::array<uint8_t,3>, 64> PPU::nes_palette_ = {{{
    {{124,124,124}},{{0,0,252}},{{0,0,188}},{{68,40,188}},{{148,0,132}},{{168,0,32}},{{168,16,0}},{{136,20,0}},
    {{80,48,0}},{{0,120,0}},{{0,104,0}},{{0,88,0}},{{0,64,88}},{{0,0,0}},{{0,0,0}},{{0,0,0}},
    {{188,188,188}},{{0,120,248}},{{0,88,248}},{{104,68,252}},{{216,0,204}},{{228,0,88}},{{248,56,0}},{{228,92,16}},
    {{172,124,0}},{{0,184,0}},{{0,168,0}},{{0,168,68}},{{0,136,136}},{{0,0,0}},{{0,0,0}},{{0,0,0}},
    {{248,248,248}},{{60,188,252}},{{104,136,252}},{{152,120,248}},{{248,120,248}},{{248,88,152}},{{248,120,88}},{{252,160,68}},
    {{248,184,0}},{{184,248,24}},{{88,216,84}},{{88,248,152}},{{0,232,216}},{{120,120,120}},{{0,0,0}},{{0,0,0}},
    {{252,252,252}},{{164,228,252}},{{184,184,248}},{{216,184,248}},{{248,184,248}},{{248,164,192}},{{240,208,176}},{{252,224,168}},
    {{248,216,120}},{{216,248,120}},{{184,248,184}},{{184,248,216}},{{0,252,252}},{{248,216,248}},{{0,0,0}},{{0,0,0}}
}};

PPU::PPU(const ROM* rom) : rom_(rom), chr_ram_(), has_chr_rom_(!rom_->chr().empty()), vram_{}, palette_{}, oam_{}, mirroring_(rom_->header().mirroring),
    ppuctrl_(0), ppumask_(0), ppustatus_(0), oamaddr_(0), ppuscroll_(0), ppuaddr_(0), ppudata_(0), vram_addr_(0), temp_addr_(0), fine_x_(0), write_toggle_(false) {
    if (!has_chr_rom_) chr_ram_.assign(8 * 1024, 0);
    for (size_t i = 0; i < palette_.size(); ++i) palette_[i] = static_cast<uint8_t>(i % 64);
}

uint8_t PPU::read_register(uint8_t reg) {
    switch (reg) {
        case 0: return ppuctrl_;
        case 1: return ppumask_;
        case 2: { uint8_t status = ppustatus_; ppustatus_ &= ~0x80; write_toggle_ = false; return status; }
        case 3: return oamaddr_;
        case 4: return oam_[oamaddr_];
        case 5: return ppuscroll_;
        case 6: return ppuaddr_;
        case 7: {
            uint8_t data = read_chr(vram_addr_);
            vram_addr_ += (ppuctrl_ & 0x04) ? 32 : 1;
            return data;
        }
        default: return 0;
    }
}

void PPU::write_register(uint8_t reg, uint8_t value) {
    switch (reg) {
        case 0: ppuctrl_ = value; temp_addr_ = (temp_addr_ & 0xF3FF) | ((value & 0x03) << 10); break;
        case 1: ppumask_ = value; break;
        case 3: oamaddr_ = value; break;
        case 4: oam_[oamaddr_++] = value; break;
        case 5: {
            if (!write_toggle_) { temp_addr_ = (temp_addr_ & 0xFFE0) | (value >> 3); fine_x_ = value & 0x07; }
            else { temp_addr_ = (temp_addr_ & 0x8FFF) | ((value & 0x07) << 12); temp_addr_ = (temp_addr_ & 0xFC1F) | ((value & 0xF8) << 2); }
            write_toggle_ = !write_toggle_;
            break;
        }
        case 6: {
            if (!write_toggle_) { temp_addr_ = (temp_addr_ & 0x80FF) | ((value & 0x3F) << 8); }
            else { temp_addr_ = (temp_addr_ & 0xFF00) | value; vram_addr_ = temp_addr_; }
            write_toggle_ = !write_toggle_;
            break;
        }
        case 7: write_chr(vram_addr_, value); vram_addr_ += (ppuctrl_ & 0x04) ? 32 : 1; break;
    }
}

uint16_t PPU::mirror_vram_addr(uint16_t addr) const {
    addr &= 0x0FFF;
    if (mirroring_ == 0) { // horizontal
        if (addr >= 0x0800) addr -= 0x0800;
    } else { // vertical
        if (addr >= 0x0400) addr &= 0x03FF;
    }
    return addr;
}

uint8_t PPU::read_chr(uint16_t addr) const {
    addr &= 0x1FFF;
    if (has_chr_rom_) {
        const auto& chr = rom_->chr();
        return chr.empty() ? 0 : chr[addr % chr.size()];
    }
    return chr_ram_[addr % chr_ram_.size()];
}

void PPU::write_chr(uint16_t addr, uint8_t value) {
    addr &= 0x1FFF;
    if (!has_chr_rom_) chr_ram_[addr % chr_ram_.size()] = value;
}

size_t PPU::chr_size() const noexcept { return has_chr_rom_ ? rom_->chr().size() : chr_ram_.size(); }

void PPU::render_pattern_table(int table_index, std::vector<uint8_t>& pixels) const {
    if (table_index < 0 || table_index > 1) throw std::out_of_range("pattern table index must be 0 or 1");
    const size_t table_size = 0x1000;
    const size_t table_offset = static_cast<size_t>(table_index) * table_size;
    pixels.assign(128 * 128, 0);

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

void PPU::render_frame(std::vector<uint8_t>& rgb_pixels) const {
    rgb_pixels.assign(256 * 240 * 3, 0);
    // Background rendering (simplified, no scrolling)
    for (int y = 0; y < 240; ++y) {
        int tile_y = y / 8, fine_y = y % 8;
        for (int x = 0; x < 256; ++x) {
            int tile_x = x / 8, fine_x = x % 8;
            int nt_index = (tile_y % 30) * 32 + (tile_x % 32);
            uint8_t tile_id = vram_[mirror_vram_addr(static_cast<uint16_t>(nt_index))];
            int attr_index = 0x03C0 + ((tile_y / 4) * 8) + (tile_x / 4);
            uint8_t attr = vram_[mirror_vram_addr(static_cast<uint16_t>(attr_index))];
            int shift = ((tile_y & 2) ? 4 : 0) + ((tile_x & 2) ? 2 : 0);
            uint8_t pal_sel = (attr >> shift) & 3;
            uint16_t tile_addr = static_cast<uint16_t>((ppuctrl_ & 0x10 ? 0x1000 : 0) + tile_id * 16 + fine_y);
            uint8_t p0 = read_chr(tile_addr), p1 = read_chr(tile_addr + 8);
            int bit = ((p0 >> (7 - fine_x)) & 1) | (((p1 >> (7 - fine_x)) & 1) << 1);
            uint8_t pal_idx = palette_[(pal_sel << 2) + bit] & 0x3F;
            const auto& col = nes_palette_[pal_idx];
            int dst = (y * 256 + x) * 3;
            rgb_pixels[dst] = col[0]; rgb_pixels[dst+1] = col[1]; rgb_pixels[dst+2] = col[2];
        }
    }
    // Sprite rendering (basic, no priority/clipping)
    for (int i = 0; i < 64; ++i) {
        uint8_t y_pos = oam_[i*4], tile_id = oam_[i*4+1], attr = oam_[i*4+2], x_pos = oam_[i*4+3];
        if (y_pos >= 240) continue;
        uint8_t pal_sel = (attr & 3) + 4; // sprite palettes
        bool flip_h = attr & 0x40, flip_v = attr & 0x80;
        for (int row = 0; row < 8; ++row) {
            int ry = flip_v ? (7 - row) : row;
            uint16_t tile_addr = static_cast<uint16_t>((ppuctrl_ & 0x08 ? 0x1000 : 0) + tile_id * 16 + ry);
            uint8_t p0 = read_chr(tile_addr), p1 = read_chr(tile_addr + 8);
            for (int col = 0; col < 8; ++col) {
                int rx = flip_h ? (7 - col) : col;
                int bit = ((p0 >> (7 - rx)) & 1) | (((p1 >> (7 - rx)) & 1) << 1);
                if (bit == 0) continue; // transparent
                uint8_t pal_idx = palette_[(pal_sel << 2) + bit] & 0x3F;
                const auto& col = nes_palette_[pal_idx];
                int px = x_pos + col, py = y_pos + row;
                if (px < 256 && py < 240) {
                    int dst = (py * 256 + px) * 3;
                    rgb_pixels[dst] = col[0]; rgb_pixels[dst+1] = col[1]; rgb_pixels[dst+2] = col[2];
                }
            }
        }
    }
}