#ifndef APU_H
#define APU_H

#include "third_party/Nes_Snd_Emu-0.1.7/nes_apu/Nes_Apu.h"

namespace nes::emulator
{
    class APU final
    {
        Blip_Buffer buffer;
        Nes_Apu handle;
        blip_sample_t output_buffer[4096];

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

        int read_status(cpu_time_t cpu_time) noexcept {return handle.read_status(cpu_time);}

        void set_output_samples(void (*output_samples)(const blip_sample_t*, size_t)) noexcept {this->output_samples = output_samples;}
        void set_dmc_reader(int (*dmc_read)(void*, cpu_addr_t address), void* user_data = nullptr) noexcept {handle.dmc_reader(dmc_read, user_data);}
        void set_irq_changed(void (*irq_changed)(void*), void* user_data = nullptr) noexcept {handle.irq_notifier(irq_changed, user_data);}
    };
}

#endif
