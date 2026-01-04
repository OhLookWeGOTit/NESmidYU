#include "nes/memory.h"
using namespace nes;

MemoryMap::MemoryMap(const RomLoader* rom, VisualProcessor* visual, AudioProcessor* audio)
    : internal_ram_{}, rom_(rom), visual_(visual), audio_(audio) {}

uint8_t MemoryMap::fetch(uint16_t address) const {
    address &= 0xFFFF;
    if (address < 0x2000) return internal_ram_[address & 0x07FF];
    if (address < 0x4000) return visual_->read_port((address - 0x2000) & 0x07);
    if (address < 0x4020) {
        if (address == 0x4016 || address == 0x4017) return 0; // input stub
        return audio_->read_port(address);
    }
    if (address >= 0x8000) {
        const auto& prog = rom_->get_program();
        if (prog.size() == 0x4000) return prog[address & 0x3FFF];
        return prog[address - 0x8000];
    }
    return 0;
}

void MemoryMap::store(uint16_t address, uint8_t value) {
    address &= 0xFFFF;
    value &= 0xFF;
    if (address < 0x2000) internal_ram_[address & 0x07FF] = value;
    else if (address < 0x4000) visual_->write_port((address - 0x2000) & 0x07, value);
    else if (address < 0x4020) {
        if (address == 0x4016) {} // input latch stub
        else audio_->write_port(address, value);
    }
    else if (address == 0x4014) {
        oam_dma(value); // Trigger OAM DMA
    }
}

void MemoryMap::oam_dma(uint8_t page) {
    ppu_->oam_dma(page);
}