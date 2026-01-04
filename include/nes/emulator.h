#pragma once
#include "nes/rom.h"
#include "nes/memory.h"
#include "nes/cpu.h"
#include "nes/ppu.h"
#include "nes/apu.h"
#include <memory>
#include <vector>

namespace nes {

class Emulator {
public:
    Emulator();
    void load_rom_bytes(const std::vector<uint8_t>& data);
    void reset();
    int step();
    CPU6502& cpu() { return *cpu_; }
    PPU& ppu() { return *ppu_; }
    APU& apu() { return *apu_; }

private:
    std::unique_ptr<ROM> rom_;
    std::unique_ptr<Memory> mem_;
    std::unique_ptr<CPU6502> cpu_;
    std::unique_ptr<PPU> ppu_;
    std::unique_ptr<APU> apu_;
};

}

