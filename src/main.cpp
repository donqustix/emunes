#include "nes/emulator/cartridge.h"
#include "nes/emulator/cpu.h"
#include "nes/emulator/ppu.h"
#include "nes/emulator/apu.h"
#include "nes/emulator/controller.h"

#include "nes/emulator/third_party/Nes_Snd_Emu-0.1.7/Sound_Queue.h"
#include "nes/emulator/third_party/nes_ntsc-0.2.2/nes_ntsc.h"

#include <SDL2/SDL.h>

#include <iostream>

namespace
{
    struct NonCopyable
    {
        NonCopyable() = default;
        NonCopyable(const NonCopyable&) = delete;
        NonCopyable& operator=(const NonCopyable&) = delete;
    };
    struct SDL : NonCopyable
    {
        explicit SDL(Uint32 flags) {if (::SDL_Init(flags) < 0) throw std::runtime_error{::SDL_GetError()};}
        ~SDL() {::SDL_Quit();}
    };

    struct SDLwindow : NonCopyable
    {
        SDL_Window* handle;
        SDLwindow(const char* title, int x, int y, int w, int h, Uint32 flags = SDL_WINDOW_RESIZABLE)
        {
            if ((handle = ::SDL_CreateWindow(title, x, y, w, h, flags)) == nullptr)
                throw std::runtime_error{::SDL_GetError()};
        }
        ~SDLwindow() {::SDL_DestroyWindow(handle);}
    };

    struct SDLrenderer : NonCopyable
    {
        SDL_Renderer* handle;
        explicit SDLrenderer(SDL_Window* window)
        {
            if ((handle = ::SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED)) == nullptr)
                throw std::runtime_error{::SDL_GetError()};
        }
        ~SDLrenderer() {::SDL_DestroyRenderer(handle);}
    };

    struct SDLtexture : NonCopyable
    {
        SDL_Texture* handle;
        SDLtexture(SDL_Renderer* renderer, Uint32 format, int access, int w, int h)
        {
            if ((handle = ::SDL_CreateTexture(renderer, format, access, w, h)) == nullptr)
                throw std::runtime_error{::SDL_GetError()};
        }
        ~SDLtexture() {::SDL_DestroyTexture(handle);}
    };

    Sound_Queue sound_queue;

    int dmc_read(void* user_data, cpu_addr_t address) noexcept {return static_cast<nes::emulator::CPU*>(user_data)->dmc_read(user_data, address);}
    void irq_changed(void* user_data) noexcept {static_cast<nes::emulator::CPU*>(user_data)->irq_changed(user_data);}
    void output_samples(const blip_sample_t* samples, size_t count) noexcept {sound_queue.write(samples, count);}

    void run(const char* rom)
    {
        auto cartridge{nes::emulator::Cartridge::load(rom)};
        nes::emulator::CPU cpu;
        nes::emulator::PPU ppu;
        nes::emulator::APU apu;
        nes::emulator::Controller controller;

        nes::emulator::CPU::MemPointers mem_pointers;
        mem_pointers.ppu            = &ppu;
        mem_pointers.apu            = &apu;
        mem_pointers.cartridge      = &cartridge;
        mem_pointers.controller     = &controller;
        cpu.set_mem_pointers(mem_pointers);

        nes::emulator::PPU::MemPointers mem_pointers_ppu;
        mem_pointers_ppu.cartridge      = &cartridge;
        mem_pointers_ppu.cpu            = &cpu;
        ppu.set_mem_pointers(mem_pointers_ppu);

        apu.set_irq_changed(::irq_changed, &cpu);
        apu.set_dmc_reader(::dmc_read, &cpu);
        apu.set_output_samples(::output_samples);

        unsigned char framebuffer[256 * 240];
        ppu.set_pixel_output(framebuffer);

        nes_ntsc_setup_t nes_ntsc_setup = nes_ntsc_composite;
        nes_ntsc_t nes_ntsc;

        nes_ntsc_setup.merge_fields = 0;

        ::nes_ntsc_init(&nes_ntsc, &nes_ntsc_setup);

        constexpr int ntsc_out_width = NES_NTSC_OUT_WIDTH(256), render_height = 240 * 2;

        std::uint_least16_t pixel_output[ntsc_out_width * 240];
        int burst_phase = 0;

        const SDL sdl{SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_AUDIO};
        const SDLwindow window{"emunes", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, ntsc_out_width, render_height};
        const SDLrenderer renderer{window.handle};
        const SDLtexture texture{renderer.handle, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, ntsc_out_width, 240};

        ::SDL_RenderSetLogicalSize(renderer.handle, ntsc_out_width, render_height);
        //::SDL_SetWindowFullscreen(window.handle, SDL_WINDOW_FULLSCREEN);

        if (sound_queue.init(44100))
            throw std::runtime_error{"It's failed to initialize Sound_Queue"};

        SDL_Event event;

        bool key_states[SDL_NUM_SCANCODES]{};

        for (bool running = true; running;)
        {
            const Uint32 start_time = ::SDL_GetTicks();

            while (::SDL_PollEvent(&event))
            {
                switch (event.type)
                {
                    case SDL_QUIT: running = false;                              break;
                    case SDL_KEYDOWN: key_states[event.key.keysym.scancode] = 1; break;
                    case SDL_KEYUP:   key_states[event.key.keysym.scancode] = 0; break;
                }
            }

            if (key_states[SDL_SCANCODE_ESCAPE]) running = false;

            const unsigned char control = key_states[SDL_SCANCODE_SPACE  ] << 0 |
                                          key_states[SDL_SCANCODE_F      ] << 1 |
                                          key_states[SDL_SCANCODE_Q      ] << 2 |
                                          key_states[SDL_SCANCODE_RETURN ] << 3 |
                                          key_states[SDL_SCANCODE_UP     ] << 4 |
                                          key_states[SDL_SCANCODE_DOWN   ] << 5 |
                                          key_states[SDL_SCANCODE_LEFT   ] << 6 |
                                          key_states[SDL_SCANCODE_RIGHT  ] << 7;
            controller.set_port_keys<0>(control);

            cpu.run_cpu(29780);
            apu.end_time_frame(cpu.get_cpu_time());
            cpu.reset_cpu_time();

            burst_phase ^= 1;
            ::nes_ntsc_blit(&nes_ntsc, framebuffer, 256, burst_phase, 256, 240, pixel_output, ntsc_out_width * sizeof (std::uint_least16_t));

            Uint32* pixels;
            int pitch;

            ::SDL_LockTexture(texture.handle, nullptr, reinterpret_cast<void**>(&pixels), &pitch);

            for (int i = 0; i < ntsc_out_width * 240; ++i)
            {
                const unsigned r = (pixel_output[i] >> 10 & 31) * 255 / 31;
                const unsigned g = (pixel_output[i] >>  5 & 31) * 255 / 31;
                const unsigned b = (pixel_output[i]       & 31) * 255 / 31;
                pixels[i] = 0xFF000000 | r << 16 | g << 8 | b;
            }

            ::SDL_UnlockTexture(texture.handle);

            ::SDL_RenderClear(renderer.handle);
            ::SDL_RenderCopy(renderer.handle, texture.handle, nullptr, nullptr);
            ::SDL_RenderPresent(renderer.handle);

            const Uint32 elapsed_time = ::SDL_GetTicks() - start_time;
            if (elapsed_time < 1000 / 60)
                ::SDL_Delay(   1000 / 60 - elapsed_time);
        }
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2)
            throw std::runtime_error{"emunes 'filepath'"};
        ::run(argv[1]);
    }
    catch (const std::exception& ex)
    {
        std::clog << ex.what() << std::endl;
    }
    return 0;
}
