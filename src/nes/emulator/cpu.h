#ifndef CPU_H
#define CPU_H

#include "int_alias.h"

namespace nes::emulator
{
    class PPU;
    class Cartridge;

    class CPU final
    {
    public:
        struct MemPointers
        {
            unsigned char*  internal_ram;
            PPU*            ppu;
            Cartridge*      cartridge;
        };

    private:
        enum InterruptType {NMI, RST, IRQ, BRK};
        enum FlagMasks : u8 {
            MC = 0b00000001,
            MZ = 0b00000010,
            MI = 0b00000100,
            MD = 0b00001000,
            MV = 0b01000000,
            MN = 0b10000000
        };
        enum FlagShifts : u8 {
            SC = 0, SZ, SI, SD, SB, SV = SB + 2, SN
        };

        MemPointers mem_pointers;

        u8   A = 0, X = 0, Y = 0, P = 0, S = 0;
        u16 PC = 0;

        bool nmi = false, nmi_edge_detected = false, irq = false, reset = true;

        void sync_hardware() noexcept;

        void wb(u16 address, u8 value) noexcept;
        u8   rb(u16 address) noexcept;

        void push(u8 v) noexcept {wb(0x100 | S--, v); S &= 255;}
        u8 pop() noexcept {++S &= 255; return rb(0x100 | S);}

        void set_flags(u8 flags, u8 mask) noexcept {P = (P & ~mask) | flags;}
        u8 flags(u8 mask) const noexcept {return P & mask;}

        void up_flag_nz(u8 t) noexcept {P &= ~(MZ | MN); P |= (!t << SZ) | (t & MN);}
        void up_flag_cv(u8 a, u8 b, u16 t) noexcept
        {
            P &= ~MV; P |= ((a  ^ t) & (b ^ t)) >> 1 & MV;
            P &= ~MC; P |=                  t   >> 8 & MC;
        }

        u16  zp() noexcept {const u16 a = rb(PC++); PC &= 0xFFFF; return a;}
        u16 zpx() noexcept {const u16 a = zp(); rb(a); return (a + X) & 255;}
        u16 zpy() noexcept {const u16 a = zp(); rb(a); return (a + Y) & 255;}
        u16 abs() noexcept {u16 a = rb(PC++); PC &= 0xFFFF; a |= rb(PC++) << 8; PC &= 0xFFFF; return a;}
        u16 abx() noexcept {const u16 t = abs(); u16 a = t + X; if ((a ^ t) & 256) {rb(a - 256); a &= 0xFFFF;} return a;}
        u16 aby() noexcept {const u16 t = abs(); u16 a = t + Y; if ((a ^ t) & 256) {rb(a - 256); a &= 0xFFFF;} return a;}
        u16 izx() noexcept {const u16 t = zpx(); return rb(t) |  rb((t + 1) & 255) << 8;}
        u16 izy() noexcept {u16 t = zp(), a  = rb(t++);      t &= 255;  
                                          a |= rb(t  ) << 8; a +=   Y;
                            if ((a ^ (a - Y)) & 256) {rb(a - 256); a &= 0xFFFF;} return a;}
        u16 abx_big() noexcept {const u16 t = abs(), a = t + X; rb((a ^ t) & 256 ? a - 256 : a); return a & 0xFFFF;}
        u16 aby_big() noexcept {const u16 t = abs(), a = t + Y; rb((a ^ t) & 256 ? a - 256 : a); return a & 0xFFFF;}
        u16 izy_big() noexcept {u16 t = zp(), a  = rb(t++);      t &= 255;
                                              a |= rb(t  ) << 8; a +=   Y; rb((a ^ t) & 256 ? a - 256 : a); return a & 0xFFFF;}
        u16 imm() noexcept {const u16 t = PC++; PC &= 0xFFFF; return t;}

        void lda(u16 a) noexcept {A = rb(a); up_flag_nz(A);}
        void ldx(u16 a) noexcept {X = rb(a); up_flag_nz(X);}
        void ldy(u16 a) noexcept {Y = rb(a); up_flag_nz(Y);}

        void sta(u16 a) noexcept {wb(a, A);}
        void stx(u16 a) noexcept {wb(a, X);}
        void sty(u16 a) noexcept {wb(a, Y);}

        void AND(u16 a) noexcept {up_flag_nz(A &= rb(a));}
        void ora(u16 a) noexcept {up_flag_nz(A |= rb(a));}
        void eor(u16 a) noexcept {up_flag_nz(A ^= rb(a));}

        void nop(u16 a) noexcept {rb(a);}

        void lsr(u16 a) noexcept
        {
            u8 t = rb(a); wb(a, t      ); P &= ~MC; P |= t & MC; 
                          wb(a, t >>= 1);
            up_flag_nz(t);
        }

        void asl(u16 a) noexcept
        {
            u8 t = rb(a); wb(a,  t                 ); P &= ~MC; P |= t >> 7; 
                          wb(a, (t <<= 1, t &= 255));
            up_flag_nz(t);
        }

        void rol(u16 a) noexcept
        {
                  u8 t =  rb(a);
            const u8 c = t >> 7; wb(a,  t                                 );
                                 wb(a, (t <<= 1, t &= 255, t |= flags(MC)));
            up_flag_nz(t); P &= ~MC; P |= c;
        }

        void ror(u16 a) noexcept
        {
                  u8 t =  rb(a);
            const u8 c = t & MC; wb(a,  t                            );
                                 wb(a, (t >>= 1, t |= flags(MC) << 7));
            up_flag_nz(t); P &= ~MC; P |= c;
        }

        void bit(u16 a) noexcept
        {
            const u8 d = rb(a); P &= ~MZ; P |= !(A &  d) << SZ;
                                P &= ~MV; P |=   d & MV;
                                P &= ~MN; P |=   d & MN;
        }

        void cmp(u16 a) noexcept {const u8 d = rb(a); up_flag_nz((A - d) & 255); P &= ~MC; P |= A >= d;}
        void cpx(u16 a) noexcept {const u8 d = rb(a); up_flag_nz((X - d) & 255); P &= ~MC; P |= X >= d;}
        void cpy(u16 a) noexcept {const u8 d = rb(a); up_flag_nz((Y - d) & 255); P &= ~MC; P |= Y >= d;}

        void dec(u16 a) noexcept {u8 t = rb(a); wb(a, t--); wb(a, t &= 255); up_flag_nz(t);}
        void inc(u16 a) noexcept {u8 t = rb(a); wb(a, t++); wb(a, t &= 255); up_flag_nz(t);}

        void adc(u8 d) noexcept
        {
            const u16 t = A + d + flags(MC); up_flag_cv(A, d, t      );
                                             up_flag_nz(A   = t & 255);
        }

        void lsr_a() noexcept {rb(PC); P &= ~MC; P |= A & MC; up_flag_nz( A >>= 1           );}
        void asl_a() noexcept {rb(PC); P &= ~MC; P |= A >> 7; up_flag_nz((A <<= 1, A &= 255));}

        void ror_a() noexcept
        {
            rb(PC);     const u8 c = A & MC;
            up_flag_nz((A >>= 1, A |= flags(MC) << 7));
            P &= ~MC; P |= c;
        }
        void rol_a() noexcept
        {
            rb(PC);     const u8 c = A >> 7;
            up_flag_nz((A <<= 1, A &= 255, A |= flags(MC)));
            P &= ~MC; P |= c;
        }

        void clc() noexcept {rb(PC); P &= ~MC;}
        void cld() noexcept {rb(PC); P &= ~MD;}
        void clv() noexcept {rb(PC); P &= ~MV;}
        void cli() noexcept {rb(PC); P &= ~MI;}

        void sei() noexcept {rb(PC); P |= MI;}
        void sec() noexcept {rb(PC); P |= MC;}
        void sed() noexcept {rb(PC); P |= MD;}

        void dex() noexcept {rb(PC); up_flag_nz(--X &= 255);}
        void dey() noexcept {rb(PC); up_flag_nz(--Y &= 255);}

        void inx() noexcept {rb(PC); up_flag_nz(++X &= 255);}
        void iny() noexcept {rb(PC); up_flag_nz(++Y &= 255);}

        void tax() noexcept {rb(PC); X = A; up_flag_nz(X);}
        void tay() noexcept {rb(PC); Y = A; up_flag_nz(Y);}
        void tya() noexcept {rb(PC); A = Y; up_flag_nz(A);}
        void tsx() noexcept {rb(PC); X = S; up_flag_nz(X);}
        void txs() noexcept {rb(PC); S = X;}
        void txa() noexcept {rb(PC); A = X; up_flag_nz(A);}

        void pha() noexcept {rb(PC); push(A);}
        void php() noexcept {rb(PC); push(P | 0x30);}

        void pla() noexcept {rb(PC); sync_hardware(); up_flag_nz(A = pop());}
        void plp() noexcept {rb(PC); sync_hardware(); P = pop() & 0xEF;}

        void rti() noexcept {plp(); PC = pop() | pop() << 8;}
        void rts() noexcept {rb(PC); sync_hardware(); PC = pop() | pop() << 8; sync_hardware(); ++PC &= 0xFFFF;}

        void rel(bool cond) noexcept
        {
            const u8 j = rb(PC++); PC &= 0xFFFF;
            if (cond)
            {
                rb(PC); PC += j; PC &= 0xFFFF;
                if (j & 0x80)
                {
                    rb(PC); PC -= 256;
                }
            }
        }

        void bcs() noexcept {rel( flags(MC));}
        void bcc() noexcept {rel(!flags(MC));}
        void beq() noexcept {rel( flags(MZ));}
        void bne() noexcept {rel(!flags(MZ));}
        void bmi() noexcept {rel( flags(MN));}
        void bpl() noexcept {rel(!flags(MN));}
        void bvs() noexcept {rel( flags(MV));}
        void bvc() noexcept {rel(!flags(MV));}

        void jmp_ind() noexcept {const u16 t = abs(); PC = rb(t) | rb((t & 0xFF00) | ((t + 1) & 0xFF)) << 8;}
        void jmp_abs() noexcept {PC = abs();}

        void jsr() noexcept
        {
            const u8 t = rb(PC++); PC &= 0xFFFF;
            sync_hardware();
            push(PC >>  8);
            push(PC & 255);
            PC = t | rb(PC) << 8;
        }

        template<InterruptType type>
        void INT() noexcept
        {
            if constexpr (type == BRK) {rb(PC++); PC &= 0xFFFF;} else rb(--PC);
            if constexpr (type == RST)
            {
                --S &= 0xFF; sync_hardware();
                --S &= 0xFF; sync_hardware();
                --S &= 0xFF; sync_hardware();
            }
            else
            {
                push(PC >>  8);
                push(PC & 255);
                     if constexpr (type == BRK               ) push(P | 0x30);
                else if constexpr (type == IRQ || type == NMI) push(P | 0x20);
            }
            static constexpr u16 vectors[]{0xFFFA, 0xFFFC, 0xFFFE, 0xFFFE};
            PC  = rb(vectors[type]    );     P |= MI;
            PC |= rb(vectors[type] + 1) << 8;
        }

        template<unsigned opcode>
        void instruction() noexcept
        {
#define O(mnemonic, num, code) if constexpr(opcode == num) {code;} else
            O("LDA <zero page>", 0xA5, lda(zp()))
            O("LDX <zero page>", 0xA6, ldx(zp()))
            O("LDY <zero page>", 0xA4, ldy(zp()))
            O("NOP <zero page>", 0x04, nop(zp()))
            O("NOP <zero page>", 0x44, nop(zp()))
            O("NOP <zero page>", 0x64, nop(zp()))
            O("ORA <zero page>", 0x05, ora(zp()))
            O("ADC <zero page>", 0x65, adc(rb(zp())))
            O("SBC <zero page>", 0xE5, adc(rb(zp()) ^ 255))
            O("AND <zero page>", 0x25, AND(zp()))
            O("BIT <zero page>", 0x24, bit(zp()))
            O("CMP <zero page>", 0xC5, cmp(zp()))
            O("CPX <zero page>", 0xE4, cpx(zp()))
            O("CPY <zero page>", 0xC4, cpy(zp()))
            O("EOR <zero page>", 0x45, eor(zp()))
            O("STA <zero page>", 0x85, sta(zp()))
            O("STX <zero page>", 0x86, stx(zp()))
            O("STY <zero page>", 0x84, sty(zp()))

            O("LSR <zero page>", 0x46, lsr(zp()))
            O("ROL <zero page>", 0x26, rol(zp()))
            O("ROR <zero page>", 0x66, ror(zp()))
            O("ASL <zero page>", 0x06, asl(zp()))
            O("DEC <zero page>", 0xC6, dec(zp()))
            O("INC <zero page>", 0xE6, inc(zp()))

            O("LDA <zero page>, X", 0xB5, lda(zpx()))
            O("LDX <zero page>, Y", 0xB6, ldx(zpy()))
            O("LDY <zero page>, X", 0xB4, ldy(zpx()))
            O("NOP <zero page>, X", 0x14, nop(zpx()))
            O("NOP <zero page>, X", 0x34, nop(zpx()))
            O("NOP <zero page>, X", 0x54, nop(zpx()))
            O("NOP <zero page>, X", 0x74, nop(zpx()))
            O("NOP <zero page>, X", 0xD4, nop(zpx()))
            O("NOP <zero page>, X", 0xF4, nop(zpx()))
            O("ORA <zero page>, X", 0x15, ora(zpx()))
            O("ADC <zero page>, X", 0x75, adc(rb(zpx())))
            O("SBC <zero page>, X", 0xF5, adc(rb(zpx()) ^ 255))
            O("AND <zero page>, X", 0x35, AND(zpx()))
            O("CMP <zero page>, X", 0xD5, cmp(zpx()))
            O("EOR <zero page>, X", 0x55, eor(zpx()))
            O("STA <zero page>, X", 0x95, sta(zpx()))
            O("STX <zero page>, Y", 0x96, stx(zpy()))
            O("STY <zero page>, X", 0x94, sty(zpx()))

            O("LSR <zero page>, X", 0x56, lsr(zpx()))
            O("ROL <zero page>, X", 0x36, rol(zpx()))
            O("ROR <zero page>, X", 0x76, ror(zpx()))
            O("ASL <zero page>, X", 0x16, asl(zpx()))
            O("DEC <zero page>, X", 0xD6, dec(zpx()))
            O("INC <zero page>, X", 0xF6, inc(zpx()))

            O("LDA <absolute>", 0xAD, lda(abs()))
            O("LDX <absolute>", 0xAE, ldx(abs()))
            O("LDY <absolute>", 0xAC, ldy(abs()))
            O("NOP <absolute>", 0x0C, nop(abs()))
            O("ORA <absolute>", 0x0D, ora(abs()))
            O("ADC <absolute>", 0x6D, adc(rb(abs())))
            O("SBC <absolute>", 0xED, adc(rb(abs()) ^ 255))
            O("AND <absolute>", 0x2D, AND(abs()))
            O("BIT <absolute>", 0x2C, bit(abs()))
            O("CMP <absolute>", 0xCD, cmp(abs()))
            O("CPX <absolute>", 0xEC, cpx(abs()))
            O("CPY <absolute>", 0xCC, cpy(abs()))
            O("EOR <absolute>", 0x4D, eor(abs()))
            O("STA <absolute>", 0x8D, sta(abs()))
            O("STX <absolute>", 0x8E, stx(abs()))
            O("STY <absolute>", 0x8C, sty(abs()))

            O("LSR <absolute>", 0x4E, lsr(abs()))
            O("ROL <absolute>", 0x2E, rol(abs()))
            O("ROR <absolute>", 0x6E, ror(abs()))
            O("ASL <absolute>", 0x0E, asl(abs()))
            O("DEC <absolute>", 0xCE, dec(abs()))
            O("INC <absolute>", 0xEE, inc(abs()))

            O("LDA <absolute>, X", 0xBD, lda(abx()))
            O("LDA <absolute>, Y", 0xB9, lda(aby()))
            O("LDX <absolute>, Y", 0xBE, ldx(aby()))
            O("LDY <absolute>, X", 0xBC, ldy(abx()))
            O("NOP <absolute>, X", 0x1C, nop(abx()))
            O("NOP <absolute>, X", 0x3C, nop(abx()))
            O("NOP <absolute>, X", 0x5C, nop(abx()))
            O("NOP <absolute>, X", 0x7C, nop(abx()))
            O("NOP <absolute>, X", 0xDC, nop(abx()))
            O("NOP <absolute>, X", 0xFC, nop(abx()))
            O("ORA <absolute>, X", 0x1D, ora(abx()))
            O("ORA <absolute>, Y", 0x19, ora(aby()))
            O("ADC <absolute>, X", 0x7D, adc(rb(abx())))
            O("ADC <absolute>, Y", 0x79, adc(rb(aby())))
            O("SBC <absolute>, X", 0xFD, adc(rb(abx()) ^ 255))
            O("SBC <absolute>, Y", 0xF9, adc(rb(aby()) ^ 255))
            O("AND <absolute>, X", 0x3D, AND(abx()))
            O("AND <absolute>, Y", 0x39, AND(aby()))
            O("CMP <absolute>, X", 0xDD, cmp(abx()))
            O("CMP <absolute>, Y", 0xD9, cmp(aby()))
            O("EOR <absolute>, X", 0x5D, eor(abx()))
            O("EOR <absolute>, Y", 0x59, eor(aby()))

            O("STA <absolute>, X", 0x9D, sta(abx_big()))
            O("STA <absolute>, Y", 0x99, sta(aby_big()))

            O("LSR <absolute>, X", 0x5E, lsr(abx_big()))
            O("ROL <absolute>, X", 0x3E, rol(abx_big()))
            O("ROR <absolute>, X", 0x7E, ror(abx_big()))
            O("ASL <absolute>, X", 0x1E, asl(abx_big()))
            O("DEC <absolute>, X", 0xDE, dec(abx_big()))
            O("INC <absolute>, X", 0xFE, inc(abx_big()))

            O("LDA <indirect, X>", 0xA1, lda(izx()))
            O("ORA <indirect, X>", 0x01, ora(izx()))
            O("ADC <indirect, X>", 0x61, adc(rb(izx())))
            O("SBC <indirect, X>", 0xE1, adc(rb(izx()) ^ 255))
            O("AND <indirect, X>", 0x21, AND(izx()))
            O("CMP <indirect, X>", 0xC1, cmp(izx()))
            O("EOR <indirect, X>", 0x41, eor(izx()))
            O("STA <indirect, X>", 0x81, sta(izx()))

            O("LDA <indirect>, Y", 0xB1, lda(izy()))
            O("ORA <indirect>, Y", 0x11, ora(izy()))
            O("ADC <indirect>, Y", 0x71, adc(rb(izy())))
            O("SBC <indirect>, Y", 0xF1, adc(rb(izy()) ^ 255))
            O("AND <indirect>, Y", 0x31, AND(izy()))
            O("CMP <indirect>, Y", 0xD1, cmp(izy()))
            O("EOR <indirect>, Y", 0x51, eor(izy()))

            O("STA <indirect>, Y", 0x91, sta(izy_big()))

            O("LDA <immediate>", 0xA9, lda(imm()))
            O("LDX <immediate>", 0xA2, ldx(imm()))
            O("LDY <immediate>", 0xA0, ldy(imm()))
            O("nop <immediate>", 0x80, nop(imm()))
            O("nop <immediate>", 0x82, nop(imm()))
            O("nop <immediate>", 0xC2, nop(imm()))
            O("nop <immediate>", 0xE2, nop(imm()))
            O("nop <immediate>", 0x89, nop(imm()))
            O("ORA <immediate>", 0x09, ora(imm()))
            O("ADC <immediate>", 0x69, adc(rb(imm())))
            O("SBC <immediate>", 0xE9, adc(rb(imm()) ^ 255))
            O("SBC <immediate>", 0xEB, adc(rb(imm()) ^ 255))
            O("AND <immediate>", 0x29, AND(imm()))
            O("CMP <immediate>", 0xC9, cmp(imm()))
            O("CPX <immediate>", 0xE0, cpx(imm()))
            O("CPY <immediate>", 0xC0, cpy(imm()))
            O("EOR <immediate>", 0x49, eor(imm()))

            O("LSR A", 0x4A, lsr_a())
            O("ROL A", 0x2A, rol_a())
            O("ROR A", 0x6A, ror_a())
            O("ASL A", 0x0A, asl_a())

            O("CLC", 0x18, clc())
            O("CLD", 0xD8, cld())
            O("CLV", 0xB8, clv())
            O("CLI", 0x58, cli())

            O("SEI", 0x78, sei())
            O("SEC", 0x38, sec())
            O("SED", 0xF8, sed())
            O("DEX", 0xCA, dex())
            O("DEY", 0x88, dey())
            O("INX", 0xE8, inx())
            O("INY", 0xC8, iny())
            O("TAX", 0xAA, tax())
            O("TAY", 0xA8, tay())
            O("TYA", 0x98, tya())
            O("TSX", 0xBA, tsx())
            O("TXS", 0x9A, txs())
            O("TXA", 0x8A, txa())

            O("NOP", 0x1A, nop(PC))
            O("NOP", 0x3A, nop(PC))
            O("NOP", 0x5A, nop(PC))
            O("NOP", 0x7A, nop(PC))
            O("NOP", 0xDA, nop(PC))
            O("NOP", 0xEA, nop(PC))
            O("NOP", 0xFA, nop(PC))

            O("PHA", 0x48, pha())
            O("PHP", 0x08, php())
            O("PLA", 0x68, pla())
            O("PLP", 0x28, plp())
            O("RTI", 0x40, rti())
            O("RTS", 0x60, rts())

            O("BCS <relative>", 0xB0, bcs())
            O("BCC <relative>", 0x90, bcc())
            O("BEQ <relative>", 0xF0, beq())
            O("BNE <relative>", 0xD0, bne())
            O("BMI <relative>", 0x30, bmi())
            O("BPL <relative>", 0x10, bpl())
            O("BVS <relative>", 0x70, bvs())
            O("BVC <relative>", 0x50, bvc())

            O("JMP <absolute>",          0x4C, jmp_abs())
            O("JMP <absolute indirect>", 0x6C, jmp_ind())
            O("JSR",                     0x20, jsr())

            O("BRK", 0x000, INT<BRK>())
            O("RST", 0x100, INT<RST>())
            O("NMI", 0x101, INT<NMI>())
            O("IRQ", 0x102, INT<IRQ>()) {}
#undef O
        }

    public:
        void set_mem_pointers(const MemPointers& mem_pointers) noexcept {this->mem_pointers = mem_pointers;}
        void set_nmi(bool nmi) noexcept {this->nmi = nmi;}
        void tick() noexcept;
    };
}

#endif
