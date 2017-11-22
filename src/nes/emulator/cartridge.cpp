#include "cartridge.h"

#include <stdexcept> // std::runtime_error
#include <fstream>   // std::ifstream
#include <utility>   // std::move
#include <cstring>   // std::strcmp
#include <algorithm> // std::copy_n
#include <iterator>  // std::back_inserter
#include <iostream>  // std::clog

using namespace nes::emulator;

Cartridge Cartridge::load(std::string_view filepath)
{
    std::ifstream stream{filepath.data(), std::ios::binary | std::ios::in};
    if (!stream)
        throw std::runtime_error{"cartridge reading error"};
    {
        char buffer[5]; buffer[4] = '\0';
        if (!stream.read(buffer, 4) || std::strcmp(buffer, "NES\x1A"))
            throw std::runtime_error{"cartridge reading error: not iNES format"};
    }
    const unsigned char  rom16_banks = stream.get(),  vrom8_banks = stream.get();
    const unsigned char control_byte = stream.get(), mapper_index = stream.get() | control_byte >> 4;
    std::clog << "PRG ROM size in 16KB:\t" << (int)  rom16_banks << std::endl <<
                 "CHR ROM size in  8KB:\t" << (int)  vrom8_banks << std::endl <<
                 "mapper index:        \t" << (int) mapper_index << std::endl <<
                 "control byte:        \t" << (int) control_byte << std::endl;
    const bool mapper_supported = mapper_index == 0 || mapper_index == 2 ||
                                  mapper_index == 3 || mapper_index == 7;
    if (!mapper_supported) throw std::runtime_error{"the mapper is not supported yet; supported mappers: 0,2,3,7"};
    for (int i = 0; i < 8; ++i) stream.get();
    const unsigned rom_size = 0x4000 * rom16_banks, vmem_size = 0x2000 * vrom8_banks;
    std::vector<unsigned char> rom, ram(8192), vmem;  rom.reserve( rom_size);
    if (!vmem_size) vmem.resize(0x2000); else vmem.reserve(0x2000);
    std::copy_n(std::istreambuf_iterator<char>{stream},  rom_size + 1, std::back_inserter( rom));
    std::copy_n(std::istreambuf_iterator<char>{stream}, vmem_size + 1, std::back_inserter(vmem));
    return {{std::move( rom),
             std::move( ram),
             std::move(vmem), rom16_banks,
                              vrom8_banks, mapper_index,
             static_cast<Data::Mirror>(mapper_index == 7 ? Data::Mirror::SINGL : control_byte & 1)}};
}

