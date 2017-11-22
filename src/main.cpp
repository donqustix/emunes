#include "nes/emulator/cartridge.h"
#include "nes/emulator/cpu.h"
#include "nes/emulator/ppu.h"
#include "nes/emulator/controller.h"

#include <SDL2/SDL.h>

#include <iostream>
#include <fstream>

using namespace nes::emulator;

namespace nes::emulator
{
    void execute(std::string_view filepath)
    {
        auto cartridge{nes::emulator::Cartridge::load(filepath)};
        nes::emulator::CPU cpu;
        nes::emulator::PPU ppu;
        nes::emulator::Controller controller;

        nes::emulator::CPU::MemPointers mem_pointers;
        mem_pointers.ppu            = &ppu;
        mem_pointers.cartridge      = &cartridge;
        mem_pointers.controller     = &controller;
        cpu.set_mem_pointers(mem_pointers);

        nes::emulator::PPU::MemPointers mem_pointers_ppu;
        mem_pointers_ppu.cartridge      = &cartridge;
        mem_pointers_ppu.cpu            = &cpu;
        ppu.set_mem_pointers(mem_pointers_ppu);

        px32 framebuffer[256 * 240];
        ppu.set_pixel_output(framebuffer);

        ::SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER);
        SDL_Window* const window = ::SDL_CreateWindow("emunes", 0, 0, 256, 240, SDL_WINDOW_RESIZABLE);
        SDL_Renderer* const renderer = ::SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        SDL_Texture* const texture = ::SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                SDL_TEXTUREACCESS_STREAMING, 256, 240);

        //::SDL_RenderSetLogicalSize(renderer, 256, 240);
        //::SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);

        constexpr unsigned secs_per_update = 1000 / 60, instrs_per_update = 11000;
        unsigned acc_update_time = 0;

        Uint32 previous_time = ::SDL_GetTicks();

        for (bool running = true; running;)
        {
            const Uint32 current_time = ::SDL_GetTicks();
            const Uint32 elapsed_time = current_time - previous_time;
            previous_time = current_time;

            acc_update_time += elapsed_time;

            ::SDL_PumpEvents();

            const Uint8* const keyboard_state = ::SDL_GetKeyboardState(nullptr);

            static bool triggered = false, latch = false;

            if (keyboard_state[SDL_SCANCODE_ESCAPE]) running = false;
            if (keyboard_state[SDL_SCANCODE_P]) triggered = true;
            else if (triggered) {triggered = false; latch = true;}

            controller.set_port_keys<0>(keyboard_state[SDL_SCANCODE_SPACE   ] << 0 |
                                        keyboard_state[SDL_SCANCODE_F       ] << 1 |
                                        keyboard_state[SDL_SCANCODE_Q       ] << 2 |
                                        keyboard_state[SDL_SCANCODE_RETURN  ] << 3 |
                                        keyboard_state[SDL_SCANCODE_UP      ] << 4 |
                                        keyboard_state[SDL_SCANCODE_DOWN    ] << 5 |
                                        keyboard_state[SDL_SCANCODE_LEFT    ] << 6 |
                                        keyboard_state[SDL_SCANCODE_RIGHT   ] << 7);
            for (; acc_update_time >= secs_per_update; acc_update_time -= secs_per_update)
                for (auto i = instrs_per_update; i--;) cpu.instruction();

            if (ppu.new_frame())
            {
                if (latch)
                {
                    static unsigned i = 0;
                    std::ofstream stream{"res/images/screenshot" + std::to_string(i++) + ".ppm", std::ios::out | std::ios::binary};
                    stream << "P6\n" << 256 << ' ' << 240 << '\n' << 255 << '\n';
                    for (int i = 0; i < 256 * 240; ++i)
                    {
                        const unsigned char r = framebuffer[i] >>  0 & 255;
                        const unsigned char g = framebuffer[i] >>  8 & 255;
                        const unsigned char b = framebuffer[i] >> 16 & 255;
                        stream.put(r);stream.put(g);stream.put(b);
                    }
                    latch = false;
                }

                Uint32* pixels;
                int pitch;

                ::SDL_LockTexture(texture, nullptr, reinterpret_cast<void**>(&pixels), &pitch);

                for (int i = 0; i < 240 * 256; ++i)
                    pixels[i] = 0xFF000000 | framebuffer[i];

                ::SDL_UnlockTexture(texture);
            }

            ::SDL_RenderClear(renderer);
            ::SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            ::SDL_RenderPresent(renderer);
        }

        ::SDL_DestroyTexture(texture);
        ::SDL_DestroyRenderer(renderer);
        ::SDL_DestroyWindow(window);
        ::SDL_Quit();
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2) throw std::runtime_error{"input: file path"};
        nes::emulator::execute(argv[1]);
    }
    catch (const std::exception& ex)
    {
        std::clog << ex.what() << std::endl;
    }
    return 0;
}
