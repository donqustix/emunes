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
    bg_shift_lo <<= 1;
    bg_shift_hi <<= 1;
    at_shift_lo = (at_shift_lo << 1) | at_latch_lo;
    at_shift_hi = (at_shift_hi << 1) | at_latch_hi;
}

void PPU::tick() noexcept
{
    if (!--open_bus_decay_timer) open_bus = 0;
    switch (scanline)
    {
        case 241: if (clks == 1) vblank         = true;  break;
        case 240: if (clks == 0) new_frame_post = true;  break;
        default:
            if (scanline == 261 && clks == 1) vblank = false;
            if ((mask & MASK_MASK_RENDERING_ENABLED) && (scanline < 240 || scanline == 261))
            {
                switch (clks)
                {
                    case   0:                                                                    break;
                    case   1: faddr = addr_nt();                                                 break;
                    case 256: render_pixel(); bg_hi = memory_read(faddr); v_scroll();            break;
                    case 257: render_pixel(); reload_shift_regs();        h_update();            break;
                    case 321:
                    case 339: faddr = addr_nt();                                                 break;
                    case 338: nt = memory_read(faddr);                                           break;
                    case 340: nt = memory_read(faddr); if (scanline == 261 && odd_frame) ++clks; break;
                    default:
                        if (scanline  == 261 && clks >= 280 && clks <= 304) v_update();
                        else if (clks <= 255 || clks >= 322)
                        {
                            render_pixel();
                            switch (clks % 8)
                            {
                                case 1: faddr = addr_nt(); reload_shift_regs();               break;
                                case 3: faddr = addr_at();                                    break;
                                case 5: faddr = addr_bg();                                    break;
                                case 7: faddr += 8;                                           break;
                                case 2: nt    = memory_read(faddr);                           break;
                                case 4: at    = memory_read(faddr); if (vaddr & 64) at >>= 4;
                                                                    if (vaddr &  2) at >>= 2; break;
                                case 6: bg_lo = memory_read(faddr);                           break;
                                case 0: bg_hi = memory_read(faddr); h_scroll();               break;
                            }
                        }
                    break;
                }
            }
        break;
    }

    if (++clks > 340)
    {
        if (++scanline == 262) {scanline = 0; odd_frame = !odd_frame;}
        clks -= 341;
    }
}

