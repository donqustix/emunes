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
    const int  rom16_banks = stream.get(),  vrom8_banks = stream.get();
    const int control_byte = stream.get(), mapper_index = stream.get() | control_byte >> 4;
    std::clog << "PRG ROM size in 16KB:\t" <<  rom16_banks << std::endl <<
                 "CHR ROM size in  8KB:\t" <<  vrom8_banks << std::endl <<
                 "mapper index:        \t" << mapper_index << std::endl;
    for (int i = 0; i < 8; ++i) stream.get();
    const int rom_size = 16384 * rom16_banks, vmem_size = 8192 * vrom8_banks;
    std::vector<unsigned char> rom, ram(8192), vmem;  rom.reserve( rom_size);
                                                     vmem.reserve(vmem_size);
    std::copy_n(std::istreambuf_iterator<char>{stream},  rom_size, std::back_inserter(rom)); stream.get(); // FIXME: strange byte
    std::copy_n(std::istreambuf_iterator<char>{stream}, vmem_size, std::back_inserter(vmem));
    return Cartridge{std::move(rom), std::move(ram), std::move(vmem)};
}

Cartridge::Cartridge(std::vector<unsigned char> rom,
                     std::vector<unsigned char> ram,
                     std::vector<unsigned char> vmem) noexcept :
    rom {std::move(rom)}, ram{std::move(ram)}, vmem{std::move(vmem)}{}

