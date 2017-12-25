#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include "int_alias.h"

#include <string_view>
#include <vector>

namespace nes::emulator
{
    class Cartridge final
    {
        struct Data
        {
            std::vector<unsigned char>         rom;
            std::vector<unsigned char>         ram;
            std::vector<unsigned char>        vmem;
            unsigned char              rom16_banks;
            unsigned char              vrom8_banks;
            unsigned char                   mapper;
            enum Mirror {HORIZ = 0, VERTI, SINGL} scroll_type;
        } data;
        unsigned char     bank   = 0, regs[4]{0xC, 0, 0, 0};
        bool           nt_page   = 0;

        u8 shift_reg = 16;

        Cartridge(Data&& data) noexcept : data{std::move(data)} {}

        u16 manip_chr_address(u16 address) const noexcept
        {
            switch (data.mapper)
            {
                case 3: return address + 0x2000 * bank;
                case 1:
                {
                    if (regs[0] & 16)
                    {
                        if (address < 0x1000) return address + 0x1000 * regs[1];
                        else                  return address + 0x1000 * regs[2] - 0x1000;
                    }
                    else return address + 0x2000 * (regs[1] >> 1);
                }
            }
            return address;
        }
    public:
        static Cartridge load(std::string_view filepath);

        void write_mapper(u16 address, u8 value) noexcept
        {
            switch (data.mapper)
            {
                case 3: bank = value & 3;                       break;
                case 7:                   nt_page = value & 16;
                case 2: bank = value & 7;                       break;
                case 1:
                    if (value & 128) {shift_reg = 16; regs[0] |= 0xC;}
                    else
                    {
                        const bool ready = shift_reg & 1;
                        shift_reg >>= 1;
                        shift_reg |= (value & 1) << 4;
                        if (ready)
                        {
                            regs[address >> 13 & 3] = shift_reg;
                            switch (regs[0] & 3)
                            {
                                case 0:
                                case 1: data.scroll_type = Data::SINGL; break;
                                case 2: data.scroll_type = Data::VERTI; break;
                                case 3: data.scroll_type = Data::HORIZ; break;
                            }
                            shift_reg = 16;
                        }
                    }
                break;
            }
        }

        void write_video_memory(u16 address, u8 value) noexcept {data.vmem[manip_chr_address(address)] = value;}
        u8 read_video_memory(u16 address) const noexcept {return data.vmem[manip_chr_address(address)];}

        void write_ram(u16 address, u8 value) noexcept {data.ram[address] = value;}

        u8 read_ram(u16 address) const noexcept {return data.ram[address];}
        u8 read_rom(u16 address) const noexcept
        {
            switch (data.mapper)
            {
                case 7: return data.rom[address + 0x8000 * bank];
                case 2:
                    if (address < 0x4000) return data.rom[address + 0x4000 *             bank               ];
                    else                  return data.rom[address + 0x4000 * (data.rom16_banks - 1) - 0x4000];
                case 0:
                case 3:
                    return data.rom[data.rom16_banks == 1 ? address & 0x3FFF : address];
                case 1:
                    switch (regs[0] >> 2 & 3)
                    {
                        case 0:
                        case 1:
                            return data.rom[address + 0x8000 * ((regs[3] & 15) >> 1)];
                        case 2:
                            if (address < 0x4000) return data.rom[address % 0x4000];
                            else                  return data.rom[address + 0x4000 * (regs[3] & 15) - 0x4000];
                        case 3:
                            if (address < 0x4000) return data.rom[address + 0x4000 * (regs[3] & 15)];
                            else                  return data.rom[address + 0x4000 * (data.rom16_banks - 1) - 0x4000];
                    }
            }
            return data.rom[address];
        }

        u16 mirror_address(u16 address) const noexcept
        {
            switch (data.scroll_type)
            {
                case Data::HORIZ: return ((address /     2) & 0x400) + (address % 0x400);
                case Data::VERTI: return   address % 0x800;
                case Data::SINGL:
                    switch (data.mapper)
                    {
                        case 7: return (address % 0x400) + 0x400 * nt_page;
                        case 1: return (address % 0x400) + 0x400 * (regs[0] & 1);
                    }
            }
            return address;
        }
    };
}

#endif
