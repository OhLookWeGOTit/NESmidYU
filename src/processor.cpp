#include "nes/processor.h"
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <array>

using namespace nes;

static const std::array<Processor6502::Instruction, 256> create_instruction_table();

inline uint8_t Processor6502::read_memory(uint16_t addr) const { return memory_->fetch(addr); }
inline void Processor6502::write_memory(uint16_t addr, uint8_t val) { const_cast<MemoryMap*>(memory_)->store(addr, val); }
inline void Processor6502::push_stack(uint8_t val) { write_memory(0x0100 + stack_pointer_, val); stack_pointer_--; }
inline uint8_t Processor6502::pull_stack() { stack_pointer_++; return read_memory(0x0100 + stack_pointer_); }

Processor6502::Processor6502(const MemoryMap* memory) : memory_(memory) {
    accumulator_ = index_x_ = index_y_ = 0;
    stack_pointer_ = 0xFD;
    status_flags_ = 0x00 | Unused;
    absolute_address_ = relative_offset_ = 0;
    operand_value_ = 0;
    current_instruction_ = 0;
    cycle_count_ = 0;
}

uint16_t Processor6502::get_program_counter() const noexcept { return program_counter_; }

void Processor6502::initialize() {
    accumulator_ = index_x_ = index_y_ = 0;
    stack_pointer_ = 0xFD;
    status_flags_ = 0x00 | Unused;
    absolute_address_ = relative_offset_ = 0;
    operand_value_ = 0;
    // Reset vector at 0xFFFC
    program_counter_ = static_cast<uint16_t>(read_memory(0xFFFC) | (read_memory(0xFFFD) << 8));
    cycle_count_ = 8;
}

uint8_t Processor6502::load_operand() {
    if ((instruction_table_[current_instruction_].addressing == &Processor6502::implied_mode)) {
        operand_value_ = accumulator_;
    } else {
        operand_value_ = read_memory(absolute_address_);
    }
    return operand_value_;
}

// ADDRESSING MODES
int Processor6502::implied_mode() { operand_value_ = accumulator_; return 0; }
int Processor6502::immediate_mode() { absolute_address_ = program_counter_++; return 0; }
int Processor6502::zero_page_mode() { absolute_address_ = read_memory(program_counter_++); absolute_address_ &= 0x00FF; return 0; }
int Processor6502::zero_page_x_mode() { absolute_address_ = (read_memory(program_counter_++) + index_x_) & 0x00FF; return 0; }
int Processor6502::zero_page_y_mode() { absolute_address_ = (read_memory(program_counter_++) + index_y_) & 0x00FF; return 0; }
int Processor6502::absolute_mode() {
    uint16_t low = read_memory(program_counter_++);
    uint16_t high = read_memory(program_counter_++);
    absolute_address_ = (high << 8) | low;
    return 0;
}
int Processor6502::absolute_x_mode() {
    uint16_t low = read_memory(program_counter_++);
    uint16_t high = read_memory(program_counter_++);
    absolute_address_ = ((high << 8) | low) + index_x_;
    if ((absolute_address_ & 0xFF00) != (high << 8)) return 1;
    return 0;
}
int Processor6502::absolute_y_mode() {
    uint16_t low = read_memory(program_counter_++);
    uint16_t high = read_memory(program_counter_++);
    absolute_address_ = ((high << 8) | low) + index_y_;
    if ((absolute_address_ & 0xFF00) != (high << 8)) return 1;
    return 0;
}
int Processor6502::indirect_mode() {
    uint16_t ptr_low = read_memory(program_counter_++);
    uint16_t ptr_high = read_memory(program_counter_++);
    uint16_t ptr = (ptr_high << 8) | ptr_low;
    // 6502 bug: if low byte is 0xFF, fetch wraps on page
    uint16_t low = read_memory(ptr);
    uint16_t high = read_memory((ptr & 0xFF00) | ((ptr + 1) & 0x00FF));
    absolute_address_ = (high << 8) | low;
    return 0;
}
int Processor6502::indexed_indirect_mode() {
    uint16_t temp = read_memory(program_counter_++);
    uint16_t low = read_memory((temp + index_x_) & 0x00FF);
    uint16_t high = read_memory((temp + index_x_ + 1) & 0x00FF);
    absolute_address_ = (high << 8) | low;
    return 0;
}
int Processor6502::indirect_indexed_mode() {
    uint16_t temp = read_memory(program_counter_++);
    uint16_t low = read_memory(temp & 0x00FF);
    uint16_t high = read_memory((temp + 1) & 0x00FF);
    absolute_address_ = ((high << 8) | low) + index_y_;
    if ((absolute_address_ & 0xFF00) != (high << 8)) return 1;
    return 0;
}
int Processor6502::relative_mode() {
    relative_offset_ = read_memory(program_counter_++);
    if (relative_offset_ & 0x80) relative_offset_ |= 0xFF00;
    return 0;
}

// HELPER: compare sets flags based on reg - value
void Processor6502::perform_comparison(uint8_t reg, uint8_t val) {
    uint16_t result = static_cast<uint16_t>(reg) - static_cast<uint16_t>(val);
    set_flag(Carry, reg >= val);
    set_flag(Zero, (result & 0x00FF) == 0);
    set_flag(Negative, result & 0x80);
}

// OPERATIONS
int Processor6502::add_with_carry() {
    load_operand();
    uint16_t sum = static_cast<uint16_t>(accumulator_) + operand_value_ + check_flag(Carry);
    set_flag(Carry, sum > 0xFF);
    set_flag(Zero, (sum & 0xFF) == 0);
    set_flag(Overflow, (~(static_cast<int>(accumulator_) ^ static_cast<int>(operand_value_)) & (static_cast<int>(accumulator_) ^ static_cast<int>(sum)) & 0x80));
    set_flag(Negative, sum & 0x80);
    accumulator_ = static_cast<uint8_t>(sum & 0xFF);
    return 1;
}

int Processor6502::logical_and() { load_operand(); accumulator_ &= operand_value_; set_flag(Zero, accumulator_ == 0); set_flag(Negative, accumulator_ & 0x80); return 1; }
int Processor6502::arithmetic_shift_left() {
    load_operand();
    uint16_t shifted = static_cast<uint16_t>(operand_value_) << 1;
    set_flag(Carry, (shifted & 0xFF00) != 0);
    set_flag(Zero, (shifted & 0x00FF) == 0);
    set_flag(Negative, shifted & 0x80);
    if (instruction_table_[current_instruction_].addressing == &Processor6502::implied_mode) accumulator_ = static_cast<uint8_t>(shifted & 0x00FF);
    else write_memory(absolute_address_, static_cast<uint8_t>(shifted & 0x00FF));
    return 0;
}

int Processor6502::branch_carry_clear() { if (check_flag(Carry) == 0) { cycle_count_++; uint16_t prev = program_counter_; program_counter_ += relative_offset_; if ((program_counter_ & 0xFF00) != (prev & 0xFF00)) cycle_count_++; } return 0; }
int Processor6502::branch_carry_set() { if (check_flag(Carry) == 1) { cycle_count_++; uint16_t prev = program_counter_; program_counter_ += relative_offset_; if ((program_counter_ & 0xFF00) != (prev & 0xFF00)) cycle_count_++; } return 0; }
int Processor6502::branch_equal() { if (check_flag(Zero) == 1) { cycle_count_++; uint16_t prev = program_counter_; program_counter_ += relative_offset_; if ((program_counter_ & 0xFF00) != (prev & 0xFF00)) cycle_count_++; } return 0; }
int Processor6502::test_bits() {
    load_operand();
    uint8_t result = operand_value_ & accumulator_;
    set_flag(Zero, (result) == 0);
    set_flag(Negative, operand_value_ & 0x80);
    set_flag(Overflow, operand_value_ & 0x40);
    return 0;
}
int Processor6502::branch_minus() { if (check_flag(Negative) == 1) { cycle_count_++; uint16_t prev = program_counter_; program_counter_ += relative_offset_; if ((program_counter_ & 0xFF00) != (prev & 0xFF00)) cycle_count_++; } return 0; }
int Processor6502::branch_not_equal() { if (check_flag(Zero) == 0) { cycle_count_++; uint16_t prev = program_counter_; program_counter_ += relative_offset_; if ((program_counter_ & 0xFF00) != (prev & 0xFF00)) cycle_count_++; } return 0; }
int Processor6502::branch_plus() { if (check_flag(Negative) == 0) { cycle_count_++; uint16_t prev = program_counter_; program_counter_ += relative_offset_; if ((program_counter_ & 0xFF00) != (prev & 0xFF00)) cycle_count_++; } return 0; }
int Processor6502::software_interrupt() {
    program_counter_++;
    set_flag(InterruptDisable, true);
    push_stack(static_cast<uint8_t>((program_counter_ >> 8) & 0x00FF));
    push_stack(static_cast<uint8_t>(program_counter_ & 0x00FF));
    set_flag(Break, true);
    push_stack(status_flags_);
    set_flag(Break, false);
    program_counter_ = static_cast<uint16_t>(read_memory(0xFFFE) | (read_memory(0xFFFF) << 8));
    return 0;
}
int Processor6502::branch_overflow_clear() { if (check_flag(Overflow) == 0) { cycle_count_++; uint16_t prev = program_counter_; program_counter_ += relative_offset_; if ((program_counter_ & 0xFF00) != (prev & 0xFF00)) cycle_count_++; } return 0; }
int Processor6502::branch_overflow_set() { if (check_flag(Overflow) == 1) { cycle_count_++; uint16_t prev = program_counter_; program_counter_ += relative_offset_; if ((program_counter_ & 0xFF00) != (prev & 0xFF00)) cycle_count_++; } return 0; }
int Processor6502::clear_carry_flag() { set_flag(Carry, false); return 0; }
int Processor6502::clear_decimal_flag() { set_flag(Decimal, false); return 0; }
int Processor6502::clear_interrupt_flag() { set_flag(InterruptDisable, false); return 0; }
int Processor6502::clear_overflow_flag() { set_flag(Overflow, false); return 0; }
int Processor6502::compare_with_accumulator() { load_operand(); perform_comparison(accumulator_, operand_value_); return 1; }
int Processor6502::compare_with_x_register() { load_operand(); perform_comparison(index_x_, operand_value_); return 0; }
int Processor6502::compare_with_y_register() { load_operand(); perform_comparison(index_y_, operand_value_); return 0; }
int Processor6502::decrement_memory() { load_operand(); uint8_t val = static_cast<uint8_t>(operand_value_ - 1); write_memory(absolute_address_, val); set_flag(Zero, val == 0); set_flag(Negative, val & 0x80); return 0; }
int Processor6502::decrement_x_register() { index_x_ = static_cast<uint8_t>(index_x_ - 1); set_flag(Zero, index_x_ == 0); set_flag(Negative, index_x_ & 0x80); return 0; }
int Processor6502::decrement_y_register() { index_y_ = static_cast<uint8_t>(index_y_ - 1); set_flag(Zero, index_y_ == 0); set_flag(Negative, index_y_ & 0x80); return 0; }
int Processor6502::exclusive_or() { load_operand(); accumulator_ ^= operand_value_; set_flag(Zero, accumulator_ == 0); set_flag(Negative, accumulator_ & 0x80); return 1; }
int Processor6502::increment_memory() { load_operand(); uint8_t val = static_cast<uint8_t>(operand_value_ + 1); write_memory(absolute_address_, val); set_flag(Zero, val == 0); set_flag(Negative, val & 0x80); return 0; }
int Processor6502::increment_x_register() { index_x_ = static_cast<uint8_t>(index_x_ + 1); set_flag(Zero, index_x_ == 0); set_flag(Negative, index_x_ & 0x80); return 0; }
int Processor6502::increment_y_register() { index_y_ = static_cast<uint8_t>(index_y_ + 1); set_flag(Zero, index_y_ == 0); set_flag(Negative, index_y_ & 0x80); return 0; }
int Processor6502::jump_to_subroutine() {
    program_counter_--;
    push_stack(static_cast<uint8_t>((program_counter_ >> 8) & 0x00FF));
    push_stack(static_cast<uint8_t>(program_counter_ & 0x00FF));
    program_counter_ = absolute_address_;
    return 0;
}
int Processor6502::load_accumulator() { load_operand(); accumulator_ = operand_value_; set_flag(Zero, accumulator_ == 0); set_flag(Negative, accumulator_ & 0x80); return 1; }
int Processor6502::load_x_register() { load_operand(); index_x_ = operand_value_; set_flag(Zero, index_x_ == 0); set_flag(Negative, index_x_ & 0x80); return 1; }
int Processor6502::load_y_register() { load_operand(); index_y_ = operand_value_; set_flag(Zero, index_y_ == 0); set_flag(Negative, index_y_ & 0x80); return 1; }
int Processor6502::logical_shift_right() {
    load_operand();
    set_flag(Carry, operand_value_ & 0x01);
    uint8_t temp = static_cast<uint8_t>(operand_value_ >> 1);
    set_flag(Zero, temp == 0);
    set_flag(Negative, temp & 0x80);
    if (instruction_table_[current_instruction_].addressing == &Processor6502::implied_mode) accumulator_ = temp;
    else write_memory(absolute_address_, temp);
    return 0;
}
int Processor6502::no_operation() { // many opcodes are effectively NOPs
    // some NOPs have extra cycles/bytes; we use basic NOP behavior here
    return 0;
}
int Processor6502::or_with_accumulator() { load_operand(); accumulator_ |= operand_value_; set_flag(Zero, accumulator_ == 0); set_flag(Negative, accumulator_ & 0x80); return 1; }
int Processor6502::push_accumulator() { push_stack(accumulator_); return 0; }
int Processor6502::push_processor_status() { push_stack(status_flags_ | Break | Unused); set_flag(Break, false); return 0; }
int Processor6502::pull_accumulator() { accumulator_ = pull_stack(); set_flag(Zero, accumulator_ == 0); set_flag(Negative, accumulator_ & 0x80); return 0; }
int Processor6502::pull_processor_status() { status_flags_ = pull_stack(); set_flag(Unused, true); return 0; }
int Processor6502::rotate_left() {
    load_operand();
    uint16_t rotated = (static_cast<uint16_t>(operand_value_) << 1) | check_flag(Carry);
    set_flag(Carry, rotated & 0xFF00);
    set_flag(Zero, (rotated & 0x00FF) == 0);
    set_flag(Negative, rotated & 0x80);
    if (instruction_table_[current_instruction_].addressing == &Processor6502::implied_mode) accumulator_ = static_cast<uint8_t>(rotated & 0x00FF);
    else write_memory(absolute_address_, static_cast<uint8_t>(rotated & 0x00FF));
    return 0;
}
int Processor6502::rotate_right() {
    load_operand();
    uint16_t rotated = (check_flag(Carry) << 7) | (operand_value_ >> 1);
    set_flag(Carry, operand_value_ & 0x01);
    set_flag(Zero, (rotated & 0x00FF) == 0);
    set_flag(Negative, rotated & 0x80);
    if (instruction_table_[current_instruction_].addressing == &Processor6502::implied_mode) accumulator_ = static_cast<uint8_t>(rotated & 0x00FF);
    else write_memory(absolute_address_, static_cast<uint8_t>(rotated & 0x00FF));
    return 0;
}
int Processor6502::return_from_interrupt() {
    status_flags_ = pull_stack();
    status_flags_ &= ~Break;
    status_flags_ |= Unused;
    uint16_t low = pull_stack();
    uint16_t high = pull_stack();
    program_counter_ = (high << 8) | low;
    return 0;
}
int Processor6502::return_from_subroutine() {
    uint16_t low = pull_stack();
    uint16_t high = pull_stack();
    program_counter_ = ((high << 8) | low) + 1;
    return 0;
}
int Processor6502::subtract_with_carry() {
    load_operand();
    uint16_t value = static_cast<uint16_t>(operand_value_) ^ 0x00FF;
    uint16_t temp = static_cast<uint16_t>(accumulator_) + value + check_flag(Carry);
    set_flag(Carry, temp & 0xFF00);
    set_flag(Zero, (temp & 0x00FF) == 0);
    set_flag(Overflow, ( (temp ^ accumulator_) & (temp ^ value) & 0x80 ));
    set_flag(Negative, temp & 0x80);
    accumulator_ = static_cast<uint8_t>(temp & 0x00FF);
    return 1;
}
int Processor6502::set_carry_flag() { set_flag(Carry, true); return 0; }
int Processor6502::set_decimal_flag() { set_flag(Decimal, true); return 0; }
int Processor6502::set_interrupt_flag() { set_flag(InterruptDisable, true); return 0; }
int Processor6502::set_overflow_flag() { set_flag(Overflow, true); return 0; }
int Processor6502::store_accumulator() { write_memory(absolute_address_, accumulator_); return 0; }
int Processor6502::store_x_register() { write_memory(absolute_address_, index_x_); return 0; }
int Processor6502::store_y_register() { write_memory(absolute_address_, index_y_); return 0; }
int Processor6502::transfer_accumulator_to_x() { index_x_ = accumulator_; set_flag(Zero, index_x_ == 0); set_flag(Negative, index_x_ & 0x80); return 0; }
int Processor6502::transfer_accumulator_to_y() { index_y_ = accumulator_; set_flag(Zero, index_y_ == 0); set_flag(Negative, index_y_ & 0x80); return 0; }
int Processor6502::transfer_stack_pointer_to_x() { index_x_ = stack_pointer_; set_flag(Zero, index_x_ == 0); set_flag(Negative, index_x_ & 0x80); return 0; }
int Processor6502::transfer_x_to_accumulator() { accumulator_ = index_x_; set_flag(Zero, accumulator_ == 0); set_flag(Negative, accumulator_ & 0x80); return 0; }
int Processor6502::transfer_x_to_stack_pointer() { stack_pointer_ = index_x_; return 0; }
int Processor6502::transfer_y_to_accumulator() { accumulator_ = index_y_; set_flag(Zero, accumulator_ == 0); set_flag(Negative, accumulator_ & 0x80); return 0; }

// Build the opcode lookup table.
// For brevity and clarity we initialize the table with the standard official opcodes.
// Unofficial opcodes map to NOP here.
const std::array<Processor6502::Instruction, 256> Processor6502::instruction_table_ = create_instruction_table();

static const std::array<Processor6502::Instruction, 256> create_instruction_table() {
    using Instruction = Processor6502::Instruction;
    std::array<Instruction, 256> table;
    // Fill with default NOP
    for (int i = 0; i < 256; ++i) {
        table[i] = { "NOP", &Processor6502::no_operation, &Processor6502::implied_mode, 2 };
    }
    // Official opcodes (mnemonic, operation, addressing mode, cycles)
    table[0x69] = { "ADC", &Processor6502::add_with_carry, &Processor6502::immediate_mode, 2 };
    table[0x65] = { "ADC", &Processor6502::add_with_carry, &Processor6502::zero_page_mode, 3 };
    table[0x75] = { "ADC", &Processor6502::add_with_carry, &Processor6502::zero_page_x_mode, 4 };
    table[0x6D] = { "ADC", &Processor6502::add_with_carry, &Processor6502::absolute_mode, 4 };
    table[0x7D] = { "ADC", &Processor6502::add_with_carry, &Processor6502::absolute_x_mode, 4 };
    table[0x79] = { "ADC", &Processor6502::add_with_carry, &Processor6502::absolute_y_mode, 4 };
    table[0x61] = { "ADC", &Processor6502::add_with_carry, &Processor6502::indexed_indirect_mode, 6 };
    table[0x71] = { "ADC", &Processor6502::add_with_carry, &Processor6502::indirect_indexed_mode, 5 };

    table[0x29] = { "AND", &Processor6502::logical_and, &Processor6502::immediate_mode, 2 };
    table[0x25] = { "AND", &Processor6502::logical_and, &Processor6502::zero_page_mode, 3 };
    table[0x35] = { "AND", &Processor6502::logical_and, &Processor6502::zero_page_x_mode, 4 };
    table[0x2D] = { "AND", &Processor6502::logical_and, &Processor6502::absolute_mode, 4 };
    table[0x3D] = { "AND", &Processor6502::logical_and, &Processor6502::absolute_x_mode, 4 };
    table[0x39] = { "AND", &Processor6502::logical_and, &Processor6502::absolute_y_mode, 4 };
    table[0x21] = { "AND", &Processor6502::logical_and, &Processor6502::indexed_indirect_mode, 6 };
    table[0x31] = { "AND", &Processor6502::logical_and, &Processor6502::indirect_indexed_mode, 5 };

    table[0x0A] = { "ASL", &Processor6502::arithmetic_shift_left, &Processor6502::implied_mode, 2 };
    table[0x06] = { "ASL", &Processor6502::arithmetic_shift_left, &Processor6502::zero_page_mode, 5 };
    table[0x16] = { "ASL", &Processor6502::arithmetic_shift_left, &Processor6502::zero_page_x_mode, 6 };
    table[0x0E] = { "ASL", &Processor6502::arithmetic_shift_left, &Processor6502::absolute_mode, 6 };
    table[0x1E] = { "ASL", &Processor6502::arithmetic_shift_left, &Processor6502::absolute_x_mode, 7 };

    table[0x90] = { "BCC", &Processor6502::branch_carry_clear, &Processor6502::relative_mode, 2 };
    table[0xB0] = { "BCS", &Processor6502::branch_carry_set, &Processor6502::relative_mode, 2 };
    table[0xF0] = { "BEQ", &Processor6502::branch_equal, &Processor6502::relative_mode, 2 };

    table[0x24] = { "BIT", &Processor6502::test_bits, &Processor6502::zero_page_mode, 3 };
    table[0x2C] = { "BIT", &Processor6502::test_bits, &Processor6502::absolute_mode, 4 };

    table[0x30] = { "BMI", &Processor6502::branch_minus, &Processor6502::relative_mode, 2 };
    table[0xD0] = { "BNE", &Processor6502::branch_not_equal, &Processor6502::relative_mode, 2 };
    table[0x10] = { "BPL", &Processor6502::branch_plus, &Processor6502::relative_mode, 2 };
    table[0x00] = { "BRK", &Processor6502::software_interrupt, &Processor6502::implied_mode, 7 };
    table[0x50] = { "BVC", &Processor6502::branch_overflow_clear, &Processor6502::relative_mode, 2 };
    table[0x70] = { "BVS", &Processor6502::branch_overflow_set, &Processor6502::relative_mode, 2 };

    table[0x18] = { "CLC", &Processor6502::clear_carry_flag, &Processor6502::implied_mode, 2 };
    table[0xD8] = { "CLD", &Processor6502::clear_decimal_flag, &Processor6502::implied_mode, 2 };
    table[0x58] = { "CLI", &Processor6502::clear_interrupt_flag, &Processor6502::implied_mode, 2 };
    table[0xB8] = { "CLV", &Processor6502::clear_overflow_flag, &Processor6502::implied_mode, 2 };

    table[0xC9] = { "CMP", &Processor6502::compare_with_accumulator, &Processor6502::immediate_mode, 2 };
    table[0xC5] = { "CMP", &Processor6502::compare_with_accumulator, &Processor6502::zero_page_mode, 3 };
    table[0xD5] = { "CMP", &Processor6502::compare_with_accumulator, &Processor6502::zero_page_x_mode, 4 };
    table[0xCD] = { "CMP", &Processor6502::compare_with_accumulator, &Processor6502::absolute_mode, 4 };
    table[0xDD] = { "CMP", &Processor6502::compare_with_accumulator, &Processor6502::absolute_x_mode, 4 };
    table[0xD9] = { "CMP", &Processor6502::compare_with_accumulator, &Processor6502::absolute_y_mode, 4 };
    table[0xC1] = { "CMP", &Processor6502::compare_with_accumulator, &Processor6502::indexed_indirect_mode, 6 };
    table[0xD1] = { "CMP", &Processor6502::compare_with_accumulator, &Processor6502::indirect_indexed_mode, 5 };

    table[0xE0] = { "CPX", &Processor6502::compare_with_x_register, &Processor6502::immediate_mode, 2 };
    table[0xE4] = { "CPX", &Processor6502::compare_with_x_register, &Processor6502::zero_page_mode, 3 };
    table[0xEC] = { "CPX", &Processor6502::compare_with_x_register, &Processor6502::absolute_mode, 4 };

    table[0xC0] = { "CPY", &Processor6502::compare_with_y_register, &Processor6502::immediate_mode, 2 };
    table[0xC4] = { "CPY", &Processor6502::compare_with_y_register, &Processor6502::zero_page_mode, 3 };
    table[0xCC] = { "CPY", &Processor6502::compare_with_y_register, &Processor6502::absolute_mode, 4 };

    table[0xC6] = { "DEC", &Processor6502::decrement_memory, &Processor6502::zero_page_mode, 5 };
    table[0xD6] = { "DEC", &Processor6502::decrement_memory, &Processor6502::zero_page_x_mode, 6 };
    table[0xCE] = { "DEC", &Processor6502::decrement_memory, &Processor6502::absolute_mode, 6 };
    table[0xDE] = { "DEC", &Processor6502::decrement_memory, &Processor6502::absolute_x_mode, 7 };

    table[0xCA] = { "DEX", &Processor6502::decrement_x_register, &Processor6502::implied_mode, 2 };
    table[0x88] = { "DEY", &Processor6502::decrement_y_register, &Processor6502::implied_mode, 2 };

    table[0x49] = { "EOR", &Processor6502::exclusive_or, &Processor6502::immediate_mode, 2 };
    table[0x45] = { "EOR", &Processor6502::exclusive_or, &Processor6502::zero_page_mode, 3 };
    table[0x55] = { "EOR", &Processor6502::exclusive_or, &Processor6502::zero_page_x_mode, 4 };
    table[0x4D] = { "EOR", &Processor6502::exclusive_or, &Processor6502::absolute_mode, 4 };
    table[0x5D] = { "EOR", &Processor6502::exclusive_or, &Processor6502::absolute_x_mode, 4 };
    table[0x59] = { "EOR", &Processor6502::exclusive_or, &Processor6502::absolute_y_mode, 4 };
    table[0x41] = { "EOR", &Processor6502::exclusive_or, &Processor6502::indexed_indirect_mode, 6 };
    table[0x51] = { "EOR", &Processor6502::exclusive_or, &Processor6502::indirect_indexed_mode, 5 };

    table[0xE6] = { "INC", &Processor6502::increment_memory, &Processor6502::zero_page_mode, 5 };
    table[0xF6] = { "INC", &Processor6502::increment_memory, &Processor6502::zero_page_x_mode, 6 };
    table[0xEE] = { "INC", &Processor6502::increment_memory, &Processor6502::absolute_mode, 6 };
    table[0xFE] = { "INC", &Processor6502::increment_memory, &Processor6502::absolute_x_mode, 7 };

    table[0xE8] = { "INX", &Processor6502::increment_x_register, &Processor6502::implied_mode, 2 };
    table[0xC8] = { "INY", &Processor6502::increment_y_register, &Processor6502::implied_mode, 2 };

    table[0x4C] = { "JMP", &Processor6502::jump_to_subroutine, &Processor6502::absolute_mode, 3 };
    table[0x6C] = { "JMP", &Processor6502::jump_to_subroutine, &Processor6502::indirect_mode, 5 };
    table[0x20] = { "JSR", &Processor6502::jump_to_subroutine, &Processor6502::absolute_mode, 6 };

    table[0xA9] = { "LDA", &Processor6502::load_accumulator, &Processor6502::immediate_mode, 2 };
    table[0xA5] = { "LDA", &Processor6502::load_accumulator, &Processor6502::zero_page_mode, 3 };
    table[0xB5] = { "LDA", &Processor6502::load_accumulator, &Processor6502::zero_page_x_mode, 4 };
    table[0xAD] = { "LDA", &Processor6502::load_accumulator, &Processor6502::absolute_mode, 4 };
    table[0xBD] = { "LDA", &Processor6502::load_accumulator, &Processor6502::absolute_x_mode, 4 };
    table[0xB9] = { "LDA", &Processor6502::load_accumulator, &Processor6502::absolute_y_mode, 4 };
    table[0xA1] = { "LDA", &Processor6502::load_accumulator, &Processor6502::indexed_indirect_mode, 6 };
    table[0xB1] = { "LDA", &Processor6502::load_accumulator, &Processor6502::indirect_indexed_mode, 5 };

    table[0xA2] = { "LDX", &Processor6502::load_x_register, &Processor6502::immediate_mode, 2 };
    table[0xA6] = { "LDX", &Processor6502::load_x_register, &Processor6502::zero_page_mode, 3 };
    table[0xB6] = { "LDX", &Processor6502::load_x_register, &Processor6502::zero_page_y_mode, 4 };
    table[0xAE] = { "LDX", &Processor6502::load_x_register, &Processor6502::absolute_mode, 4 };
    table[0xBE] = { "LDX", &Processor6502::load_x_register, &Processor6502::absolute_y_mode, 4 };

    table[0xA0] = { "LDY", &Processor6502::load_y_register, &Processor6502::immediate_mode, 2 };
    table[0xA4] = { "LDY", &Processor6502::load_y_register, &Processor6502::zero_page_mode, 3 };
    table[0xB4] = { "LDY", &Processor6502::load_y_register, &Processor6502::zero_page_x_mode, 4 };
    table[0xAC] = { "LDY", &Processor6502::load_y_register, &Processor6502::absolute_mode, 4 };
    table[0xBC] = { "LDY", &Processor6502::load_y_register, &Processor6502::absolute_x_mode, 4 };

    table[0x4A] = { "LSR", &Processor6502::logical_shift_right, &Processor6502::implied_mode, 2 };
    table[0x46] = { "LSR", &Processor6502::logical_shift_right, &Processor6502::zero_page_mode, 5 };
    table[0x56] = { "LSR", &Processor6502::logical_shift_right, &Processor6502::zero_page_x_mode, 6 };
    table[0x4E] = { "LSR", &Processor6502::logical_shift_right, &Processor6502::absolute_mode, 6 };
    table[0x5E] = { "LSR", &Processor6502::logical_shift_right, &Processor6502::absolute_x_mode, 7 };

    table[0xEA] = { "NOP", &Processor6502::no_operation, &Processor6502::implied_mode, 2 };

    table[0x09] = { "ORA", &Processor6502::or_with_accumulator, &Processor6502::immediate_mode, 2 };
    table[0x05] = { "ORA", &Processor6502::or_with_accumulator, &Processor6502::zero_page_mode, 3 };
    table[0x15] = { "ORA", &Processor6502::or_with_accumulator, &Processor6502::zero_page_x_mode, 4 };
    table[0x0D] = { "ORA", &Processor6502::or_with_accumulator, &Processor6502::absolute_mode, 4 };
    table[0x1D] = { "ORA", &Processor6502::or_with_accumulator, &Processor6502::absolute_x_mode, 4 };
    table[0x19] = { "ORA", &Processor6502::or_with_accumulator, &Processor6502::absolute_y_mode, 4 };
    table[0x01] = { "ORA", &Processor6502::or_with_accumulator, &Processor6502::indexed_indirect_mode, 6 };
    table[0x11] = { "ORA", &Processor6502::or_with_accumulator, &Processor6502::indirect_indexed_mode, 5 };

    table[0x48] = { "PHA", &Processor6502::push_accumulator, &Processor6502::implied_mode, 3 };
    table[0x08] = { "PHP", &Processor6502::push_processor_status, &Processor6502::implied_mode, 3 };
    table[0x68] = { "PLA", &Processor6502::pull_accumulator, &Processor6502::implied_mode, 4 };
    table[0x28] = { "PLP", &Processor6502::pull_processor_status, &Processor6502::implied_mode, 4 };

    table[0x2A] = { "ROL", &Processor6502::rotate_left, &Processor6502::implied_mode, 2 };
    table[0x26] = { "ROL", &Processor6502::rotate_left, &Processor6502::zero_page_mode, 5 };
    table[0x36] = { "ROL", &Processor6502::rotate_left, &Processor6502::zero_page_x_mode, 6 };
    table[0x2E] = { "ROL", &Processor6502::rotate_left, &Processor6502::absolute_mode, 6 };
    table[0x3E] = { "ROL", &Processor6502::rotate_left, &Processor6502::absolute_x_mode, 7 };

    table[0x6A] = { "ROR", &Processor6502::rotate_right, &Processor6502::implied_mode, 2 };
    table[0x66] = { "ROR", &Processor6502::rotate_right, &Processor6502::zero_page_mode, 5 };
    table[0x76] = { "ROR", &Processor6502::rotate_right, &Processor6502::zero_page_x_mode, 6 };
    table[0x6E] = { "ROR", &Processor6502::rotate_right, &Processor6502::absolute_mode, 6 };
    table[0x7E] = { "ROR", &Processor6502::rotate_right, &Processor6502::absolute_x_mode, 7 };

    table[0x40] = { "RTI", &Processor6502::return_from_interrupt, &Processor6502::implied_mode, 6 };
    table[0x60] = { "RTS", &Processor6502::return_from_subroutine, &Processor6502::implied_mode, 6 };

    table[0xE9] = { "SBC", &Processor6502::subtract_with_carry, &Processor6502::immediate_mode, 2 };
    table[0xE5] = { "SBC", &Processor6502::subtract_with_carry, &Processor6502::zero_page_mode, 3 };
    table[0xF5] = { "SBC", &Processor6502::subtract_with_carry, &Processor6502::zero_page_x_mode, 4 };
    table[0xED] = { "SBC", &Processor6502::subtract_with_carry, &Processor6502::absolute_mode, 4 };
    table[0xFD] = { "SBC", &Processor6502::subtract_with_carry, &Processor6502::absolute_x_mode, 4 };
    table[0xF9] = { "SBC", &Processor6502::subtract_with_carry, &Processor6502::absolute_y_mode, 4 };
    table[0xE1] = { "SBC", &Processor6502::subtract_with_carry, &Processor6502::indexed_indirect_mode, 6 };
    table[0xF1] = { "SBC", &Processor6502::subtract_with_carry, &Processor6502::indirect_indexed_mode, 5 };

    table[0x38] = { "SEC", &Processor6502::set_carry_flag, &Processor6502::implied_mode, 2 };
    table[0xF8] = { "SED", &Processor6502::set_decimal_flag, &Processor6502::implied_mode, 2 };
    table[0x78] = { "SEI", &Processor6502::set_interrupt_flag, &Processor6502::implied_mode, 2 };

    table[0x85] = { "STA", &Processor6502::store_accumulator, &Processor6502::zero_page_mode, 3 };
    table[0x95] = { "STA", &Processor6502::store_accumulator, &Processor6502::zero_page_x_mode, 4 };
    table[0x8D] = { "STA", &Processor6502::store_accumulator, &Processor6502::absolute_mode, 4 };
    table[0x9D] = { "STA", &Processor6502::store_accumulator, &Processor6502::absolute_x_mode, 5 };
    table[0x99] = { "STA", &Processor6502::store_accumulator, &Processor6502::absolute_y_mode, 5 };
    table[0x81] = { "STA", &Processor6502::store_accumulator, &Processor6502::indexed_indirect_mode, 6 };
    table[0x91] = { "STA", &Processor6502::store_accumulator, &Processor6502::indirect_indexed_mode, 6 };

    table[0x86] = { "STX", &Processor6502::store_x_register, &Processor6502::zero_page_mode, 3 };
    table[0x96] = { "STX", &Processor6502::store_x_register, &Processor6502::zero_page_y_mode, 4 };
    table[0x8E] = { "STX", &Processor6502::store_x_register, &Processor6502::absolute_mode, 4 };

    table[0x84] = { "STY", &Processor6502::store_y_register, &Processor6502::zero_page_mode, 3 };
    table[0x94] = { "STY", &Processor6502::store_y_register, &Processor6502::zero_page_x_mode, 4 };
    table[0x8C] = { "STY", &Processor6502::store_y_register, &Processor6502::absolute_mode, 4 };

    table[0xAA] = { "TAX", &Processor6502::transfer_accumulator_to_x, &Processor6502::implied_mode, 2 };
    table[0xA8] = { "TAY", &Processor6502::transfer_accumulator_to_y, &Processor6502::implied_mode, 2 };
    table[0xBA] = { "TSX", &Processor6502::transfer_stack_pointer_to_x, &Processor6502::implied_mode, 2 };
    table[0x8A] = { "TXA", &Processor6502::transfer_x_to_accumulator, &Processor6502::implied_mode, 2 };
    table[0x9A] = { "TXS", &Processor6502::transfer_x_to_stack_pointer, &Processor6502::implied_mode, 2 };
    table[0x98] = { "TYA", &Processor6502::transfer_y_to_accumulator, &Processor6502::implied_mode, 2 };

    // Many other official opcodes map above; this table covers official instructions.
    return table;
}

// STEP: execute one instruction
int Processor6502::step() {
    if (cycle_count_ > 0) {
        cycle_count_--;
        return cycle_count_;
    }

    current_instruction_ = read_memory(program_counter_);
    program_counter_++;
    const Instruction& instr = instruction_table_[current_instruction_];
    cycle_count_ = instr.cycles;
    int additional_cycle1 = (this->*instr.addressing)();
    int additional_cycle2 = (this->*instr.operation)();
    cycle_count_ += (additional_cycle1 & additional_cycle2);
    return cycle_count_;
}

std::string Processor6502::state() const {
    std::ostringstream o;
    o << "PC=" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << program_counter_
      << " A=" << std::setw(2) << static_cast<int>(accumulator_)
      << " X=" << std::setw(2) << static_cast<int>(index_x_)
      << " Y=" << std::setw(2) << static_cast<int>(index_y_)
      << " SP=" << std::setw(2) << static_cast<int>(stack_pointer_)
      << " P=" << std::setw(2) << static_cast<int>(status_flags_);
    return o.str();
}