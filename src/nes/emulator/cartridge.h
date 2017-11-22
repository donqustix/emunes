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
            enum Mirror {HORIZ, VERTI, SINGL} scroll_type;
        } data;
        unsigned char     bank   = 0;
        bool           nt_page   = 0;

        Cartridge(Data&& data) noexcept : data{std::move(data)} {}
    public:
        static Cartridge load(std::string_view filepath);

        void write_video_memory(u16 address, u8 value) noexcept
        {
            if (data.mapper == 3) data.vmem[address + 0x2000 * bank] = value;
            else                  data.vmem[address                ] = value;
        }

        u8 read_video_memory(u16 address) const noexcept
        {
            if (data.mapper == 3) return data.vmem[address + 0x2000 * bank];
            else                  return data.vmem[address                ];
        }

        void write_ram(u16 address, u8 value) noexcept {data.ram[address] = value;}
        void write_mapper(u8 value) noexcept
        {
            switch (data.mapper)
            {
                case 3: bank = value & 3;                       break;
                case 7:                   nt_page = value & 16;
                case 2: bank = value & 7;                       break;
            }
        }

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
            }
            return data.rom[address];
        }

        u16 mirror_address(u16 address) const noexcept
        {
            switch (data.scroll_type)
            {
                case Data::HORIZ: return ((address /     2) & 0x400) + (address % 0x400);
                case Data::VERTI: return   address % 0x800;
                case Data::SINGL: return  (address & 0x3FF) + 0x400 * nt_page;
                default:          return   address;
            }
        }
    };
}

#endif
