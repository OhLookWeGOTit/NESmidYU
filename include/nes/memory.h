#pragma once
#include "nes/rom.h"
#include <cstdint>
#include <array>

namespace nes {

class Memory {
public:
    explicit Memory(const ROM* rom);
    uint8_t read(uint16_t addr) const;
    void write(uint16_t addr, uint8_t value);

private:
    std::array<uint8_t, 0x0800> ram_;
    const ROM* rom_;
};

}