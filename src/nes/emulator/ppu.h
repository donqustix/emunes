#ifndef PPU_H
#define PPU_H

#include "int_alias.h"

namespace nes::emulator
{
    class Cartridge;

    class PPU final
    {
        friend class MMU;

    public:
        struct MemPointers
        {
            unsigned char* internal_ram;
            Cartridge*     cartridge;
        };

    private:
        enum CtrlMasks : u8 {
            CTRL_MASK_BASE_NAMETABLE_ADDRESS            = 0b00000011, // 0 - $2000; 1 - $2400; 2 - $2800; 3 - $2C00
            CTRL_MASK_VRAM_ADDRESS_INCREMENT            = 0b00000100, // 0 - 1; 1 - 32
            CTRL_MASK_SPRITE_PATTERN_TABLE_ADDRESS      = 0b00001000, // 0 - $0000; 1 - $1000
            CTRL_MASK_BACKGROUND_PATTERN_TABLE_ADDRESS  = 0b00010000, // 0 - $0000; 1 - $1000
            CTRL_MASK_SPRITE_SIZE                       = 0b00100000, // 0 - 8x8; 1 - 8x16
            CTRL_MASK_MASTER_SLAVE_SELECT               = 0b01000000, // 0 - read backdrop from EXT pins;
                                                                       // 1 - output color on EXT pins
            CTRL_MASK_GENERATE_NMI                      = 0b10000000  // 0 - off; 1 - on
        };

        enum MaskMasks : u8{
            MASK_MASK_GREYSCALE                         = 0b00000001, // 0 - normal color; 1 - produce a greyscale display
            MASK_MASK_SHOW_BACKGROUND_LEFTMOST_8_PIXELS = 0b00000010, // 0 - hide
            MASK_MASK_SHOW_SPRITES_LEFTMOST_8_PIXELS    = 0b00000100, // 0 - hide
            MASK_MASK_SHOW_BACKGROUND                   = 0b00001000,
            MASK_MASK_SHOW_SPRITES                      = 0b00010000,
            MASK_MASK_EMPHASIZE_RED                     = 0b00100000,
            MASK_MASK_EMPHASIZE_GREEN                   = 0b01000000,
            MASK_MASK_EMPHASIZE_BLUE                    = 0b10000000
        };

        static constexpr u8 MASK_MASK_RENDERING_ENABLED = MASK_MASK_SHOW_BACKGROUND | MASK_MASK_SHOW_SPRITES;

        unsigned char ctrl = 0, mask = 0, data_buff, open_bus;
        unsigned char ram[0x0800], palette[0x20], oam[256], scan_oam[32];

        unsigned framebuffer[240 * 256];

        u8 xfine, nt, at, bg_lo, bg_hi, at_latch_hi, at_latch_lo;

        u16 bg_shift_lo, bg_shift_hi, at_shift_lo, at_shift_hi, faddr;
        u16 clks = 0, scanline = 261, vaddr = 0, tmp_vaddr, open_bus_decay_timer = 0;

        bool write_toggle = false, odd_frame = false, vblank = false, new_frame_post = false;

        MemPointers mem_pointers;

        void memory_write(u16 address, u8 value) noexcept;
        u8 memory_read(u16 address) const noexcept;

        void reload_shift_regs() noexcept
        {
            bg_shift_lo &= 0xFF00; bg_shift_lo |= bg_lo;
            bg_shift_hi &= 0xFF00; bg_shift_hi |= bg_hi;
            at_latch_lo = at      & 1;
            at_latch_hi = at >> 1 & 1;
        }

        void v_scroll() noexcept
        {
            if ((vaddr  & 0x7000) != 0x7000)
                 vaddr += 0x1000;
            else
            {
                auto y = vaddr >> 5 & 31;
                     if (y == 29) {y = 0; vaddr ^= 0x0800;}
                else if (y == 31)  y = 0;
                else   ++y;
                vaddr &= ~0x73E0; vaddr |= y << 5;
            }
        }

        void h_scroll() noexcept
        {
            if ((vaddr & 31) == 31) vaddr ^= 0x041F; else ++vaddr;
        }

        void v_update() noexcept {vaddr &= ~0x7BE0; vaddr |= tmp_vaddr & 0x7BE0;}
        void h_update() noexcept {vaddr &= ~0x041F; vaddr |= tmp_vaddr & 0x041F;}

        bool pattern_table_address() const noexcept {return ctrl & CTRL_MASK_BACKGROUND_PATTERN_TABLE_ADDRESS;}

        u16 addr_nt() const noexcept {return 0x2000 | (vaddr & 0x0FFF);}
        u16 addr_at() const noexcept {return 0x23C0 | (vaddr & 0x0C00) | (vaddr >> 4 & 0x38) | (vaddr >> 2 & 0x07);}
        u16 addr_bg() const noexcept {return 0x1000 * pattern_table_address() + 16 * nt + (vaddr >> 12);}

        void open_bus_write_ctrl() noexcept
        {
            tmp_vaddr &=              0b111001111111111;
            tmp_vaddr |=  (open_bus & 0b000000000000011) << 10;
            ctrl = open_bus;
        }

        void open_bus_write_mask() noexcept {mask = open_bus;}

        void open_bus_write_scroll() noexcept
        {
            if (write_toggle)
            {
                tmp_vaddr &=             0b000110000011111;
                tmp_vaddr |= (open_bus & 0b000000000000111) << 12;
                tmp_vaddr |= (open_bus & 0b000000011111000) <<  2;
            }
            else
            {
                tmp_vaddr &=             0b111111111100000;
                tmp_vaddr |= (open_bus & 0b000000011111000) >> 3;
                xfine      =  open_bus & 0xb00000111;
            }
            write_toggle = !write_toggle;
        }

        void open_bus_write_addr() noexcept
        {
            if (write_toggle)
            {
                tmp_vaddr &=            0b111111100000000;
                tmp_vaddr |= open_bus & 0b000000011111111;
                vaddr = tmp_vaddr;
            }
            else
            {
                tmp_vaddr &=             0b000000011111111;
                tmp_vaddr |= (open_bus & 0b000000000111111) << 8;
            }
            write_toggle = !write_toggle;
        }

        void open_bus_write_data() noexcept
        {
            memory_write(vaddr, open_bus);
            vaddr += ctrl & CTRL_MASK_VRAM_ADDRESS_INCREMENT ? 32 : 1;
            vaddr &= 0x7FFF;
        }

        void open_bus_read_status() noexcept
        {
            const u8 temp = vblank << 7; write_toggle = vblank = false;
            open_bus &= 31; open_bus |= temp & ~31;
        }

        void open_bus_read_data() noexcept
        {
            u8 temp = memory_read(vaddr);
            if (vaddr < 0x3F00) {temp = data_buff; data_buff = memory_read(vaddr); open_bus_refresh<255>(temp); }
            else                {temp = data_buff            = memory_read(vaddr); open_bus_refresh< 63>(temp); }
            vaddr += ctrl & CTRL_MASK_VRAM_ADDRESS_INCREMENT ? 32 : 1;
            vaddr &= 0x7FFF;
        }

        template<u8 mask>
        void open_bus_refresh(u8 value) noexcept
        {
            open_bus_decay_timer = 7777;
            open_bus &= ~mask; open_bus |= value & mask;
        }

        void render_pixel() noexcept;

    public:
        PPU() noexcept
        {
            for (int i = 0; i < 0x800; ++i) ram[i] = (i & 4) ? 255 : 0;
        }

        template<unsigned reg>
        void reg_write(u8 value) noexcept
        {
            open_bus_refresh<255>(value);
                 if constexpr (reg == 0) open_bus_write_ctrl();
            else if constexpr (reg == 1) open_bus_write_mask();
            else if constexpr (reg == 5) open_bus_write_scroll();
            else if constexpr (reg == 6) open_bus_write_addr();
            else if constexpr (reg == 7) open_bus_write_data();
        }

        template<unsigned reg>
        u8 reg_read() noexcept
        {
                 if constexpr (reg == 2) open_bus_read_status();
            else if constexpr (reg == 7) open_bus_read_data();

            return open_bus;
        }

        void set_mem_pointers(const MemPointers& mem_pointers) noexcept {this->mem_pointers = mem_pointers;}
        bool nmi() const noexcept {return vblank && (ctrl & CTRL_MASK_GENERATE_NMI);}
        bool new_frame() noexcept
        {
            const bool temp = new_frame_post; new_frame_post = false;
            return     temp;
        }

        void tick() noexcept;

        const unsigned* get_framebuffer() const noexcept {return framebuffer;}
    };
}

#endif
