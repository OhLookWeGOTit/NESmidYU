#include "nes/rom.h"
#include <cstring>

namespace nes {
RomLoader::RomLoader(const std::vector<uint8_t>& data) {
    if (data.size() < 16 || std::memcmp(data.data(), "NES\x1A", 4) != 0) {
        throw std::runtime_error("Invalid ROM format");
    }
    header_.program_banks = data[4];
    header_.graphic_banks = data[5];
    header_.control_flags = data[6];
    header_.mapper_id = (data[7] & 0xF0) | (data[6] >> 4);
    header_.mirror_type = (data[6] & 0x01);
    size_t offset = 16;
    if (header_.control_flags & 0x04) offset += 512;
    size_t prog_size = static_cast<size_t>(header_.program_banks) * 16 * 1024;
    size_t graph_size = static_cast<size_t>(header_.graphic_banks) * 8 * 1024;
    if (offset + prog_size > data.size()) throw std::runtime_error("Incomplete program data");
    program_data_.assign(data.begin() + offset, data.begin() + offset + prog_size);
    offset += prog_size;
    if (graph_size > 0) {
        if (offset + graph_size > data.size()) throw std::runtime_error("Incomplete graphic data");
        graphic_data_.assign(data.begin() + offset, data.begin() + offset + graph_size);
    }
}
}