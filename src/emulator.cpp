#include "nes/emulator.h"

using namespace nes;

Emulator::Emulator() = default;

void Emulator::load_rom_bytes(const std::vector<uint8_t>& data) {
    rom_ = std::make_unique<ROM>(data);
    apu_ = std::make_unique<APU>();
    ppu_ = std::make_unique<PPU>(rom_.get(), cpu_.get()); // Pass CPU
    mem_ = std::make_unique<Memory>(rom_.get(), ppu_.get(), apu_.get());
    cpu_ = std::make_unique<CPU6502>(mem_.get());
}

void Emulator::reset() { if (cpu_) cpu_->reset(); }

void Emulator::step() {
    if (!cpu_) throw std::runtime_error("No ROM loaded");
    cpu_->step();
    ppu_->step(); // Step PPU each CPU cycle (approx 3 PPU cycles per CPU)
    if (ppu_->nmi_triggered()) cpu_->trigger_nmi();
}