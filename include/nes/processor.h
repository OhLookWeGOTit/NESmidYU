#pragma once
#include "nes/memory.h"
#include <cstdint>
#include <string>
#include <array>

namespace nes {
class Processor6502 {
public:
    explicit Processor6502(const MemoryMap* memory);
    void initialize();
    int execute_cycle();
    std::string get_status() const;
    uint16_t get_program_counter() const noexcept { return program_counter_; }

private:
    const MemoryMap* memory_;
    uint8_t accumulator_, index_x_, index_y_, stack_pointer_;
    uint16_t program_counter_;
    uint8_t status_flags_;
    uint16_t absolute_address_, relative_offset_;
    uint8_t operand_value_, current_instruction_;
    int cycle_count_;

    enum StatusBits { Carry = 1 << 0, Zero = 1 << 1, InterruptDisable = 1 << 2, Decimal = 1 << 3, Break = 1 << 4, Unused = 1 << 5, Overflow = 1 << 6, Negative = 1 << 7 };
    inline uint8_t check_flag(uint8_t flag) const { return (status_flags_ & flag) ? 1 : 0; }
    inline void set_flag(uint8_t flag, bool state) { if (state) status_flags_ |= flag; else status_flags_ &= ~flag; }

    inline uint8_t read_memory(uint16_t addr) const { return memory_->fetch(addr); }
    inline void write_memory(uint16_t addr, uint8_t val) { const_cast<MemoryMap*>(memory_)->store(addr, val); }
    inline void push_stack(uint8_t val) { write_memory(0x0100 + stack_pointer_, val); stack_pointer_--; }
    inline uint8_t pull_stack() { stack_pointer_++; return read_memory(0x0100 + stack_pointer_); }

    int implied_mode(), immediate_mode(), zero_page_mode(), zero_page_x_mode(), zero_page_y_mode(), relative_mode(), absolute_mode(), absolute_x_mode(), absolute_y_mode(), indirect_mode(), indexed_indirect_mode(), indirect_indexed_mode();
    uint8_t load_operand();
    int add_with_carry(), logical_and(), arithmetic_shift_left(), branch_carry_clear(), branch_carry_set(), branch_equal(), test_bits(), branch_minus(), branch_not_equal(), branch_plus(), software_interrupt(), branch_overflow_clear(), branch_overflow_set(), clear_carry(), clear_decimal(), clear_interrupt(), clear_overflow(), compare_accumulator(), compare_x(), compare_y(), decrement_memory(), decrement_x(), decrement_y(), exclusive_or(), increment_memory(), increment_x(), increment_y(), jump_absolute(), jump_subroutine(), load_accumulator(), load_x(), load_y(), logical_shift_right(), no_operation(), logical_or(), push_accumulator(), push_flags(), pull_accumulator(), pull_flags(), rotate_left(), rotate_right(), return_interrupt(), return_subroutine(), subtract_with_carry(), set_carry(), set_decimal(), set_interrupt(), store_accumulator(), store_x(), store_y(), transfer_accumulator_x(), transfer_accumulator_y(), transfer_stack_x(), transfer_x_accumulator(), transfer_x_stack(), transfer_y_accumulator();
    void perform_comparison(uint8_t register_value, uint8_t compare_value);
    static const std::array<Instruction, 256> instruction_table_;
    struct Instruction { const char* mnemonic; int (Processor6502::*operation)(); int (Processor6502::*addressing)(); uint8_t cycles; };
};
}