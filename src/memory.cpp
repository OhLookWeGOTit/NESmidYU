#include "nes/memory.h"
using namespace nes;

Memory::Memory(const ROM* rom) : ram_{}, rom_(rom) {}

uint8_t Memory::read(uint16_t addr) const {
    if (addr <= 0x1FFF) {
        return ram_[addr & 0x07FF];
    }
    if (addr >= 0x8000) {
        const auto& prg = rom_->prg();
        if (prg.size() == 0x4000) {
            size_t base = (addr - 0x8000) % 0x4000;
            return prg[base];
        } else if (!prg.empty()) {
            size_t base = addr - 0x8000;
            if (base < prg.size()) return prg[base];
        }
    }
    return 0;
}

void Memory::write(uint16_t addr, uint8_t value) {
    if (addr <= 0x1FFF) {
        ram_[addr & 0x07FF] = value;
    }
}