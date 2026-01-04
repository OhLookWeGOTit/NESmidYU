#pragma once
#include "nes/memory.h"
#include <cstdint>
#include <string>

namespace nes {

class CPU6502 {
public:
    explicit CPU6502(const Memory* mem);
    void reset();
    int step();
    std::string state() const;

    uint16_t pc() const noexcept;
private:
    const Memory* mem_;
    uint8_t a_;
    uint8_t x_;
    uint8_t y_;
    uint8_t sp_;
    uint16_t pc_;
    uint8_t status_;

    uint8_t read_byte(uint16_t addr) const;
    uint16_t read_word(uint16_t addr) const;
};

}