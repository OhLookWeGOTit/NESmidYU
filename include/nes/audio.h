#pragma once
#include <cstdint>

namespace nes {
class APU {
public:
    APU();
    uint8_t read_register(uint16_t addr);
    void write_register(uint16_t addr, uint8_t value);
    // Stub: no audio output yet
};
}