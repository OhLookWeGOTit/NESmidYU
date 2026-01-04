#include "nes/rom.h"
#include <cstring>

namespace nes {

ROM::ROM(const std::vector<uint8_t>& data) {
    if (data.size() < 16 || std::memcmp(data.data(), "NES\x1A", 4) != 0) {
        throw std::runtime_error("Not a valid iNES file");
    }
    header_.prg_rom_chunks = data[4];
    header_.chr_rom_chunks = data[5];
    header_.flags6 = data[6];
    header_.flags7 = data[7];
    header_.mapper = (header_.flags7 & 0xF0) | (header_.flags6 >> 4);
    size_t offset = 16;
    if (header_.flags6 & 0x04) offset += 512;
    size_t prg_size = static_cast<size_t>(header_.prg_rom_chunks) * 16 * 1024;
    size_t chr_size = static_cast<size_t>(header_.chr_rom_chunks) * 8 * 1024;
    if (offset + prg_size > data.size()) throw std::runtime_error("Truncated PRG ROM");
    prg_rom_.assign(data.begin() + offset, data.begin() + offset + prg_size);
    offset += prg_size;
    if (chr_size > 0) {
        if (offset + chr_size > data.size()) throw std::runtime_error("Truncated CHR ROM");
        chr_rom_.assign(data.begin() + offset, data.begin() + offset + chr_size);
    }
}

const INesHeader& ROM::header() const noexcept { return header_; }
const std::vector<uint8_t>& ROM::prg() const noexcept { return prg_rom_; }
const std::vector<uint8_t>& ROM::chr() const noexcept { return chr_rom_; }

}