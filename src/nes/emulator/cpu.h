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

        void oam_dma(u8 value) noexcept;

    public:
        void set_mem_pointers(const MemPointers& mem_pointers) noexcept {this->mem_pointers = mem_pointers;}
        void set_nmi(bool nmi) noexcept {this->nmi = nmi;}
        void instruction() noexcept;
    };
}

#endif
