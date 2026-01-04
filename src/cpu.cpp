#include "nes/cpu.h"
#include <sstream>
#include <iomanip>

using namespace nes;

CPU6502::CPU6502(const Memory* mem) : mem_(mem) { reset(); }

uint8_t CPU6502::read_byte(uint16_t addr) const { return mem_->read(addr); }

uint16_t CPU6502::read_word(uint16_t addr) const {
    uint8_t lo = read_byte(addr);
    uint8_t hi = read_byte(static_cast<uint16_t>(addr + 1));
    return static_cast<uint16_t>((hi << 8) | lo);
}

void CPU6502::reset() {
    a_ = x_ = y_ = 0;
    sp_ = 0xFD;
    status_ = 0x34;
    pc_ = read_word(0xFFFC);
}

int CPU6502::step() {
    uint8_t opcode = read_byte(pc_);
    pc_ = static_cast<uint16_t>(pc_ + 1);
    if (opcode == 0xEA) {
        return 2;
    } else if (opcode == 0x00) {
        throw std::runtime_error("BRK encountered");
    } else {
        return 2;
    }
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

uint16_t CPU6502::pc() const noexcept { return pc_; }