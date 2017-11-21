#include "nes/emulator/cartridge.h"
#include "nes/emulator/cpu.h"
#include "nes/emulator/ppu.h"

#include <SDL2/SDL.h>

using namespace nes::emulator;

int main()
{
    auto cartridge{nes::emulator::Cartridge::load("res/roms/Super Mario Bros (E).nes")};
    nes::emulator::CPU cpu;
    nes::emulator::PPU ppu;

    nes::emulator::CPU::MemPointers mem_pointers;
    mem_pointers.ppu            = &ppu;
    mem_pointers.cartridge      = &cartridge;
    cpu.set_mem_pointers(mem_pointers);

    nes::emulator::PPU::MemPointers mem_pointers_ppu;
    mem_pointers_ppu.cartridge      = &cartridge;
    mem_pointers_ppu.cpu            = &cpu;
    ppu.set_mem_pointers(mem_pointers_ppu);

    ::SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    SDL_Window* const window = ::SDL_CreateWindow("emunes", 100, 100, 256, 240, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* const renderer = ::SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* const texture = ::SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING, 256, 240);

    //::SDL_RenderSetLogicalSize(renderer, 256, 240);
    //::SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);

    SDL_Event event;
    for (bool running = true; running;)
    {
        while (::SDL_PollEvent(&event))
            if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) running = false;

        for (int i = 0; i < 1500; ++i)
            cpu.instruction();

        if (ppu.new_frame())
        {
            Uint32* pixels;
            int pitch;

            ::SDL_LockTexture(texture, nullptr, reinterpret_cast<void**>(&pixels), &pitch);

            for (int i = 0; i < 240 * 256; ++i)
                pixels[i] = 0xFF000000 | ppu.get_framebuffer()[i];

            ::SDL_UnlockTexture(texture);
        }

        ::SDL_RenderClear(renderer);
        ::SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        ::SDL_RenderPresent(renderer);
        ::SDL_Delay(1);
    }

    ::SDL_DestroyTexture(texture);
    ::SDL_DestroyRenderer(renderer);
    ::SDL_DestroyWindow(window);
    ::SDL_Quit();

    return 0;
}
