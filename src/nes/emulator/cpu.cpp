#include "cpu.h"

#include "cartridge.h"
#include "ppu.h"
#include "apu.h"
#include "controller.h"

using namespace nes::emulator;

cpu_time_t CPU::earliest_irq_before(cpu_time_t end_time) noexcept
{
    if (!(P & MI))
    {
        const cpu_time_t irq_time = mem_pointers.apu->earliest_irq();
        if (irq_time < end_time)
            end_time = irq_time;
    }
    return end_time;
}

void CPU::sync_hardware() noexcept
{
    for (int i = 0; i < 3; ++i) mem_pointers.ppu->tick();
    ++cpu_time;
}

void CPU::wb(u16 address, u8 value) noexcept
{
    sync_hardware();
    if      (address < 0x0800) internal_ram[address] = value; // 2KB internal RAM
    else if (address < 0x2000) internal_ram[address - 0x0800] = value; // mirrors of $0000 - $800
    else if (address < 0x4000)
    {
        // $2000 - $2008 - ppu registers
        // $2008 - $4000 - mirrors of $2000-$2007 (repeats every 8 bytes)
        switch (address % 8)
        {
            case 0: mem_pointers.ppu->reg_write<0>(value);  break;
            case 1: mem_pointers.ppu->reg_write<1>(value);  break;
            case 2: mem_pointers.ppu->reg_write<2>(value);  break;
            case 3: mem_pointers.ppu->reg_write<3>(value);  break;
            case 4: mem_pointers.ppu->reg_write<4>(value);  break;
            case 5: mem_pointers.ppu->reg_write<5>(value);  break; // x2
            case 6: mem_pointers.ppu->reg_write<6>(value);  break; // x2
            case 7: mem_pointers.ppu->reg_write<7>(value);  break;
        }
    }
    else if (address < 0x4018) // apu and I/O registers
    {
        switch (address)
        {
            case 0x4014: oam_dma(value);                                             break;
            case 0x4016: mem_pointers.controller->write(value);                      break;
            default:     mem_pointers.apu->write_register(cpu_time, address, value); break;
        }
    }
    else if (address < 0x4020); // normally disabled
    else if (address < 0x6000); // cartridge space
    else if (address < 0x8000) mem_pointers.cartridge->write_ram(address - 0x6000, value);
    else mem_pointers.cartridge->write_mapper(address, value);
}

u8 CPU::rb(u16 address) noexcept
{
    sync_hardware();
    if      (address < 0x0800) return internal_ram[address]; // 2KB internal RAM
    else if (address < 0x2000) return internal_ram[address - 0x0800]; // mirror of $0-$800
    else if (address < 0x4000)
        // $2000 - $2008 - ppu registers
        // $2008 - $4000 - mirrors of $2000-$2007 (repeats every 8 bytes)
        switch (address % 8)
        {
            case  0: return mem_pointers.ppu->reg_read<0>();
            case  1: return mem_pointers.ppu->reg_read<1>();
            case  2: return mem_pointers.ppu->reg_read<2>();
            case  3: return mem_pointers.ppu->reg_read<3>();
            case  4: return mem_pointers.ppu->reg_read<4>();
            case  5: return mem_pointers.ppu->reg_read<5>();
            case  6: return mem_pointers.ppu->reg_read<6>();
            case  7: return mem_pointers.ppu->reg_read<7>();
        }
    else if (address < 0x4018) // apu and I/O registers
    {
        switch (address)
        {
            case 0x4015: return mem_pointers.apu->read_status(cpu_time);
            case 0x4016: return mem_pointers.controller->read<0>();
            case 0x4017: return mem_pointers.controller->read<1>();
        }
    }
    else if (address < 0x4020) return 0; // normally disabled
    else if (address < 0x6000) return 0;
    else if (address < 0x8000) return mem_pointers.cartridge->read_ram(address - 0x6000);
    else return mem_pointers.cartridge->read_rom(address - 0x8000);

    return 0;
}

void CPU::oam_dma(u8 value) noexcept
{
    rb(value << 8);
    if (mem_pointers.ppu->odd_frame()) sync_hardware();
    for (u16 i = 0; i < 256; ++i)
        wb(0x2004, rb((value << 8) | i));
}

void CPU::instruction() noexcept
{
    /*opcode fetch*/u16 op = rb(PC++); PC &= 0xFFFF;

    switch (const u16 i = pending_interrupt; i)
    {
        case RST: op = 0x100; break;
        case NMI: op = 0x101; break;
    }

    switch (op)
    {
        case 0xA5: lda(zp()); break;
        case 0xA6: ldx(zp()); break;
        case 0xA4: ldy(zp()); break;
        case 0x04: nop(zp()); break;
        case 0x44: nop(zp()); break;
        case 0x64: nop(zp()); break;
        case 0x05: ora(zp()); break;
        case 0x65: adc<0>(zp()); break;
        case 0xE5: adc<1>(zp()); break;
        case 0x25: AND(zp()); break;
        case 0x24: bit(zp()); break;
        case 0xC5: cmp(zp()); break;
        case 0xE4: cpx(zp()); break;
        case 0xC4: cpy(zp()); break;
        case 0x45: eor(zp()); break;
        case 0x85: sta(zp()); break;
        case 0x86: stx(zp()); break;
        case 0x84: sty(zp()); break;
        case 0x46: lsr(zp()); break;
        case 0x26: rol(zp()); break;
        case 0x66: ror(zp()); break;
        case 0x06: asl(zp()); break;
        case 0xC6: dec(zp()); break;
        case 0xE6: inc(zp()); break;

        case 0xB5: lda(zpx()); break;
        case 0xB6: ldx(zpy()); break;
        case 0xB4: ldy(zpx()); break;

        case 0x14: nop(zpx()); break;
        case 0x34: nop(zpx()); break;
        case 0x54: nop(zpx()); break;
        case 0x74: nop(zpx()); break;
        case 0xD4: nop(zpx()); break;
        case 0xF4: nop(zpx()); break;
        case 0x15: ora(zpx()); break;
        case 0x75: adc<0>(zpx()); break;
        case 0xF5: adc<1>(zpx()); break;
        case 0x35: AND(zpx()); break;
        case 0xD5: cmp(zpx()); break;
        case 0x55: eor(zpx()); break;
        case 0x95: sta(zpx()); break;
        case 0x96: stx(zpy()); break;
        case 0x94: sty(zpx()); break;

        case 0x56: lsr(zpx()); break;
        case 0x36: rol(zpx()); break;
        case 0x76: ror(zpx()); break;
        case 0x16: asl(zpx()); break;
        case 0xD6: dec(zpx()); break;
        case 0xF6: inc(zpx()); break;

        case 0xAD: lda(abs()); break;
        case 0xAE: ldx(abs()); break;
        case 0xAC: ldy(abs()); break;
        case 0x0C: nop(abs()); break;
        case 0x0D: ora(abs()); break;
        case 0x6D: adc<0>(abs()); break;
        case 0xED: adc<1>(abs()); break;
        case 0x2D: AND(abs()); break;
        case 0x2C: bit(abs()); break;
        case 0xCD: cmp(abs()); break;
        case 0xEC: cpx(abs()); break;
        case 0xCC: cpy(abs()); break;
        case 0x4D: eor(abs()); break;
        case 0x8D: sta(abs()); break;
        case 0x8E: stx(abs()); break;
        case 0x8C: sty(abs()); break;

        case 0x4E: lsr(abs()); break;
        case 0x2E: rol(abs()); break;
        case 0x6E: ror(abs()); break;
        case 0x0E: asl(abs()); break;
        case 0xCE: dec(abs()); break;
        case 0xEE: inc(abs()); break;

        case 0xBD: lda(abx()); break;
        case 0xB9: lda(aby()); break;
        case 0xBE: ldx(aby()); break;
        case 0xBC: ldy(abx()); break;
        case 0x1C: nop(abx()); break;
        case 0x3C: nop(abx()); break;
        case 0x5C: nop(abx()); break;
        case 0x7C: nop(abx()); break;
        case 0xDC: nop(abx()); break;
        case 0xFC: nop(abx()); break;
        case 0x1D: ora(abx()); break;
        case 0x19: ora(aby()); break;
        case 0x7D: adc<0>(abx()); break;
        case 0x79: adc<0>(aby()); break;
        case 0xFD: adc<1>(abx()); break;
        case 0xF9: adc<1>(aby()); break;
        case 0x3D: AND(abx()); break;
        case 0x39: AND(aby()); break;
        case 0xDD: cmp(abx()); break;
        case 0xD9: cmp(aby()); break;
        case 0x5D: eor(abx()); break;
        case 0x59: eor(aby()); break;

        case 0x9D: sta(abx_big()); break;
        case 0x99: sta(aby_big()); break;

        case 0x5E: lsr(abx_big()); break;
        case 0x3E: rol(abx_big()); break;
        case 0x7E: ror(abx_big()); break;
        case 0x1E: asl(abx_big()); break;
        case 0xDE: dec(abx_big()); break;
        case 0xFE: inc(abx_big()); break;

        case 0xA1: lda(izx()); break;
        case 0x01: ora(izx()); break;
        case 0x61: adc<0>(izx()); break;
        case 0xE1: adc<1>(izx()); break;
        case 0x21: AND(izx()); break;
        case 0xC1: cmp(izx()); break;
        case 0x41: eor(izx()); break;
        case 0x81: sta(izx()); break;

        case 0xB1: lda(izy()); break;
        case 0x11: ora(izy()); break;
        case 0x71: adc<0>(izy()); break;
        case 0xF1: adc<1>(izy()); break;
        case 0x31: AND(izy()); break;
        case 0xD1: cmp(izy()); break;
        case 0x51: eor(izy()); break;

        case 0x91: sta(izy_big()); break;

        case 0xA9: lda(imm()); break;
        case 0xA2: ldx(imm()); break;
        case 0xA0: ldy(imm()); break;
        case 0x80: nop(imm()); break;
        case 0x82: nop(imm()); break;
        case 0xC2: nop(imm()); break;
        case 0xE2: nop(imm()); break;
        case 0x89: nop(imm()); break;
        case 0x09: ora(imm()); break;
        case 0x69: adc<0>(imm()); break;
        case 0xE9: adc<1>(imm()); break;
        case 0xEB: adc<1>(imm()); break;
        case 0x29: AND(imm()); break;
        case 0xC9: cmp(imm()); break;
        case 0xE0: cpx(imm()); break;
        case 0xC0: cpy(imm()); break;
        case 0x49: eor(imm()); break;

        case 0x4A: lsr_a(); break;
        case 0x2A: rol_a(); break;
        case 0x6A: ror_a(); break;
        case 0x0A: asl_a(); break;

        case 0x18: clc(); break;
        case 0xD8: cld(); break;
        case 0xB8: clv(); break;
        case 0x58: cli(); break;

        case 0x78: sei(); break;
        case 0x38: sec(); break;
        case 0xF8: sed(); break;
        case 0xCA: dex(); break;
        case 0x88: dey(); break;
        case 0xE8: inx(); break;
        case 0xC8: iny(); break;
        case 0xAA: tax(); break;
        case 0xA8: tay(); break;
        case 0x98: tya(); break;
        case 0xBA: tsx(); break;
        case 0x9A: txs(); break;
        case 0x8A: txa(); break;

        case 0x1A: nop(PC); break;
        case 0x3A: nop(PC); break;
        case 0x5A: nop(PC); break;
        case 0x7A: nop(PC); break;
        case 0xDA: nop(PC); break;
        case 0xEA: nop(PC); break;
        case 0xFA: nop(PC); break;

        case 0x48: pha(); break;
        case 0x08: php(); break;
        case 0x68: pla(); break;
        case 0x28: plp(); break;
        case 0x40: rti(); break;
        case 0x60: rts(); break;

        case 0xB0: bcs(); break;
        case 0x90: bcc(); break;
        case 0xF0: beq(); break;
        case 0xD0: bne(); break;
        case 0x30: bmi(); break;
        case 0x10: bpl(); break;
        case 0x70: bvs(); break;
        case 0x50: bvc(); break;

        case 0x4C: jmp_abs(); break;
        case 0x6C: jmp_ind(); break;
        case 0x20:     jsr(); break;

        case 0x000: INT<BRK>(); break;
        case 0x100: INT<RST>(); break;
        case 0x101: INT<NMI>(); break;
    }
}

