from dataclasses import dataclass

@dataclass
class INesHeader:
    prg_rom_chunks: int
    chr_rom_chunks: int
    flags6: int
    flags7: int
    mapper: int

class ROM:
    def __init__(self, data: bytes):
        if len(data) < 16 or data[0:4] != b"NES\x1a":
            raise ValueError("Not a valid iNES file")
        header = data[0:16]
        prg_chunks = header[4]
        chr_chunks = header[5]
        flags6 = header[6]
        flags7 = header[7]
        mapper = (flags7 & 0xF0) | (flags6 >> 4)
        self.header = INesHeader(prg_chunks, chr_chunks, flags6, flags7, mapper)
        offset = 16
        if flags6 & 0x04:
            # trainer present (512 bytes)
            offset += 512
        prg_size = prg_chunks * 16 * 1024
        chr_size = chr_chunks * 8 * 1024
        self.prg_rom = data[offset:offset + prg_size]
        offset += prg_size
        self.chr_rom = data[offset:offset + chr_size] if chr_size > 0 else b""