#ifndef PPU_H
#define PPU_H

#include "int_alias.h"

namespace nes::emulator
{
    class Cartridge;
    class CPU;

    class PPU final
    {
    public:
        struct MemPointers
        {
            Cartridge*     cartridge;
            CPU*           cpu;
        };

    private:
        static unsigned rev_bute(unsigned byte) noexcept;
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

        enum MaskMasks : u8 {
            MASK_MASK_GREYSCALE                         = 0b00000001, // 0 - normal color; 1 - produce a greyscale display
            MASK_MASK_SHOW_BACKGROUND_LEFTMOST_8_PIXELS = 0b00000010, // 0 - hide
            MASK_MASK_SHOW_SPRITES_LEFTMOST_8_PIXELS    = 0b00000100, // 0 - hide
            MASK_MASK_SHOW_BACKGROUND                   = 0b00001000,
            MASK_MASK_SHOW_SPRITES                      = 0b00010000,
            MASK_MASK_EMPHASIZE_RED                     = 0b00100000,
            MASK_MASK_EMPHASIZE_GREEN                   = 0b01000000,
            MASK_MASK_EMPHASIZE_BLUE                    = 0b10000000
        };

        enum StatMasks : u8 {
            MASK_STAT_VBLANK          = 0b10000000,
            MASK_STAT_SPRITE_ZERO_HIT = 0b01000000,
            MASK_STAT_SPRITE_OVERFLOW = 0b00100000
        };

        static constexpr u8 MASK_MASK_RENDERING_ENABLED = MASK_MASK_SHOW_BACKGROUND | MASK_MASK_SHOW_SPRITES;

        unsigned char ctrl = 0, mask = 0, stat = 0, data_buff, open_bus_data;
        unsigned char ram[0x0800], palette[0x20], oam[256], scan_oam[32], oam_tmp;

        struct
        {
            unsigned char id, y, x[8], attr[8], pat_l[8], pat_h[8];
            bool in_range;
        } sprite;

        px32* pixel_output = nullptr;

        u8 xfine, nt, at, bg_lo, bg_hi, at_latch_hi, at_latch_lo;
        u8 oam_addr = 0, scan_oam_addr, oam_copy;

        u16 bg_shift_lo, bg_shift_hi, at_shift_lo, at_shift_hi, open_bus_addr;
        u16 clks = 0, scanline = 261, vaddr = 0, tmp_vaddr, open_bus_decay_timer = 0;

        u8 write_addr_delay = 0;

        bool write_toggle = false, odd_frame_post = false, new_frame_post = false;
        bool oam_addr_overflow, scan_oam_addr_overflow, sprite_overflow_detection, sprite_overflow = false;
        bool s0_next_scanline, s0_curr_scanline;

        MemPointers mem_pointers;

        void memory_write(u16 address, u8 value) noexcept;
        u8 memory_read(u16 address) const noexcept;

        void set_cpu_nmi(bool nmi) noexcept;

        void reload_shift_regs() noexcept
        {
            bg_shift_lo &= 0xFF00; bg_shift_lo |= bg_lo;
            bg_shift_hi &= 0xFF00; bg_shift_hi |= bg_hi;
            at_latch_lo = at      & 1;
            at_latch_hi = at >> 1 & 1;
        }

        void shift_shifters() noexcept
        {
            bg_shift_lo <<= 1;
            bg_shift_hi <<= 1;
            at_shift_lo = (at_shift_lo << 1) | at_latch_lo;
            at_shift_hi = (at_shift_hi << 1) | at_latch_hi;
        }

        void v_scroll() noexcept
        {
            if ((vaddr & 0x7000) != 0x7000) vaddr += 0x1000;
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

        bool background_pattern_table_address() const noexcept {return ctrl & CTRL_MASK_BACKGROUND_PATTERN_TABLE_ADDRESS;}
        bool sprite_pattern_table_address() const noexcept {return ctrl & CTRL_MASK_SPRITE_PATTERN_TABLE_ADDRESS;}

        u16 addr_nt() const noexcept {return 0x2000 | (vaddr & 0x0FFF);}
        u16 addr_at() const noexcept {return 0x23C0 | (vaddr & 0x0C00) | (vaddr >> 4 & 0x38) | (vaddr >> 2 & 0x07);}
        u16 addr_bg() const noexcept {return 0x1000 * background_pattern_table_address() + 16 * nt + (vaddr >> 12);}

        void access_data_increment() noexcept
        {
            if (!(mask & MASK_MASK_RENDERING_ENABLED) || (scanline >= 240 && scanline != 261))
            {
                vaddr += ctrl & CTRL_MASK_VRAM_ADDRESS_INCREMENT ? 32 : 1;
                vaddr &= 0x7FFF;
            }
            else
            {
                h_scroll();
                v_scroll();
            }
        }

        void open_bus_write_ctrl() noexcept
        {
                 if (~open_bus_data & CTRL_MASK_GENERATE_NMI) set_cpu_nmi(0);
            else if (    !(scanline == 261 && clks == 1))
            {
                 if (~ctrl   & stat & CTRL_MASK_GENERATE_NMI) set_cpu_nmi(1);
            }
            tmp_vaddr = (tmp_vaddr & 0x73FF) | ((open_bus_data & 0x03) << 10);
            ctrl = open_bus_data;
        }

        void open_bus_write_mask() noexcept
        {
            if ((mask ^ open_bus_data) & MASK_MASK_RENDERING_ENABLED)
            {
                if (scanline == 261 && clks == 339 && odd_frame_post)
                    clks = open_bus_data & MASK_MASK_RENDERING_ENABLED ? 338 : 340;
            }
            mask = open_bus_data;
        }

        void open_bus_write_oam_addr() noexcept {   oam_addr     = open_bus_data;}
        void open_bus_write_oam_data() noexcept
        {
            oam[oam_addr++] = open_bus_data; oam_addr &= 255;
        }

        void open_bus_write_scroll() noexcept
        {
            if (write_toggle)
                tmp_vaddr = (tmp_vaddr & 0x0C1F) | ((open_bus_data & 0xF8) << 2) | ((open_bus_data & 7) << 12);
            else
            {
                tmp_vaddr = (tmp_vaddr & 0x7FE0) | ((open_bus_data & 0xF8) >> 3);
                xfine = open_bus_data & 7;
            }
            write_toggle = !write_toggle;
        }

        void open_bus_write_addr() noexcept
        {
            if (write_toggle)
            {
                tmp_vaddr = (tmp_vaddr & 0x7F00) | open_bus_data;
                write_addr_delay = 2;
            }
            else 
            {
                tmp_vaddr = (tmp_vaddr & 0x00FF) | ((open_bus_data & 0x3F) << 8);
            }
            write_toggle = !write_toggle;
        }

        void open_bus_write_data() noexcept
        {
            memory_write(vaddr, open_bus_data);
            access_data_increment();
        }

        void open_bus_read_status() noexcept
        {
            if (scanline == 241)
            {
                switch (clks)
                {
                    case 1: stat &= ~MASK_STAT_VBLANK;
                    case 2:case 3: set_cpu_nmi(false); break;
                }
            }
            u8 temp = stat; stat &= ~MASK_STAT_VBLANK; write_toggle = 0;
            open_bus_data &= 31; open_bus_data |= temp & ~31;
        }

        void open_bus_read_data() noexcept
        {
            u8 temp = memory_read(vaddr);
            if (vaddr < 0x3F00) {temp = data_buff; data_buff = memory_read(vaddr); open_bus_refresh<255>(temp); }
            else                {temp = data_buff            = memory_read(vaddr); open_bus_refresh< 63>(temp); }
            access_data_increment();
        }

        void open_bus_read_oam_data() noexcept {open_bus_refresh<255>(oam[oam_addr] & 0xE3);}

        template<u8 mask>
        void open_bus_refresh(u8 value) noexcept
        {
            open_bus_decay_timer = 7777;
            open_bus_data &= ~mask; open_bus_data |= value & mask;
        }

        template<bool high>
        void fetch_sprite_pattern(int spr_index) noexcept
        {
            unsigned char* pat;
            if constexpr (high) pat = sprite.pat_h + spr_index;
            else                pat = sprite.pat_l + spr_index;
            if (!sprite.in_range) *pat = 0;
            else
            {
                *pat = memory_read(open_bus_addr);
                if (sprite.attr[spr_index] & 0x40) *pat = rev_bute(*pat);
            }
        }

        template<bool high>
        bool sprite_tile_address(unsigned attr) noexcept
        {
            const unsigned diff        =        scanline - sprite.y;
            const unsigned diff_y_flip = attr & 0x80 ? ~diff : diff;

            if (ctrl & CTRL_MASK_SPRITE_SIZE)
            {
                open_bus_addr = 0x1000 * (sprite.id & 1) + 16 * (sprite.id & 0xFE) + ((diff_y_flip & 8) << 1) +
                                                                         8 * high  +  (diff_y_flip & 7);
                return diff < 16 && scanline < 239;
            }
            else
            {
                open_bus_addr = 0x1000 * sprite_pattern_table_address() + 16 * sprite.id + 8 * high + (diff_y_flip & 7);
                return diff  < 8 && scanline < 239;
            }
        }

        unsigned sprite_pixel(unsigned &spr_pal, bool &spr_behind_bg, bool &spr_is_s0) noexcept
        {
            const unsigned x = clks - 1;
            if (!(mask & MASK_MASK_SHOW_SPRITES) || (!(mask & MASK_MASK_SHOW_SPRITES_LEFTMOST_8_PIXELS) && x < 8))
                return 0;
            for (int i = 0; i < 8; ++i)
            {
                const unsigned offset = x - sprite.x[i];
                if (offset < 8)
                {
                    const unsigned pat_res = ((sprite.pat_h[i] >> (7 - offset) & 1) << 1) |
                                              (sprite.pat_l[i] >> (7 - offset) & 1);
                    if (pat_res)
                    {
                        spr_pal       = sprite.attr[i] & 0x03;
                        spr_behind_bg = sprite.attr[i] & 0x20;
                        spr_is_s0     =            !i && s0_curr_scanline;
                        return pat_res;
                    }
                }
            }
            return 0;
        }

        void sprite_operations() noexcept;
        void sprite_evaluation() noexcept;
        void sprite_loading() noexcept;

        void background_misc() noexcept;

        void render_pixel() noexcept;

    public:
        template<unsigned reg>
        void reg_write(u8 value) noexcept
        {
            open_bus_refresh<255>(value);
                 if constexpr (reg == 0) open_bus_write_ctrl();
            else if constexpr (reg == 1) open_bus_write_mask();
            else if constexpr (reg == 3) open_bus_write_oam_addr();
            else if constexpr (reg == 4) open_bus_write_oam_data();
            else if constexpr (reg == 5) open_bus_write_scroll();
            else if constexpr (reg == 6) open_bus_write_addr();
            else if constexpr (reg == 7) open_bus_write_data();
        }

        template<unsigned reg>
        u8 reg_read() noexcept
        {
                 if constexpr (reg == 2) open_bus_read_status();
            else if constexpr (reg == 4) open_bus_read_oam_data();
            else if constexpr (reg == 7) open_bus_read_data();

            return open_bus_data;
        }

        void set_mem_pointers(const MemPointers& mem_pointers) noexcept {this->mem_pointers = mem_pointers;}
        void set_pixel_output(px32* pixel_output) noexcept {this->pixel_output = pixel_output;}
        bool odd_frame() noexcept {return odd_frame_post;}
        bool new_frame() noexcept
        {
            const bool temp = new_frame_post; new_frame_post = false;
            return     temp;
        }

        void tick() noexcept;
    };
}

#endif
