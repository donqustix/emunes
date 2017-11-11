#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include "int_alias.h"

#include <string_view>
#include <vector>

namespace nes::emulator
{
    class Cartridge final
    {
        std::vector<unsigned char> rom, ram, vmem;
    public:
        static Cartridge load(std::string_view filepath);

        Cartridge(std::vector<unsigned char> rom,
                  std::vector<unsigned char> ram,
                  std::vector<unsigned char> vmem) noexcept;

        void write_video_memory(u16 address, u8 value) noexcept {vmem[address] = value;}
        u8 read_video_memory(u16 address) const noexcept {return vmem[address];}

        void write_ram(u16 address, u8 value) noexcept {ram[address] = value;}
        u8 read_ram(u16 address) const noexcept {return ram[address];}
        u8 read_rom(u16 address) const noexcept {return rom[address];}
    };
}

#endif