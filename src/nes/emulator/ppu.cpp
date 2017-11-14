#include "ppu.h"

#include "cartridge.h"

using namespace nes::emulator;

void PPU::memory_write(u16 address, u8 value) noexcept
{
    if      (address < 0x2000) mem_pointers.cartridge->write_video_memory(address, value);
    else if (address < 0x3F00) ram[address - 0x2000] = value;
    else if (address < 0x4000)
    {
        if ((address & 0x13) == 0x10) address &= ~0x10;
        palette[address & 0x1F] = value;
    }
}

u8 PPU::memory_read(u16 address) const noexcept
{
    if      (address < 0x2000) return mem_pointers.cartridge->read_video_memory(address);
    else if (address < 0x3F00) return ram[address - 0x2000];
    else
    {
        if ((address & 0x13) == 0x10) address &= ~0x10;
        return palette[address & 0x1F];
    }
}

void PPU::render_pixel() noexcept
{
    static constexpr unsigned nesRgb[] =
    { 0x7C7C7C, 0x0000FC, 0x0000BC, 0x4428BC, 0x940084, 0xA80020, 0xA81000, 0x881400,
      0x503000, 0x007800, 0x006800, 0x005800, 0x004058, 0x000000, 0x000000, 0x000000,
      0xBCBCBC, 0x0078F8, 0x0058F8, 0x6844FC, 0xD800CC, 0xE40058, 0xF83800, 0xE45C10,
      0xAC7C00, 0x00B800, 0x00A800, 0x00A844, 0x008888, 0x000000, 0x000000, 0x000000,
      0xF8F8F8, 0x3CBCFC, 0x6888FC, 0x9878F8, 0xF878F8, 0xF85898, 0xF87858, 0xFCA044,
      0xF8B800, 0xB8F818, 0x58D854, 0x58F898, 0x00E8D8, 0x787878, 0x000000, 0x000000,
      0xFCFCFC, 0xA4E4FC, 0xB8B8F8, 0xD8B8F8, 0xF8B8F8, 0xF8A4C0, 0xF0D0B0, 0xFCE0A8,
      0xF8D878, 0xD8F878, 0xB8F8B8, 0xB8F8D8, 0x00FCFC, 0xF8D8F8, 0x000000, 0x000000 };

    const int x = clks - 2;
    if (scanline < 240 && x >= 0 && x < 256)
    {
        if (!(mask & MASK_MASK_RENDERING_ENABLED)) {framebuffer[scanline * 256 + x] = 0xFFFFFF; return;}
        if (   (mask & MASK_MASK_SHOW_BACKGROUND) &&
            !(!(mask & MASK_MASK_SHOW_BACKGROUND_LEFTMOST_8_PIXELS) && x < 8))
        {
            auto pal = (((bg_shift_hi >> (15 - xfine)) & 1) << 1) |
                        ((bg_shift_lo >> (15 - xfine)) & 1);
            if (pal)
                pal |= (((at_shift_hi >> (7 - xfine)) & 1)  << 1 |
                        ((at_shift_lo >> (7 - xfine)) & 1)) << 2;
            framebuffer[scanline * 256 + x] = nesRgb[memory_read(0x3F00 + pal)];
        }
    }
}

void PPU::sprite_processing() noexcept
{
    if (clks >= 257 && clks <= 320) oam_addr = 0;
    if (scanline < 240)
    {
        if (clks ==  0)
        {
            oam_addr_overflow = scan_oam_addr_overflow = sprite_overflow_detection = false;
            scan_oam_addr = 0;
        }
        else if (clks <  65) scan_oam[(clks - 1) / 2] = 255;
        else if (clks < 257) sprite_evaluation();
        else if (clks < 321) sprite_fetches();
    }
}

void PPU::sprite_evaluation() noexcept
{
    if (clks & 1) oam_data = oam[oam_addr];
    else
    {
        const bool in_range = scanline - oam_data < (ctrl & CTRL_MASK_SPRITE_SIZE ? 16 : 8);
        if (oam_addr_overflow || scan_oam_addr_overflow)
            oam_data = scan_oam[scan_oam_addr];
        else
            scan_oam[scan_oam_addr] = oam_data;
        if (oam_copy)
        {
            --oam_copy;
            increment_oam_addrs();
        }
        else
        {
            if (in_range && !(oam_addr_overflow || scan_oam_addr_overflow))
            {
                oam_copy = 3;
                increment_oam_addrs();
            }
            else
            {
                if (!sprite_overflow_detection)
                {
                    oam_addr += 4; oam_addr &= 0xFC;
                    if (!oam_addr)
                        oam_addr_overflow = true;
                }
                else
                {
                    if (in_range && !scan_oam_addr_overflow)
                    {
                        stat |= 0x40;
                        sprite_overflow_detection = false;
                    }
                    else
                    {
                        oam_addr = ((oam_addr + 4) & 0xFC) | ((oam_addr + 1) & 3);
                        if ((oam_addr & 0xFC) == 0)
                            oam_addr_overflow = true;
                    }
                }
            }
        }
    }
}

void PPU::sprite_fetches() noexcept
{
    switch (clks % 8)
    {
        case 1: latch_sprite_y  = scan_oam[scan_oam_addr++]; scan_oam_addr &= 0x1F; break;
        case 2: latch_sprite_id = scan_oam[scan_oam_addr++]; scan_oam_addr &= 0x1F; break;
        case 3: latch_sprite_attr[(clks - 257) / 8] = scan_oam[scan_oam_addr++]; scan_oam_addr &= 0x1F; break;
        case 4: latch_sprite_x   [(clks - 257) / 8] = scan_oam[scan_oam_addr++]; scan_oam_addr &= 0x1F; break;
        case 5: break;
        case 6: break;
        case 7: break;
        case 0: break;
    }
}

void PPU::background_processing() noexcept
{
    if (scanline < 240 || scanline == 261)
    {
        background_fetches();
        scroll_evaluation();
    }
}

void PPU::background_fetches() noexcept
{
    if (scanline < 240 || scanline == 261)
    {
        switch (clks)
        {
            case   0:                                     break;
            case   1: open_bus_addr = addr_nt();          break;
            case 256: bg_hi = memory_read(open_bus_addr); break;
            case 321:
            case 339: open_bus_addr = addr_nt();          break;
            case 338: nt = memory_read(open_bus_addr);    break;
            case 340: nt = memory_read(open_bus_addr);    break;
            default:
                if (clks <= 255 || clks >= 322 || (clks & ~3))
                {
                    switch (clks % 8)
                    {
                        case 1: open_bus_addr = addr_nt(); break;
                        case 3: open_bus_addr = addr_at(); break;
                        case 5: open_bus_addr = addr_bg(); break;
                        case 7: open_bus_addr += 8;        break;
                        case 2: nt    = memory_read(open_bus_addr);                           break;
                        case 4: at    = memory_read(open_bus_addr); if (vaddr & 64) at >>= 4;
                                                                    if (vaddr &  2) at >>= 2; break;
                        case 6: bg_lo = memory_read(open_bus_addr); break;
                        case 0: bg_hi = memory_read(open_bus_addr); break;
                    }
                }
            break;
        }
    }
}

void PPU::scroll_evaluation() noexcept
{
    switch (clks)
    {
        case   0: break;
        case   1: break;
        case 256: shift_shifters();                      v_scroll(); break;
        case 257: shift_shifters(); reload_shift_regs(); h_update(); break;
        case 338: break;
        case 339: break;
        case 340: break;
        default:
            if (scanline  == 261 && clks >= 280 && clks <= 304) v_update();
            else if (clks <= 255 || clks >= 322)
            {
                shift_shifters();
                switch (clks % 8)
                {
                    case 1: reload_shift_regs(); break;
                    case 0: h_scroll();          break;
                }
            }
        break;
    }
}

void PPU::tick() noexcept
{
    if (!--open_bus_decay_timer) open_bus_data = 0;
    switch (scanline)
    {
        case 241: if (clks == 1) stat |=  0x80;          break;
        case 261: if (clks == 1) stat &= ~0x80;          break;
        case 240: if (clks == 0) new_frame_post = true;  break;
    }
    if (rendering_enabled())
    {
        render_pixel();
        background_processing();
        sprite_processing();
    }
    next_cycle();
}

