#include "nes/cpu.h"
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <array>

using namespace nes;

/*
  Implementation notes:
  - Table-driven: each opcode maps to an operation and addressing mode.
  - Addressing modes set addr_abs_ (and/or addr_rel_) and may add an extra cycle.
  - Operations use fetched_ (populated by fetch()) when appropriate.
  - Decimal mode (D flag) is not used on NES (6502 variant), but flags are maintained.
*/

// Forward declaration for table initializer
static const std::array<CPU6502::Op, 256> make_lookup();

inline uint8_t CPU6502::GetFlag(uint8_t f) const { return (status_ & f) > 0 ? 1 : 0; }
inline void CPU6502::SetFlag(uint8_t f, bool v) {
    if (v) status_ |= f; else status_ &= ~f;
}

inline uint8_t CPU6502::read(uint16_t addr) const { return mem_->read(addr); }
inline void CPU6502::write(uint16_t addr, uint8_t value) { const_cast<Memory*>(mem_)->write(addr, value); }

inline void CPU6502::push(uint8_t value) {
    write(0x0100 + sp_, value);
    sp_--;
}

inline uint8_t CPU6502::pop() {
    sp_++;
    return read(0x0100 + sp_);
}

CPU6502::CPU6502(const Memory* mem) : mem_(mem) {
    a_ = x_ = y_ = 0;
    sp_ = 0xFD;
    status_ = 0x00 | U;
    addr_abs_ = addr_rel_ = 0;
    fetched_ = 0;
    opcode_ = 0;
    cycles_ = 0;
}

uint16_t CPU6502::pc() const noexcept { return pc_; }

void CPU6502::reset() {
    a_ = x_ = y_ = 0;
    sp_ = 0xFD;
    status_ = 0x00 | U;
    addr_abs_ = addr_rel_ = 0;
    fetched_ = 0;
    // Reset vector at 0xFFFC
    pc_ = static_cast<uint16_t>(read(0xFFFC) | (read(0xFFFD) << 8));
    cycles_ = 8;
}

// FETCH
uint8_t CPU6502::fetch() {
    if ((lookup_[opcode_].addrmode == &CPU6502::IMP)) {
        fetched_ = a_;
    } else {
        fetched_ = read(addr_abs_);
    }
    return fetched_;
}

// ADDRESSING MODES
int CPU6502::IMP() { fetched_ = a_; return 0; }
int CPU6502::IMM() { addr_abs_ = pc_++; return 0; }
int CPU6502::ZP0() { addr_abs_ = read(pc_++); addr_abs_ &= 0x00FF; return 0; }
int CPU6502::ZPX() { addr_abs_ = (read(pc_++) + x_) & 0x00FF; return 0; }
int CPU6502::ZPY() { addr_abs_ = (read(pc_++) + y_) & 0x00FF; return 0; }
int CPU6502::ABS() {
    uint16_t lo = read(pc_++);
    uint16_t hi = read(pc_++);
    addr_abs_ = (hi << 8) | lo;
    return 0;
}
int CPU6502::ABX() {
    uint16_t lo = read(pc_++);
    uint16_t hi = read(pc_++);
    addr_abs_ = ((hi << 8) | lo) + x_;
    if ((addr_abs_ & 0xFF00) != (hi << 8)) return 1;
    return 0;
}
int CPU6502::ABY() {
    uint16_t lo = read(pc_++);
    uint16_t hi = read(pc_++);
    addr_abs_ = ((hi << 8) | lo) + y_;
    if ((addr_abs_ & 0xFF00) != (hi << 8)) return 1;
    return 0;
}
int CPU6502::IND() {
    uint16_t ptr_lo = read(pc_++);
    uint16_t ptr_hi = read(pc_++);
    uint16_t ptr = (ptr_hi << 8) | ptr_lo;
    // 6502 bug: if low byte is 0xFF, fetch wraps on page
    uint16_t lo = read(ptr);
    uint16_t hi = read((ptr & 0xFF00) | ((ptr + 1) & 0x00FF));
    addr_abs_ = (hi << 8) | lo;
    return 0;
}
int CPU6502::IZX() {
    uint16_t t = read(pc_++);
    uint16_t lo = read((t + x_) & 0x00FF);
    uint16_t hi = read((t + x_ + 1) & 0x00FF);
    addr_abs_ = (hi << 8) | lo;
    return 0;
}
int CPU6502::IZY() {
    uint16_t t = read(pc_++);
    uint16_t lo = read(t & 0x00FF);
    uint16_t hi = read((t + 1) & 0x00FF);
    addr_abs_ = ((hi << 8) | lo) + y_;
    if ((addr_abs_ & 0xFF00) != (hi << 8)) return 1;
    return 0;
}
int CPU6502::REL() {
    addr_rel_ = read(pc_++);
    if (addr_rel_ & 0x80) addr_rel_ |= 0xFF00;
    return 0;
}

// HELPER: compare sets flags based on reg - value
void CPU6502::compare(uint8_t reg, uint8_t value) {
    uint16_t temp = static_cast<uint16_t>(reg) - static_cast<uint16_t>(value);
    SetFlag(C, reg >= value);
    SetFlag(Z, (temp & 0x00FF) == 0);
    SetFlag(N, temp & 0x80);
}

// OPERATIONS
int CPU6502::ADC() {
    fetch();
    uint16_t temp = static_cast<uint16_t>(a_) + fetched_ + GetFlag(C);
    SetFlag(C, temp > 0xFF);
    SetFlag(Z, (temp & 0xFF) == 0);
    SetFlag(V, (~(static_cast<int>(a_) ^ static_cast<int>(fetched_)) & (static_cast<int>(a_) ^ static_cast<int>(temp)) & 0x80));
    SetFlag(N, temp & 0x80);
    a_ = static_cast<uint8_t>(temp & 0xFF);
    return 1;
}

int CPU6502::AND() { fetch(); a_ &= fetched_; SetFlag(Z, a_ == 0); SetFlag(N, a_ & 0x80); return 1; }
int CPU6502::ASL() {
    fetch();
    uint16_t temp = static_cast<uint16_t>(fetched_) << 1;
    SetFlag(C, (temp & 0xFF00) != 0);
    SetFlag(Z, (temp & 0x00FF) == 0);
    SetFlag(N, temp & 0x80);
    if (lookup_[opcode_].addrmode == &CPU6502::IMP) a_ = static_cast<uint8_t>(temp & 0x00FF);
    else write(addr_abs_, static_cast<uint8_t>(temp & 0x00FF));
    return 0;
}

int CPU6502::BCC() { if (GetFlag(C) == 0) { cycles_++; uint16_t prev = pc_; pc_ += addr_rel_; if ((pc_ & 0xFF00) != (prev & 0xFF00)) cycles_++; } return 0; }
int CPU6502::BCS() { if (GetFlag(C) == 1) { cycles_++; uint16_t prev = pc_; pc_ += addr_rel_; if ((pc_ & 0xFF00) != (prev & 0xFF00)) cycles_++; } return 0; }
int CPU6502::BEQ() { if (GetFlag(Z) == 1) { cycles_++; uint16_t prev = pc_; pc_ += addr_rel_; if ((pc_ & 0xFF00) != (prev & 0xFF00)) cycles_++; } return 0; }
int CPU6502::BIT() {
    fetch();
    uint8_t temp = fetched_ & a_;
    SetFlag(Z, (temp) == 0);
    SetFlag(N, fetched_ & 0x80);
    SetFlag(V, fetched_ & 0x40);
    return 0;
}
int CPU6502::BMI() { if (GetFlag(N) == 1) { cycles_++; uint16_t prev = pc_; pc_ += addr_rel_; if ((pc_ & 0xFF00) != (prev & 0xFF00)) cycles_++; } return 0; }
int CPU6502::BNE() { if (GetFlag(Z) == 0) { cycles_++; uint16_t prev = pc_; pc_ += addr_rel_; if ((pc_ & 0xFF00) != (prev & 0xFF00)) cycles_++; } return 0; }
int CPU6502::BPL() { if (GetFlag(N) == 0) { cycles_++; uint16_t prev = pc_; pc_ += addr_rel_; if ((pc_ & 0xFF00) != (prev & 0xFF00)) cycles_++; } return 0; }
int CPU6502::BRK() {
    pc_++;
    SetFlag(I, true);
    push(static_cast<uint8_t>((pc_ >> 8) & 0x00FF));
    push(static_cast<uint8_t>(pc_ & 0x00FF));
    SetFlag(B, true);
    push(status_);
    SetFlag(B, false);
    pc_ = static_cast<uint16_t>(read(0xFFFE) | (read(0xFFFF) << 8));
    return 0;
}
int CPU6502::BVC() { if (GetFlag(V) == 0) { cycles_++; uint16_t prev = pc_; pc_ += addr_rel_; if ((pc_ & 0xFF00) != (prev & 0xFF00)) cycles_++; } return 0; }
int CPU6502::BVS() { if (GetFlag(V) == 1) { cycles_++; uint16_t prev = pc_; pc_ += addr_rel_; if ((pc_ & 0xFF00) != (prev & 0xFF00)) cycles_++; } return 0; }
int CPU6502::CLC() { SetFlag(C, false); return 0; }
int CPU6502::CLD() { SetFlag(D, false); return 0; }
int CPU6502::CLI() { SetFlag(I, false); return 0; }
int CPU6502::CLV() { SetFlag(V, false); return 0; }
int CPU6502::CMP() { fetch(); compare(a_, fetched_); return 1; }
int CPU6502::CPX() { fetch(); compare(x_, fetched_); return 0; }
int CPU6502::CPY() { fetch(); compare(y_, fetched_); return 0; }
int CPU6502::DEC() { fetch(); uint8_t val = static_cast<uint8_t>(fetched_ - 1); write(addr_abs_, val); SetFlag(Z, val == 0); SetFlag(N, val & 0x80); return 0; }
int CPU6502::DEX() { x_ = static_cast<uint8_t>(x_ - 1); SetFlag(Z, x_ == 0); SetFlag(N, x_ & 0x80); return 0; }
int CPU6502::DEY() { y_ = static_cast<uint8_t>(y_ - 1); SetFlag(Z, y_ == 0); SetFlag(N, y_ & 0x80); return 0; }
int CPU6502::EOR() { fetch(); a_ ^= fetched_; SetFlag(Z, a_ == 0); SetFlag(N, a_ & 0x80); return 1; }
int CPU6502::INC() { fetch(); uint8_t val = static_cast<uint8_t>(fetched_ + 1); write(addr_abs_, val); SetFlag(Z, val == 0); SetFlag(N, val & 0x80); return 0; }
int CPU6502::INX() { x_ = static_cast<uint8_t>(x_ + 1); SetFlag(Z, x_ == 0); SetFlag(N, x_ & 0x80); return 0; }
int CPU6502::INY() { y_ = static_cast<uint8_t>(y_ + 1); SetFlag(Z, y_ == 0); SetFlag(N, y_ & 0x80); return 0; }
int CPU6502::JMP() { pc_ = addr_abs_; return 0; }
int CPU6502::JSR() {
    pc_--;
    push(static_cast<uint8_t>((pc_ >> 8) & 0x00FF));
    push(static_cast<uint8_t>(pc_ & 0x00FF));
    pc_ = addr_abs_;
    return 0;
}
int CPU6502::LDA() { fetch(); a_ = fetched_; SetFlag(Z, a_ == 0); SetFlag(N, a_ & 0x80); return 1; }
int CPU6502::LDX() { fetch(); x_ = fetched_; SetFlag(Z, x_ == 0); SetFlag(N, x_ & 0x80); return 1; }
int CPU6502::LDY() { fetch(); y_ = fetched_; SetFlag(Z, y_ == 0); SetFlag(N, y_ & 0x80); return 1; }
int CPU6502::LSR() {
    fetch();
    SetFlag(C, fetched_ & 0x01);
    uint8_t temp = static_cast<uint8_t>(fetched_ >> 1);
    SetFlag(Z, temp == 0);
    SetFlag(N, temp & 0x80);
    if (lookup_[opcode_].addrmode == &CPU6502::IMP) a_ = temp;
    else write(addr_abs_, temp);
    return 0;
}
int CPU6502::NOP() { // many opcodes are effectively NOPs
    // some NOPs have extra cycles/bytes; we use basic NOP behavior here
    return 0;
}
int CPU6502::ORA() { fetch(); a_ |= fetched_; SetFlag(Z, a_ == 0); SetFlag(N, a_ & 0x80); return 1; }
int CPU6502::PHA() { push(a_); return 0; }
int CPU6502::PHP() { push(status_ | B | U); SetFlag(B, false); return 0; }
int CPU6502::PLA() { a_ = pop(); SetFlag(Z, a_ == 0); SetFlag(N, a_ & 0x80); return 0; }
int CPU6502::PLP() { status_ = pop(); SetFlag(U, true); return 0; }
int CPU6502::ROL() {
    fetch();
    uint16_t temp = (static_cast<uint16_t>(fetched_) << 1) | GetFlag(C);
    SetFlag(C, temp & 0xFF00);
    SetFlag(Z, (temp & 0x00FF) == 0);
    SetFlag(N, temp & 0x80);
    if (lookup_[opcode_].addrmode == &CPU6502::IMP) a_ = static_cast<uint8_t>(temp & 0x00FF);
    else write(addr_abs_, static_cast<uint8_t>(temp & 0x00FF));
    return 0;
}
int CPU6502::ROR() {
    fetch();
    uint16_t temp = (GetFlag(C) << 7) | (fetched_ >> 1);
    SetFlag(C, fetched_ & 0x01);
    SetFlag(Z, (temp & 0x00FF) == 0);
    SetFlag(N, temp & 0x80);
    if (lookup_[opcode_].addrmode == &CPU6502::IMP) a_ = static_cast<uint8_t>(temp & 0x00FF);
    else write(addr_abs_, static_cast<uint8_t>(temp & 0x00FF));
    return 0;
}
int CPU6502::RTI() {
    status_ = pop();
    status_ &= ~B;
    status_ |= U;
    uint16_t lo = pop();
    uint16_t hi = pop();
    pc_ = (hi << 8) | lo;
    return 0;
}
int CPU6502::RTS() {
    uint16_t lo = pop();
    uint16_t hi = pop();
    pc_ = ((hi << 8) | lo) + 1;
    return 0;
}
int CPU6502::SBC() {
    fetch();
    uint16_t value = static_cast<uint16_t>(fetched_) ^ 0x00FF;
    uint16_t temp = static_cast<uint16_t>(a_) + value + GetFlag(C);
    SetFlag(C, temp & 0xFF00);
    SetFlag(Z, (temp & 0x00FF) == 0);
    SetFlag(V, ( (temp ^ a_) & (temp ^ value) & 0x80 ));
    SetFlag(N, temp & 0x80);
    a_ = static_cast<uint8_t>(temp & 0x00FF);
    return 1;
}
int CPU6502::SEC() { SetFlag(C, true); return 0; }
int CPU6502::SED() { SetFlag(D, true); return 0; }
int CPU6502::SEI() { SetFlag(I, true); return 0; }
int CPU6502::STA() { write(addr_abs_, a_); return 0; }
int CPU6502::STX() { write(addr_abs_, x_); return 0; }
int CPU6502::STY() { write(addr_abs_, y_); return 0; }
int CPU6502::TAX() { x_ = a_; SetFlag(Z, x_ == 0); SetFlag(N, x_ & 0x80); return 0; }
int CPU6502::TAY() { y_ = a_; SetFlag(Z, y_ == 0); SetFlag(N, y_ & 0x80); return 0; }
int CPU6502::TSX() { x_ = sp_; SetFlag(Z, x_ == 0); SetFlag(N, x_ & 0x80); return 0; }
int CPU6502::TXA() { a_ = x_; SetFlag(Z, a_ == 0); SetFlag(N, a_ & 0x80); return 0; }
int CPU6502::TXS() { sp_ = x_; return 0; }
int CPU6502::TYA() { a_ = y_; SetFlag(Z, a_ == 0); SetFlag(N, a_ & 0x80); return 0; }

// Build the opcode lookup table.
// For brevity and clarity we initialize the table with the standard official opcodes.
// Unofficial opcodes map to NOP here.
const std::array<CPU6502::Op, 256> CPU6502::lookup_ = make_lookup();

static const std::array<CPU6502::Op, 256> make_lookup() {
    using Op = CPU6502::Op;
    std::array<Op, 256> table;
    // Fill with default NOP
    for (int i = 0; i < 256; ++i) {
        table[i] = { "NOP", &CPU6502::NOP, &CPU6502::IMP, 2 };
    }
    // Official opcodes (mnemonic, operation, addressing mode, cycles)
    table[0x69] = { "ADC", &CPU6502::ADC, &CPU6502::IMM, 2 };
    table[0x65] = { "ADC", &CPU6502::ADC, &CPU6502::ZP0, 3 };
    table[0x75] = { "ADC", &CPU6502::ADC, &CPU6502::ZPX, 4 };
    table[0x6D] = { "ADC", &CPU6502::ADC, &CPU6502::ABS, 4 };
    table[0x7D] = { "ADC", &CPU6502::ADC, &CPU6502::ABX, 4 };
    table[0x79] = { "ADC", &CPU6502::ADC, &CPU6502::ABY, 4 };
    table[0x61] = { "ADC", &CPU6502::ADC, &CPU6502::IZX, 6 };
    table[0x71] = { "ADC", &CPU6502::ADC, &CPU6502::IZY, 5 };

    table[0x29] = { "AND", &CPU6502::AND, &CPU6502::IMM, 2 };
    table[0x25] = { "AND", &CPU6502::AND, &CPU6502::ZP0, 3 };
    table[0x35] = { "AND", &CPU6502::AND, &CPU6502::ZPX, 4 };
    table[0x2D] = { "AND", &CPU6502::AND, &CPU6502::ABS, 4 };
    table[0x3D] = { "AND", &CPU6502::AND, &CPU6502::ABX, 4 };
    table[0x39] = { "AND", &CPU6502::AND, &CPU6502::ABY, 4 };
    table[0x21] = { "AND", &CPU6502::AND, &CPU6502::IZX, 6 };
    table[0x31] = { "AND", &CPU6502::AND, &CPU6502::IZY, 5 };

    table[0x0A] = { "ASL", &CPU6502::ASL, &CPU6502::IMP, 2 };
    table[0x06] = { "ASL", &CPU6502::ASL, &CPU6502::ZP0, 5 };
    table[0x16] = { "ASL", &CPU6502::ASL, &CPU6502::ZPX, 6 };
    table[0x0E] = { "ASL", &CPU6502::ASL, &CPU6502::ABS, 6 };
    table[0x1E] = { "ASL", &CPU6502::ASL, &CPU6502::ABX, 7 };

    table[0x90] = { "BCC", &CPU6502::BCC, &CPU6502::REL, 2 };
    table[0xB0] = { "BCS", &CPU6502::BCS, &CPU6502::REL, 2 };
    table[0xF0] = { "BEQ", &CPU6502::BEQ, &CPU6502::REL, 2 };

    table[0x24] = { "BIT", &CPU6502::BIT, &CPU6502::ZP0, 3 };
    table[0x2C] = { "BIT", &CPU6502::BIT, &CPU6502::ABS, 4 };

    table[0x30] = { "BMI", &CPU6502::BMI, &CPU6502::REL, 2 };
    table[0xD0] = { "BNE", &CPU6502::BNE, &CPU6502::REL, 2 };
    table[0x10] = { "BPL", &CPU6502::BPL, &CPU6502::REL, 2 };

    table[0x00] = { "BRK", &CPU6502::BRK, &CPU6502::IMP, 7 };
    table[0x50] = { "BVC", &CPU6502::BVC, &CPU6502::REL, 2 };
    table[0x70] = { "BVS", &CPU6502::BVS, &CPU6502::REL, 2 };

    table[0x18] = { "CLC", &CPU6502::CLC, &CPU6502::IMP, 2 };
    table[0xD8] = { "CLD", &CPU6502::CLD, &CPU6502::IMP, 2 };
    table[0x58] = { "CLI", &CPU6502::CLI, &CPU6502::IMP, 2 };
    table[0xB8] = { "CLV", &CPU6502::CLV, &CPU6502::IMP, 2 };

    table[0xC9] = { "CMP", &CPU6502::CMP, &CPU6502::IMM, 2 };
    table[0xC5] = { "CMP", &CPU6502::CMP, &CPU6502::ZP0, 3 };
    table[0xD5] = { "CMP", &CPU6502::CMP, &CPU6502::ZPX, 4 };
    table[0xCD] = { "CMP", &CPU6502::CMP, &CPU6502::ABS, 4 };
    table[0xDD] = { "CMP", &CPU6502::CMP, &CPU6502::ABX, 4 };
    table[0xD9] = { "CMP", &CPU6502::CMP, &CPU6502::ABY, 4 };
    table[0xC1] = { "CMP", &CPU6502::CMP, &CPU6502::IZX, 6 };
    table[0xD1] = { "CMP", &CPU6502::CMP, &CPU6502::IZY, 5 };

    table[0xE0] = { "CPX", &CPU6502::CPX, &CPU6502::IMM, 2 };
    table[0xE4] = { "CPX", &CPU6502::CPX, &CPU6502::ZP0, 3 };
    table[0xEC] = { "CPX", &CPU6502::CPX, &CPU6502::ABS, 4 };

    table[0xC0] = { "CPY", &CPU6502::CPY, &CPU6502::IMM, 2 };
    table[0xC4] = { "CPY", &CPU6502::CPY, &CPU6502::ZP0, 3 };
    table[0xCC] = { "CPY", &CPU6502::CPY, &CPU6502::ABS, 4 };

    table[0xC6] = { "DEC", &CPU6502::DEC, &CPU6502::ZP0, 5 };
    table[0xD6] = { "DEC", &CPU6502::DEC, &CPU6502::ZPX, 6 };
    table[0xCE] = { "DEC", &CPU6502::DEC, &CPU6502::ABS, 6 };
    table[0xDE] = { "DEC", &CPU6502::DEC, &CPU6502::ABX, 7 };

    table[0xCA] = { "DEX", &CPU6502::DEX, &CPU6502::IMP, 2 };
    table[0x88] = { "DEY", &CPU6502::DEY, &CPU6502::IMP, 2 };

    table[0x49] = { "EOR", &CPU6502::EOR, &CPU6502::IMM, 2 };
    table[0x45] = { "EOR", &CPU6502::EOR, &CPU6502::ZP0, 3 };
    table[0x55] = { "EOR", &CPU6502::EOR, &CPU6502::ZPX, 4 };
    table[0x4D] = { "EOR", &CPU6502::EOR, &CPU6502::ABS, 4 };
    table[0x5D] = { "EOR", &CPU6502::EOR, &CPU6502::ABX, 4 };
    table[0x59] = { "EOR", &CPU6502::EOR, &CPU6502::ABY, 4 };
    table[0x41] = { "EOR", &CPU6502::EOR, &CPU6502::IZX, 6 };
    table[0x51] = { "EOR", &CPU6502::EOR, &CPU6502::IZY, 5 };

    table[0xE6] = { "INC", &CPU6502::INC, &CPU6502::ZP0, 5 };
    table[0xF6] = { "INC", &CPU6502::INC, &CPU6502::ZPX, 6 };
    table[0xEE] = { "INC", &CPU6502::INC, &CPU6502::ABS, 6 };
    table[0xFE] = { "INC", &CPU6502::INC, &CPU6502::ABX, 7 };

    table[0xE8] = { "INX", &CPU6502::INX, &CPU6502::IMP, 2 };
    table[0xC8] = { "INY", &CPU6502::INY, &CPU6502::IMP, 2 };

    table[0x4C] = { "JMP", &CPU6502::JMP, &CPU6502::ABS, 3 };
    table[0x6C] = { "JMP", &CPU6502::JMP, &CPU6502::IND, 5 };
    table[0x20] = { "JSR", &CPU6502::JSR, &CPU6502::ABS, 6 };

    table[0xA9] = { "LDA", &CPU6502::LDA, &CPU6502::IMM, 2 };
    table[0xA5] = { "LDA", &CPU6502::LDA, &CPU6502::ZP0, 3 };
    table[0xB5] = { "LDA", &CPU6502::LDA, &CPU6502::ZPX, 4 };
    table[0xAD] = { "LDA", &CPU6502::LDA, &CPU6502::ABS, 4 };
    table[0xBD] = { "LDA", &CPU6502::LDA, &CPU6502::ABX, 4 };
    table[0xB9] = { "LDA", &CPU6502::LDA, &CPU6502::ABY, 4 };
    table[0xA1] = { "LDA", &CPU6502::LDA, &CPU6502::IZX, 6 };
    table[0xB1] = { "LDA", &CPU6502::LDA, &CPU6502::IZY, 5 };

    table[0xA2] = { "LDX", &CPU6502::LDX, &CPU6502::IMM, 2 };
    table[0xA6] = { "LDX", &CPU6502::LDX, &CPU6502::ZP0, 3 };
    table[0xB6] = { "LDX", &CPU6502::LDX, &CPU6502::ZPY, 4 };
    table[0xAE] = { "LDX", &CPU6502::LDX, &CPU6502::ABS, 4 };
    table[0xBE] = { "LDX", &CPU6502::LDX, &CPU6502::ABY, 4 };

    table[0xA0] = { "LDY", &CPU6502::LDY, &CPU6502::IMM, 2 };
    table[0xA4] = { "LDY", &CPU6502::LDY, &CPU6502::ZP0, 3 };
    table[0xB4] = { "LDY", &CPU6502::LDY, &CPU6502::ZPX, 4 };
    table[0xAC] = { "LDY", &CPU6502::LDY, &CPU6502::ABS, 4 };
    table[0xBC] = { "LDY", &CPU6502::LDY, &CPU6502::ABX, 4 };

    table[0x4A] = { "LSR", &CPU6502::LSR, &CPU6502::IMP, 2 };
    table[0x46] = { "LSR", &CPU6502::LSR, &CPU6502::ZP0, 5 };
    table[0x56] = { "LSR", &CPU6502::LSR, &CPU6502::ZPX, 6 };
    table[0x4E] = { "LSR", &CPU6502::LSR, &CPU6502::ABS, 6 };
    table[0x5E] = { "LSR", &CPU6502::LSR, &CPU6502::ABX, 7 };

    table[0xEA] = { "NOP", &CPU6502::NOP, &CPU6502::IMP, 2 };

    table[0x09] = { "ORA", &CPU6502::ORA, &CPU6502::IMM, 2 };
    table[0x05] = { "ORA", &CPU6502::ORA, &CPU6502::ZP0, 3 };
    table[0x15] = { "ORA", &CPU6502::ORA, &CPU6502::ZPX, 4 };
    table[0x0D] = { "ORA", &CPU6502::ORA, &CPU6502::ABS, 4 };
    table[0x1D] = { "ORA", &CPU6502::ORA, &CPU6502::ABX, 4 };
    table[0x19] = { "ORA", &CPU6502::ORA, &CPU6502::ABY, 4 };
    table[0x01] = { "ORA", &CPU6502::ORA, &CPU6502::IZX, 6 };
    table[0x11] = { "ORA", &CPU6502::ORA, &CPU6502::IZY, 5 };

    table[0x48] = { "PHA", &CPU6502::PHA, &CPU6502::IMP, 3 };
    table[0x08] = { "PHP", &CPU6502::PHP, &CPU6502::IMP, 3 };
    table[0x68] = { "PLA", &CPU6502::PLA, &CPU6502::IMP, 4 };
    table[0x28] = { "PLP", &CPU6502::PLP, &CPU6502::IMP, 4 };

    table[0x2A] = { "ROL", &CPU6502::ROL, &CPU6502::IMP, 2 };
    table[0x26] = { "ROL", &CPU6502::ROL, &CPU6502::ZP0, 5 };
    table[0x36] = { "ROL", &CPU6502::ROL, &CPU6502::ZPX, 6 };
    table[0x2E] = { "ROL", &CPU6502::ROL, &CPU6502::ABS, 6 };
    table[0x3E] = { "ROL", &CPU6502::ROL, &CPU6502::ABX, 7 };

    table[0x6A] = { "ROR", &CPU6502::ROR, &CPU6502::IMP, 2 };
    table[0x66] = { "ROR", &CPU6502::ROR, &CPU6502::ZP0, 5 };
    table[0x76] = { "ROR", &CPU6502::ROR, &CPU6502::ZPX, 6 };
    table[0x6E] = { "ROR", &CPU6502::ROR, &CPU6502::ABS, 6 };
    table[0x7E] = { "ROR", &CPU6502::ROR, &CPU6502::ABX, 7 };

    table[0x40] = { "RTI", &CPU6502::RTI, &CPU6502::IMP, 6 };
    table[0x60] = { "RTS", &CPU6502::RTS, &CPU6502::IMP, 6 };

    table[0xE9] = { "SBC", &CPU6502::SBC, &CPU6502::IMM, 2 };
    table[0xE5] = { "SBC", &CPU6502::SBC, &CPU6502::ZP0, 3 };
    table[0xF5] = { "SBC", &CPU6502::SBC, &CPU6502::ZPX, 4 };
    table[0xED] = { "SBC", &CPU6502::SBC, &CPU6502::ABS, 4 };
    table[0xFD] = { "SBC", &CPU6502::SBC, &CPU6502::ABX, 4 };
    table[0xF9] = { "SBC", &CPU6502::SBC, &CPU6502::ABY, 4 };
    table[0xE1] = { "SBC", &CPU6502::SBC, &CPU6502::IZX, 6 };
    table[0xF1] = { "SBC", &CPU6502::SBC, &CPU6502::IZY, 5 };

    table[0x38] = { "SEC", &CPU6502::SEC, &CPU6502::IMP, 2 };
    table[0xF8] = { "SED", &CPU6502::SED, &CPU6502::IMP, 2 };
    table[0x78] = { "SEI", &CPU6502::SEI, &CPU6502::IMP, 2 };

    table[0x85] = { "STA", &CPU6502::STA, &CPU6502::ZP0, 3 };
    table[0x95] = { "STA", &CPU6502::STA, &CPU6502::ZPX, 4 };
    table[0x8D] = { "STA", &CPU6502::STA, &CPU6502::ABS, 4 };
    table[0x9D] = { "STA", &CPU6502::STA, &CPU6502::ABX, 5 };
    table[0x99] = { "STA", &CPU6502::STA, &CPU6502::ABY, 5 };
    table[0x81] = { "STA", &CPU6502::STA, &CPU6502::IZX, 6 };
    table[0x91] = { "STA", &CPU6502::STA, &CPU6502::IZY, 6 };

    table[0x86] = { "STX", &CPU6502::STX, &CPU6502::ZP0, 3 };
    table[0x96] = { "STX", &CPU6502::STX, &CPU6502::ZPY, 4 };
    table[0x8E] = { "STX", &CPU6502::STX, &CPU6502::ABS, 4 };

    table[0x84] = { "STY", &CPU6502::STY, &CPU6502::ZP0, 3 };
    table[0x94] = { "STY", &CPU6502::STY, &CPU6502::ZPX, 4 };
    table[0x8C] = { "STY", &CPU6502::STY, &CPU6502::ABS, 4 };

    table[0xAA] = { "TAX", &CPU6502::TAX, &CPU6502::IMP, 2 };
    table[0xA8] = { "TAY", &CPU6502::TAY, &CPU6502::IMP, 2 };
    table[0xBA] = { "TSX", &CPU6502::TSX, &CPU6502::IMP, 2 };
    table[0x8A] = { "TXA", &CPU6502::TXA, &CPU6502::IMP, 2 };
    table[0x9A] = { "TXS", &CPU6502::TXS, &CPU6502::IMP, 2 };
    table[0x98] = { "TYA", &CPU6502::TYA, &CPU6502::IMP, 2 };

    // Many other official opcodes map above; this table covers official instructions.
    return table;
}

// STEP: execute one instruction
int CPU6502::step() {
    if (cycles_ > 0) {
        cycles_--;
        return cycles_;
    }

    opcode_ = read(pc_);
    pc_++;
    const Op& op = lookup_[opcode_];
    cycles_ = op.cycles;
    int additional_cycle1 = (this->*op.addrmode)();
    int additional_cycle2 = (this->*op.operate)();
    cycles_ += (additional_cycle1 & additional_cycle2);
    return cycles_;
}

std::string CPU6502::state() const {
    std::ostringstream o;
    o << "PC=" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << pc_
      << " A=" << std::setw(2) << static_cast<int>(a_)
      << " X=" << std::setw(2) << static_cast<int>(x_)
      << " Y=" << std::setw(2) << static_cast<int>(y_)
      << " SP=" << std::setw(2) << static_cast<int>(sp_)
      << " P=" << std::setw(2) << static_cast<int>(status_);
    return o.str();
}