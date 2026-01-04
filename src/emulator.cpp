#include "nes/emulator.h"

using namespace nes;

Emulator::Emulator() = default;

void Emulator::load_rom_bytes(const std::vector<uint8_t>& data) {
    rom_ = std::make_unique<ROM>(data);
    apu_ = std::make_unique<APU>();
    ppu_ = std::make_unique<PPU>(rom_.get());
    mem_ = std::make_unique<Memory>(rom_.get(), ppu_.get(), apu_.get());
    cpu_ = std::make_unique<CPU6502>(mem_.get());
}

void Emulator::reset() { if (cpu_) cpu_->reset(); }

int Emulator::step() { if (!cpu_) throw std::runtime_error("No ROM loaded"); return cpu_->step(); }