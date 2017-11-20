#ifndef APU_H
#define APU_H

#include "cpu.h"

#include <iostream>

namespace nes::emulator
{
    class CPU;
    class APU
    {
    public:
        struct MemPointers
        {
            CPU* cpu;
        };
    private:
        MemPointers mem_pointers;
        enum FrameCounterMode {M4, M5} frame_counter_mode = M4;
        unsigned frame_counter_clock = 0;
        unsigned delayed_frame_timer_reset = 0;
        bool inhibit_frame_irq = false;
        bool apu_clk1_is_high = false;
    public:
        void write_frame_counter(unsigned v) noexcept
        {
            frame_counter_mode = FrameCounterMode(v >> 7);
            if ((inhibit_frame_irq = v & 0x40))
                mem_pointers.cpu->set_irq(false);
            delayed_frame_timer_reset = apu_clk1_is_high ? 4 : 3;
        }
        void clock_frame_counter_clock() noexcept
        {
            switch (frame_counter_mode)
            {
                case M4:
                    if (delayed_frame_timer_reset > 0 && --delayed_frame_timer_reset == 0)
                        frame_counter_clock = 0;
                    else
                        if (++frame_counter_clock == 2*14914 + 2) {
                            frame_counter_clock = 0;
                            if (!inhibit_frame_irq) {mem_pointers.cpu->set_irq(true);}
                        }
                    switch (frame_counter_clock)
                    {
                        case 2*14914:
                        case 2*14914 + 1:
                            if (!inhibit_frame_irq) mem_pointers.cpu->set_irq(true);
                            break;
                    }
                    break;
                case M5:
                    if (delayed_frame_timer_reset > 0 && --delayed_frame_timer_reset == 0)
                        frame_counter_clock = 0;
                    else
                        if (++frame_counter_clock == 2*18640 + 2)
                            frame_counter_clock = 0;
                    break;
            }
        }
        void tick() noexcept {apu_clk1_is_high = !apu_clk1_is_high; clock_frame_counter_clock();}
        void set_mem_pointers(const MemPointers& mem_pointers) noexcept {this->mem_pointers = mem_pointers;}
    };
}

#endif
