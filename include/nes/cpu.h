#pragma once
#include "nes/memory.h"
#include <cstdint>
#include <string>
#include <array>

namespace nes {

class CPU6502 {
public:
    explicit CPU6502(const Memory* mem);
    void reset();
    int step();
    std::string state() const;
    uint16_t pc() const noexcept;

private:
    // CPU registers
    const Memory* mem_;
    uint8_t a_;
    uint8_t x_;
    uint8_t y_;
    uint8_t sp_;
    uint16_t pc_;
    uint8_t status_;

    // internal helper fields
    uint16_t addr_abs_;
    uint16_t addr_rel_;
    uint8_t fetched_;
    uint8_t opcode_;
    int cycles_;

    // flag accessors
    enum FLAGS6502 {
        C = (1 << 0), // Carry
        Z = (1 << 1), // Zero
        I = (1 << 2), // Disable Interrupts
        D = (1 << 3), // Decimal (unused on NES)
        B = (1 << 4), // Break
        U = (1 << 5), // Unused
        V = (1 << 6), // Overflow
        N = (1 << 7), // Negative
    };

    inline uint8_t GetFlag(uint8_t f) const;
    inline void SetFlag(uint8_t f, bool v);

    // memory helpers
    inline uint8_t read(uint16_t addr) const;
    inline void write(uint16_t addr, uint8_t value);

    // stack helpers
    inline void push(uint8_t value);
    inline uint8_t pop();

    // addressing modes
    int IMP(); int IMM(); int ZP0(); int ZPX(); int ZPY();
    int REL(); int ABS(); int ABX(); int ABY(); int IND(); int IZX(); int IZY();

    // fetch operand
    uint8_t fetch();

    // operations
    int ADC(); int AND(); int ASL(); int BCC(); int BCS(); int BEQ(); int BIT();
    int BMI(); int BNE(); int BPL(); int BRK(); int BVC(); int BVS();
    int CLC(); int CLD(); int CLI(); int CLV(); int CMP(); int CPX(); int CPY();
    int DEC(); int DEX(); int DEY(); int EOR(); int INC(); int INX(); int INY();
    int JMP(); int JSR(); int LDA(); int LDX(); int LDY(); int LSR(); int NOP();
    int ORA(); int PHA(); int PHP(); int PLA(); int PLP(); int ROL(); int ROR();
    int RTI(); int RTS(); int SBC(); int SEC(); int SED(); int SEI();
    int STA(); int STX(); int STY(); int TAX(); int TAY(); int TSX(); int TXA(); int TXS(); int TYA();

    // helper for compare
    void compare(uint8_t reg, uint8_t value);

    // opcode lookup table
    struct Op {
        const char* name;
        int (CPU6502::*operate)();
        int (CPU6502::*addrmode)();
        uint8_t cycles;
    };
    static const std::array<Op, 256> lookup_;
};

}