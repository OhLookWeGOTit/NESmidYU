#pragma once
#include "nes/rom.h"
#include "nes/visual.h"
#include "nes/audio.h"
#include <cstdint>
#include <array>

namespace nes {
class MemoryMap {
public:
    MemoryMap(const RomLoader* rom, VisualProcessor* visual, AudioProcessor* audio);
    uint8_t fetch(uint16_t address) const;
    void store(uint16_t address, uint8_t value);

private:
    std::array<uint8_t, 0x0800> internal_ram_;
    const RomLoader* rom_;
    VisualProcessor* visual_;
    AudioProcessor* audio_;
};
}