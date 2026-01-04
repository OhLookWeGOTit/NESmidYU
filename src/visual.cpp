#include "nes/visual.h"
#include "nes/cpu.h"
#include <stdexcept>
#include <algorithm>

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

PPU::PPU(const ROM* rom, CPU6502* cpu) : rom_(rom), cpu_(cpu), chr_ram_(), has_chr_rom_(!rom_->chr().empty()), vram_{}, palette_{}, oam_{}, mirroring_(rom_->header().mirroring),
    ppuctrl_(0), ppumask_(0), ppustatus_(0), oamaddr_(0), ppuscroll_(0), ppuaddr_(0), ppudata_(0), vram_addr_(0), temp_addr_(0), fine_x_(0), write_toggle_(false),
    scanline_(-1), cycle_(0), scroll_x_(0), scroll_y_(0), nametable_base_(0), nmi_pending_(false),
    coarse_x_(0), coarse_y_(0), fine_y_(0), sprite_zero_hit_(false), sprite_overflow_(false), scanline_pixels_{}, scanline_palettes_{}, sprite_buffer_() {
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
        case 0: ppuctrl_ = value; temp_addr_ = (temp_addr_ & 0xF3FF) | ((value & 0x03) << 10); nametable_base_ = (value & 0x03) * 0x0400; break;
        case 1: ppumask_ = value; break;
        case 3: oamaddr_ = value; break;
        case 4: oam_[oamaddr_++] = value; break;
        case 5: {
            if (!write_toggle_) { scroll_x_ = value; temp_addr_ = (temp_addr_ & 0xFFE0) | (value >> 3); fine_x_ = value & 0x07; }
            else { scroll_y_ = value; temp_addr_ = (temp_addr_ & 0x8FFF) | ((value & 0x07) << 12); temp_addr_ = (temp_addr_ & 0xFC1F) | ((value & 0xF8) << 2); }
            write_toggle_ = !write_toggle_;
            break;
        }
        case 6: {
            if (!write_toggle_) { temp_addr_ = (temp_addr_ & 0x80FF) | ((value & 0x3F) << 8); }
            else { temp_addr_ = (temp_addr_ & 0xFF00) | value; vram_addr_ = temp_addr_; coarse_x_ = vram_addr_ & 0x1F; coarse_y_ = (vram_addr_ >> 5) & 0x1F; fine_y_ = (vram_addr_ >> 12) & 0x07; }
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

void PPU::step() {
    cycle_++;
    if (cycle_ > 340) {
        cycle_ = 0;
        scanline_++;
        if (scanline_ == 240) render_scanline(); // Render visible scanlines
        if (scanline_ > 260) { scanline_ = -1; sprite_zero_hit_ = false; sprite_overflow_ = false; }
    }

    if (scanline_ >= 0 && scanline_ < 240) {
        if (cycle_ >= 1 && cycle_ <= 256) fetch_background();
        if (cycle_ == 257) fetch_sprites();
        if (cycle_ >= 1 && cycle_ <= 256) render_pixel(cycle_ - 1, scanline_pixels_[cycle_ - 1], scanline_palettes_[cycle_ - 1], 0, 0, false); // Simplified
    }

    if (scanline_ == 241 && cycle_ == 1) {
        ppustatus_ |= 0x80;
        if (ppuctrl_ & 0x80) nmi_pending_ = true;
    }
    if (scanline_ == -1 && cycle_ == 1) ppustatus_ &= ~0x80;
}

void PPU::fetch_background() {
    // Simplified: Fetch tile and attribute for current position
    int x = cycle_ - 1 + scroll_x_;
    int y = scanline_ + scroll_y_;
    int tile_x = x / 8, tile_y = y / 8;
    int nt_index = (tile_y % 30) * 32 + (tile_x % 32);
    uint8_t tile_id = vram_[mirror_vram_addr(nametable_base_ + nt_index)];
    int attr_index = nametable_base_ + 0x03C0 + ((tile_y / 4) * 8) + (tile_x / 4);
    uint8_t attr = vram_[mirror_vram_addr(attr_index)];
    int shift = ((tile_y & 2) ? 4 : 0) + ((tile_x & 2) ? 2 : 0);
    uint8_t pal_sel = (attr >> shift) & 3;
    uint16_t tile_addr = (ppuctrl_ & 0x10 ? 0x1000 : 0) + tile_id * 16 + (y % 8);
    uint8_t p0 = read_chr(tile_addr), p1 = read_chr(tile_addr + 8);
    int bit = ((p0 >> (7 - (x % 8))) & 1) | (((p1 >> (7 - (x % 8))) & 1) << 1);
    scanline_pixels_[cycle_ - 1] = bit;
    scanline_palettes_[cycle_ - 1] = pal_sel;
}

void PPU::fetch_sprites() {
    sprite_buffer_.clear();
    for (int i = 0; i < 64; ++i) {
        uint8_t y = oam_[i*4];
        if (scanline_ >= y && scanline_ < y + 8) {
            if (sprite_buffer_.size() < 8) {
                sprite_buffer_.push_back({oam_[i*4], oam_[i*4+1], oam_[i*4+2], oam_[i*4+3]});
            } else {
                sprite_overflow_ = true;
            }
        }
    }
}

void PPU::render_pixel(int x, uint8_t bg_pixel, uint8_t bg_pal, uint8_t sp_pixel, uint8_t sp_pal, bool sp_priority) {
    uint8_t pixel = bg_pixel, pal = bg_pal;
    for (const auto& sprite : sprite_buffer_) {
        int sx = sprite[3];
        if (x >= sx && x < sx + 8) {
            int col = x - sx;
            uint16_t tile_addr = (ppuctrl_ & 0x08 ? 0x1000 : 0) + sprite[1] * 16 + (scanline_ - sprite[0]);
            uint8_t p0 = read_chr(tile_addr), p1 = read_chr(tile_addr + 8);
            int bit = ((p0 >> (7 - col)) & 1) | (((p1 >> (7 - col)) & 1) << 1);
            if (bit != 0) {
                if (bg_pixel == 0 || !(sprite[2] & 0x20)) { // Priority
                    pixel = bit; pal = (sprite[2] & 3) + 4;
                    if (i == 0 && bg_pixel != 0) sprite_zero_hit_ = true;
                }
            }
        }
    }
    // Store in frame buffer (for Vulkan or export)
}

void PPU::render_scanline() {
    // Accumulate scanline into full frame
}

void PPU::render_frame(std::vector<uint8_t>& rgb_pixels) const {
    // Simplified: Render all at once for export
    rgb_pixels.assign(256 * 240 * 3, 0);
    for (int y = 0; y < 240; ++y) {
        for (int x = 0; x < 256; ++x) {
            uint8_t pal_idx = palette_[0] & 0x3F; // Default
            const auto& col = nes_palette_[pal_idx];
            int dst = (y * 256 + x) * 3;
            rgb_pixels[dst] = col[0]; rgb_pixels[dst+1] = col[1]; rgb_pixels[dst+2] = col[2];
        }
    }
}
