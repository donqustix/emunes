#include "cpu.h"

#include "cartridge.h"
#include "ppu.h"

using namespace nes::emulator;

void CPU::sync_hardware() noexcept
{
    PPU* const ppu = mem_pointers.ppu;
    for (int i = 0; i < 3; ++i)
    {
        ppu->tick();set_nmi(ppu->nmi());
    };
}

void CPU::wb(u16 address, u8 value) noexcept
{
    sync_hardware();
    if      (address < 0x0800) mem_pointers.internal_ram[address] = value; // 2KB internal RAM
    else if (address < 0x2000) mem_pointers.internal_ram[address - 0x0800] = value; // mirrors of $0000 - $800
    else if (address < 0x4000)
    {
        // $2000 - $2008 - ppu registers
        // $2008 - $4000 - mirrors of $2000-$2007 (repeats every 8 bytes)
        switch (address % 8)
        {
            case 0: mem_pointers.ppu->write_ctrl(value);        break;
            case 1: mem_pointers.ppu->write_mask(value);        break;
           // case 3: mem_pointers.ppu->oamaddr = value;     break;
           // case 4: mem_pointers.ppu->oamdata = value;     break;
            case 5: mem_pointers.ppu->write_scroll(value);      break; // x2
            case 6: mem_pointers.ppu->write_addr(value);        break; // x2
            case 7: mem_pointers.ppu->write_data(value);        break;
        }
    }
    else if (address < 0x4018); // apu and I/O registers
    else if (address < 0x4020); // normally disabled
    else if (address < 0x6000); // cartridge space
    else if (address < 0x8000) mem_pointers.cartridge->write_ram(address - 0x6000, value);
}

u8 CPU::rb(u16 address) noexcept
{
    sync_hardware();
    if      (address < 0x0800) return mem_pointers.internal_ram[address]; // 2KB internal RAM
    else if (address < 0x2000) return mem_pointers.internal_ram[address - 0x0800]; // mirror of $0-$800
    else if (address < 0x4000)
        // $2000 - $2008 - ppu registers
        // $2008 - $4000 - mirrors of $2000-$2007 (repeats every 8 bytes)
        switch (address % 8)
        {
            case  2: return mem_pointers.ppu->read_status();
            //case  4: return mem_pointers.ppu->oamdata;
            case  7: return mem_pointers.ppu->read_dada();
            default: return 0;
        }
    else if (address < 0x4018) return 0; // apu and I/O registers
    else if (address < 0x4020) return 0; // normally disabled
    else if (address < 0x8000) return 0;
    else if (address < 0xC000) return mem_pointers.cartridge->read_rom(address - 0x8000);
    else return mem_pointers.cartridge->read_rom(address - 0xC000); // cartridge space
}

void CPU::tick() noexcept
{
    u16 op = rb(PC++); PC &= 0xFFFF;

         if (reset)                     {op = 0x100; reset = false;           }
    else if (nmi && !nmi_edge_detected) {op = 0x101; nmi_edge_detected = true;}
    else if (irq && !(P & MI))          {op = 0x102;                          }
    if (!nmi) nmi_edge_detected = false;

    switch (op)
    {
#define I(op) case 0x##op:instruction<0x##op>(); break;
        // NOP
        I(EA)I(04)I(14)I(1A)I(0C)I(1C)I(34)I(3A)I(3C)I(44)I(54)I(64)I(74)I(80)
        I(C2)I(E2)I(D4)I(F4)I(FA)I(DA)I(FC)I(DC)I(89)I(7A)I(7C)I(5A)I(5C)I(82)
        // LDA
        I(A9)I(A5)I(B5)I(AD)I(B9)I(BD)I(A1)I(B1)
        // LDX
        I(A2)I(A6)I(B6)I(AE)I(BE)
        // LDY
        I(A0)I(A4)I(B4)I(AC)I(BC)
        // LAX
       // I(A7)I(B7)I(AF)I(BF)I(A3)I(B3)
        // SAX
       // I(87)I(97)I(83)I(8F)
        // LSR
        I(4A)I(46)I(56)I(4E)I(5E)
        // ORA
        I(09)I(05)I(15)I(0D)I(1D)I(19)I(01)I(11)
        // PHA, PHP
        I(48)I(08)
        // PLA, PLP
        I(68)I(28)
        // ROL
        I(2A)I(26)I(36)I(2E)I(3E)
        // ROR
        I(6A)I(66)I(76)I(6E)I(7E)
        /*RTI*/I(40)/*RTS*/I(60)
        // ADC
        I(69)I(65)I(75)I(6D)I(7D)I(79)I(61)I(71)
        // SBC
        I(E9)I(EB)I(E5)I(F5)I(ED)I(FD)I(F9)I(E1)I(F1)
        // AND
        I(29)I(25)I(35)I(2D)I(3D)I(39)I(21)I(31)
        // ASL
        I(0A)I(06)I(16)I(0E)I(1E)
        // branches
        I(90)I(B0)I(F0)I(D0)I(30)I(10)I(70)I(50)
        /*BIT*/I(24)I(2C)
        /*BRK*/I(00)
        /*CLC*/I(18)
        /*CLD*/I(D8)
        /*CLV*/I(B8)
        /*CLI*/I(58)
        /*SEI*/I(78)
        /*SEC*/I(38)
        /*SED*/I(F8)
        // CMP
        I(C9)I(C5)I(D5)I(CD)I(DD)I(D9)I(C1)I(D1)
        // CPX
        I(E0)I(E4)I(EC)
        // CPY
        I(C0)I(C4)I(CC)
        // DEC
        I(C6)I(D6)I(CE)I(DE)
        /*DEX*/I(CA)/*DEY*/I(88)
        // INC
        I(E6)I(F6)I(EE)I(FE)
        /*INX*/I(E8)/*INY*/I(C8)
        // EOR
        I(49)I(45)I(55)I(4D)I(5D)I(59)I(41)I(51)
        /*JMP*/I(4C)I(6C)
        /*JSR*/I(20)
        // STA
        I(85)I(95)I(8D)I(9D)I(99)I(81)I(91)
        // STX
        I(86)I(96)I(8E)
        // STY
        I(84)I(94)I(8C)
        // DCP
       // I(C7)I(D7)I(C3)I(D3)I(CF)I(DF)I(DB)
        // ISB
        //I(E7)I(F7)I(E3)I(F3)I(EF)I(FF)I(FB)
        /*TAX*/I(AA)/*TAY*/I(A8)/*TYA*/I(98)/*TSX*/I(BA)/*TXS*/I(9A)/*TXA*/I(8A)
        /*interrupts*/I(100)I(101)I(102)
#undef I
    }
}

