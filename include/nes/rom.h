#pragma once
#include <cstdint>
#include <vector>
#include <stdexcept>

namespace nes {
struct RomHeader {
    uint8_t program_banks;
    uint8_t graphic_banks;
    uint8_t control_flags;
    uint8_t mapper_id;
    uint8_t mirror_type; // 0=horizontal, 1=vertical
};

class RomLoader {
public:
    explicit RomLoader(const std::vector<uint8_t>& data);
    const RomHeader& get_header() const noexcept { return header_; }
    const std::vector<uint8_t>& get_program() const noexcept { return program_data_; }
    const std::vector<uint8_t>& get_graphics() const noexcept { return graphic_data_; }

private:
    RomHeader header_;
    std::vector<uint8_t> program_data_;
    std::vector<uint8_t> graphic_data_;
};

class ROM {
    // ROM class implementation
};
}