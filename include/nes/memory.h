#pragma once
#include "nes/rom.h"     // Ensure this file exists and defines ROM
#include "nes/visual.h"  // Includes PPU class
#include "nes/audio.h"   // Includes APU class
#include <cstdint>
#include <array>

namespace nes {
class Memory {
public:
    Memory(const ROM* rom, PPU* ppu, APU* apu);
    uint8_t read(uint16_t addr) const;
    void write(uint16_t addr, uint8_t value);
    void oam_dma(uint8_t page);

private:
    std::array<uint8_t, 0x0800> ram_;
    const ROM* rom_;
    PPU* ppu_;
    APU* apu_;
};
}