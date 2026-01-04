#include "nes/emulator.h"
#include <stdexcept>

using namespace nes;

Emulator::Emulator() = default;

void Emulator::load_rom_bytes(const std::vector<uint8_t>& data) {
    rom_ = std::make_unique<ROM>(data);
    mem_ = std::make_unique<Memory>(rom_.get());
    cpu_ = std::make_unique<CPU6502>(mem_.get());
    ppu_ = std::make_unique<PPU>(rom_.get());
}

void Emulator::reset() {
    if (cpu_) cpu_->reset();
}

int Emulator::step() {
    if (!cpu_) throw std::runtime_error("No ROM loaded");
    return cpu_->step();
}

CPU6502& Emulator::cpu() { return *cpu_; }
PPU& Emulator::ppu() { return *ppu_; }