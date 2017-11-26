#include "ppu.h"

#include "cartridge.h"
#include "cpu.h"

using namespace nes::emulator;

unsigned PPU::rev_bute(unsigned n) noexcept
{
    static constexpr unsigned char rev_table[] = {
      0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
      0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
      0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
      0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
      0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
      0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
      0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
      0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
      0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
      0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
      0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
      0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
      0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
      0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
      0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
      0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF };
    return rev_table[n];
}

void PPU::memory_write(u16 address, u8 value) noexcept
{
    if      (address < 0x2000)     mem_pointers.cartridge->write_video_memory(address, value);
    else if (address < 0x3F00) ram[mem_pointers.cartridge->mirror_address(address - 0x2000)]  = value;
    else if (address < 0x4000)
        palette[((address & 0x13) == 0x10 ? address & ~0x10 : address) & 0x1F] = value;
}

u8 PPU::memory_read(u16 address) const noexcept
{
    if      (address < 0x2000) return     mem_pointers.cartridge->read_video_memory(address);
    else if (address < 0x3F00) return ram[mem_pointers.cartridge->mirror_address(address - 0x2000)];
    else
        return palette[((address & 0x13) == 0x10 ? address & ~0x10 : address) & 0x1F];
}

void PPU::set_cpu_nmi(bool nmi) noexcept {mem_pointers.cpu->set_nmi(nmi);}

void PPU::render_pixel() noexcept
{
    static constexpr px32 rgb_table[] =
    { 0x7C7C7C, 0x0000FC, 0x0000BC, 0x4428BC, 0x940084, 0xA80020, 0xA81000, 0x881400,
      0x503000, 0x007800, 0x006800, 0x005800, 0x004058, 0x000000, 0x000000, 0x000000,
      0xBCBCBC, 0x0078F8, 0x0058F8, 0x6844FC, 0xD800CC, 0xE40058, 0xF83800, 0xE45C10,
      0xAC7C00, 0x00B800, 0x00A800, 0x00A844, 0x008888, 0x000000, 0x000000, 0x000000,
      0xF8F8F8, 0x3CBCFC, 0x6888FC, 0x9878F8, 0xF878F8, 0xF85898, 0xF87858, 0xFCA044,
      0xF8B800, 0xB8F818, 0x58D854, 0x58F898, 0x00E8D8, 0x787878, 0x000000, 0x000000,
      0xFCFCFC, 0xA4E4FC, 0xB8B8F8, 0xD8B8F8, 0xF8B8F8, 0xF8A4C0, 0xF0D0B0, 0xFCE0A8,
      0xF8D878, 0xD8F878, 0xB8F8B8, 0xB8F8D8, 0x00FCFC, 0xF8D8F8, 0x000000, 0x000000 };

    const auto x = clks - 1;
    u16 pal;

    if (!(mask & MASK_MASK_RENDERING_ENABLED)) pal = (~vaddr & 0x3F00) ? 0 : vaddr & 0x1F;
    else
    {
        unsigned       bg_pat;

        bool           spr_behind_bg, spr_is_s0;
        unsigned       spr_pal;
        const unsigned spr_pat = sprite_pixel(spr_pal, spr_behind_bg, spr_is_s0);

        if (!(mask & MASK_MASK_SHOW_BACKGROUND) || (!(mask & MASK_MASK_SHOW_BACKGROUND_LEFTMOST_8_PIXELS) && x < 8))
            bg_pat = 0;
        else {
            bg_pat = ((bg_shift_hi >> (15 - xfine) & 1) << 1) |
                      (bg_shift_lo >> (15 - xfine) & 1);
            if (spr_pat && bg_pat && spr_is_s0 && x != 255) stat |= MASK_STAT_SPRITE_ZERO_HIT;
        }
        if (spr_pat && !(spr_behind_bg && bg_pat)) pal = 0x10 + (spr_pal << 2) + spr_pat;
        else {
            if (!bg_pat) pal = 0;
            else
            {
                const unsigned attr = ((at_shift_hi >> (7 - xfine) & 1) << 1) |
                                       (at_shift_lo >> (7 - xfine) & 1);
                pal = (attr << 2) | bg_pat;
            }
        }
    }
    pixel_output[scanline * 256 + x] = rgb_table[memory_read(0x3F00 + pal)];
}

void PPU::sprite_operations() noexcept
{
    if (scanline < 240)
    {
        switch (clks)
        {
            case 256: scan_oam_addr = 0; sprite_loading(); break;
            case 340:                                      break;
            case   0:
                scan_oam_addr = oam_addr_overflow = scan_oam_addr_overflow = sprite_overflow_detection = 0;
                scan_oam[clks / 2] = 255;
            break;
            default:
                     if (clks <  64) scan_oam[clks / 2] = 255;
                else if (clks < 256)  sprite_evaluation();
                else if (clks < 320) {sprite_loading(); s0_curr_scanline = s0_next_scanline; oam_addr = 0;}
            break;
        }
    }
}

void PPU::sprite_evaluation() noexcept
{
    switch (clks & 1)
    {
        case 0: oam_tmp = oam[oam_addr]; break;
        case 1:
        {
            const bool in_range = (scanline - oam_tmp < (ctrl & CTRL_MASK_SPRITE_SIZE ? 16 : 8));
            if (clks == 65) s0_next_scanline = in_range;
            if (!scan_oam_addr_overflow && !oam_addr_overflow)
                scan_oam[scan_oam_addr] = oam_tmp;
            else 
                oam_tmp = scan_oam[scan_oam_addr];
            if (oam_copy > 0)
            {
                --oam_copy;
                if (!(++     oam_addr &= 0xFF))      oam_addr_overflow = true;
                if (!(++scan_oam_addr &= 0x1F)) scan_oam_addr_overflow = sprite_overflow_detection = true;
            }
            else if (in_range && !scan_oam_addr_overflow && !oam_addr_overflow)
            {
                oam_copy = 3;
                if (!(++     oam_addr &= 0xFF))      oam_addr_overflow = true;
                if (!(++scan_oam_addr &= 0x1F)) scan_oam_addr_overflow = sprite_overflow_detection = true;
            }
            else if (sprite_overflow_detection)
            {
                if (in_range && !oam_addr_overflow) {sprite_overflow = true; sprite_overflow_detection = false;}
                else 
                {
                    const u16 temp = ((oam_addr + 4) & ~3) | ((oam_addr + 1) & 3); oam_addr = temp & 255;
                    if (temp & 256) oam_addr_overflow = true;
                }
            }
            else
            {
                const u16 temp = oam_addr + 4; oam_addr          = temp & 0xFC;
                if       (temp & 256)          oam_addr_overflow = true;
            }
        break;
        }
    }
}

void PPU::sprite_loading() noexcept
{
    switch (const auto spr_index = (clks - 256) / 8; clks % 8)
    {
        case 0: sprite.y  = scan_oam[scan_oam_addr++]; scan_oam_addr &= 0x1F;              break;
        case 1: sprite.id = scan_oam[scan_oam_addr++]; scan_oam_addr &= 0x1F;              break;
        case 2: sprite.attr[spr_index] = scan_oam[scan_oam_addr++]; scan_oam_addr &= 0x1F; break;
        case 3: sprite.x   [spr_index] = scan_oam[scan_oam_addr++]; scan_oam_addr &= 0x1F; break;
        case 4: sprite.in_range = sprite_tile_address<0>(sprite.attr[spr_index]);          break;
        case 6: sprite.in_range = sprite_tile_address<1>(sprite.attr[spr_index]);          break;
        case 5: fetch_sprite_pattern<0>(spr_index);                                        break;
        case 7: fetch_sprite_pattern<1>(spr_index);                                        break;
    }
}

void PPU::background_misc() noexcept
{
    if (scanline >= 240 && scanline != 261) return;
    switch (clks)
    {
        case 340:                                                                   break;
        case   0: open_bus_addr = addr_nt();                                        break;
        case 254: shift_shifters(); open_bus_addr += 8;                 v_scroll(); break;
        case 255: shift_shifters(); bg_hi = memory_read(open_bus_addr);             break;
        case 256: shift_shifters(); reload_shift_regs();                h_update(); break;
        case 320: open_bus_addr = addr_nt();                                        break;
        case 338: open_bus_addr = addr_nt();                                        break;
        case 337: nt = memory_read(open_bus_addr);                                  break;
        case 339: nt = memory_read(open_bus_addr); if (scanline == 261 && odd_frame_post) ++clks;   break;
        default:
            if (scanline  == 261 && clks >= 279 && clks <= 303) v_update();
            else if (clks <= 254 || clks >= 321)
            {
                shift_shifters();
                switch (clks % 8)
                {
                    case 0: open_bus_addr = addr_nt(); reload_shift_regs();               break;
                    case 2: open_bus_addr = addr_at();                                    break;
                    case 4: open_bus_addr = addr_bg();                                    break;
                    case 6: open_bus_addr += 8;                                           break;
                    case 1: nt    = memory_read(open_bus_addr);                           break;
                    case 3: at    = memory_read(open_bus_addr); if (vaddr & 64) at >>= 4;
                                                                if (vaddr &  2) at >>= 2; break;
                    case 5: bg_lo = memory_read(open_bus_addr);                           break;
                    case 7: bg_hi = memory_read(open_bus_addr); h_scroll();               break;
                }
            }
        break;
    }
}

void PPU::tick() noexcept
{
    if (open_bus_decay_timer && !--open_bus_decay_timer) open_bus_data = 0;
    if (sprite_overflow) {sprite_overflow = false; stat |= MASK_STAT_SPRITE_OVERFLOW;}
    if (scanline < 240 && clks >= 1 && clks <= 256) render_pixel();
    switch (scanline)
    {
        case 241: if (clks == 0) {stat |= MASK_STAT_VBLANK; if (ctrl & CTRL_MASK_GENERATE_NMI) set_cpu_nmi(true);} break;
        case 261:
            switch (clks)
            {
                case 0: stat &= ~(MASK_STAT_SPRITE_OVERFLOW | MASK_STAT_SPRITE_ZERO_HIT); break;
                case 1: stat &= ~ MASK_STAT_VBLANK; s0_next_scanline = false;             break;
            }
        break;
        case 240: if (clks == 1) new_frame_post = true; break;
    }
    if (mask & MASK_MASK_RENDERING_ENABLED)
    {
        sprite_operations();
        background_misc();
    }
    if (write_addr_delay && !--write_addr_delay) {vaddr = tmp_vaddr;}
    if (++clks > 340)
    {
        if (++scanline == 262) {scanline = 0; odd_frame_post = !odd_frame_post;}
        clks -= 341;
    }
}

