#ifndef APU_H
#define APU_H

#include "int_alias.h"

#include "third_party/Nes_Snd_Emu-0.1.7/nes_apu/Nes_Apu.h"

namespace nes::emulator
{
    class CPU;

    class APU final
    {
        Blip_Buffer buffer;
        Nes_Apu handle;
        blip_sample_t output_buffer[4096];
        CPU* cpu;

        void (*output_samples)(const blip_sample_t* samples, size_t count);

    public:
        APU();

        void end_time_frame(cpu_time_t length) noexcept
        {
            handle.end_frame(length);
            buffer.end_frame(length);
            
            if (buffer.samples_avail() >= 4096)
            {
                const size_t count = buffer.read_samples(output_buffer, 4096);
                output_samples(output_buffer, count);
            }
        }

        cpu_time_t earliest_irq() noexcept {return handle.earliest_irq();}

        void write_register(cpu_time_t cpu_time, cpu_addr_t address, int data) noexcept
        {
            handle.write_register(cpu_time, address, data);
        }

        u8 read_status(cpu_time_t cpu_time) noexcept {return handle.read_status(cpu_time);}

        void set_output_samples(void (*output_samples)(const blip_sample_t*, size_t)) noexcept {this->output_samples = output_samples;}
        void set_cpu_pointer(CPU* cpu) noexcept {this->cpu = cpu;}

        void set_dmc_reader(int (*dmc_read)(void*, cpu_addr_t address)) noexcept {handle.dmc_reader(dmc_read);}
        void set_irq_changed(void (*irq_changed)(void*)) noexcept {handle.irq_notifier(irq_changed);}
    };
}

#endif
