#include "nes/emulator/cartridge.h"
#include "nes/emulator/cpu.h"
#include "nes/emulator/ppu.h"
#include "nes/emulator/controller.h"

#include <SDL2/SDL_net.h>
#include <SDL2/SDL.h>

#include <algorithm>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <iterator>
#include <list>

namespace
{
    struct SDLNet
    {
        SDLNet() {if (::SDLNet_Init() == -1) throw std::runtime_error{::SDLNet_GetError()};}
        ~SDLNet() {::SDLNet_Quit();}
    };

    struct SDLNetTCPsocket
    {
        TCPsocket handle;
#define MAKE_CODE(func, arg)                                          \
    {                                                                 \
        TCPsocket tcp_socket;                                         \
        if ((tcp_socket = ::SDLNet_TCP_##func(arg)) == nullptr)       \
            throw std::runtime_error{::SDLNet_GetError()};            \
        return SDLNetTCPsocket{tcp_socket};                           \
    }
        static SDLNetTCPsocket open(IPaddress* ip_address) {MAKE_CODE(Open, ip_address)}
        static SDLNetTCPsocket accept(TCPsocket server_tcp_socket) {MAKE_CODE(Accept, server_tcp_socket)}
#undef MAKE_CODE
        explicit SDLNetTCPsocket(TCPsocket handle) noexcept : handle{handle} {}
        SDLNetTCPsocket(SDLNetTCPsocket&& tcp_socket) noexcept : handle{tcp_socket.handle} {tcp_socket.handle = nullptr;}
        SDLNetTCPsocket(const SDLNetTCPsocket&) = delete;
        SDLNetTCPsocket& operator=(const SDLNetTCPsocket&) = delete;
        ~SDLNetTCPsocket() {if (handle) ::SDLNet_TCP_Close(handle);}
    };

    struct SDLNetSocketSet
    {
        std::list<SDLNetTCPsocket> tcp_sockets;
        SDLNet_SocketSet handle;
        explicit SDLNetSocketSet(int size)
        {
            if ((handle = ::SDLNet_AllocSocketSet(size)) == nullptr)
                throw std::runtime_error{::SDLNet_GetError()};
        }
        SDLNetSocketSet(const SDLNetSocketSet&) = delete;
        SDLNetSocketSet& operator=(const SDLNetSocketSet&) = delete;
        ~SDLNetSocketSet()
        {
            for (auto& s : tcp_sockets) ::SDLNet_TCP_DelSocket(handle, s.handle);
            ::SDLNet_FreeSocketSet(handle);
        }
        void add_tcp_socket(SDLNetTCPsocket tcp_socket)
        {
            ::SDLNet_TCP_AddSocket(handle, tcp_socket.handle);
            tcp_sockets.push_back(std::move(tcp_socket));
        }
        void del_tcp_socket(decltype(tcp_sockets)::const_iterator iter)
        {
            ::SDLNet_TCP_DelSocket(handle, iter->handle);
            tcp_sockets.erase(iter);
        }
    };

    struct SDL
    {
        explicit SDL(Uint32 flags) {if (::SDL_Init(flags) < 0) throw std::runtime_error{::SDL_GetError()};}
        ~SDL() {::SDL_Quit();}
    };

    struct SDLwindow
    {
        SDL_Window* handle;
        SDLwindow(const char* title, int x, int y, int w, int h, Uint32 flags = SDL_WINDOW_RESIZABLE)
        {
            if ((handle = ::SDL_CreateWindow(title, x, y, w, h, flags)) == nullptr)
                throw std::runtime_error{::SDL_GetError()};
        }
        SDLwindow(const SDLwindow&) = delete;
        SDLwindow& operator=(const SDLwindow&) = delete;
        ~SDLwindow() {::SDL_DestroyWindow(handle);}
    };

    struct SDLrenderer
    {
        SDL_Renderer* handle;
        explicit SDLrenderer(SDL_Window* window)
        {
            if ((handle = ::SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED)) == nullptr)
                throw std::runtime_error{::SDL_GetError()};
        }
        SDLrenderer(const SDLrenderer&) = delete;
        SDLrenderer& operator=(const SDLrenderer&) = delete;
        ~SDLrenderer() {::SDL_DestroyRenderer(handle);}
    };

    struct SDLtexture
    {
        SDL_Texture* handle;
        SDLtexture(SDL_Renderer* renderer, Uint32 format, int access, int w, int h)
        {
            if ((handle = ::SDL_CreateTexture(renderer, format, access, w, h)) == nullptr)
                throw std::runtime_error{::SDL_GetError()};
        }
        SDLtexture(const SDLtexture&) = delete;
        SDLtexture& operator=(const SDLtexture&) = delete;
        ~SDLtexture() {::SDL_DestroyTexture(handle);}
    };

    void make_server(const char* rom, int port)
    {
        auto cartridge{nes::emulator::Cartridge::load(rom)};
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

        nes::emulator::px32 framebuffer[256 * 240], framebuffer_temp[256 * 240];
        ppu.set_pixel_output(framebuffer);

        const SDL sdl{SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER};
        const SDLwindow window{"emunes-server", 0, 0, 256 * 2, 240 * 2};
        const SDLrenderer renderer{window.handle};
        const SDLtexture texture{renderer.handle, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 256, 240};

        const SDLNet sdl_net;

        IPaddress ip_address;
        if (::SDLNet_ResolveHost(&ip_address, nullptr, port) == -1)
            throw std::runtime_error{::SDLNet_GetError()};

        SDLNetSocketSet socket_set{3};
        socket_set.add_tcp_socket(SDLNetTCPsocket::open(&ip_address));

        constexpr unsigned secs_per_update = 1000 / 60, instrs_per_update = 11000;
        unsigned acc_update_time = 0;

        SDL_Event event;

        Uint32 previous_time = ::SDL_GetTicks();

        for (bool running = true; running;)
        {
            const Uint32 current_time = ::SDL_GetTicks();
            const Uint32 elapsed_time = current_time - previous_time;
            previous_time = current_time;

            acc_update_time += elapsed_time;

            while (::SDL_PollEvent(&event))
                if (event.type == SDL_QUIT) running = false;

            const int sockets_status = ::SDLNet_CheckSockets(socket_set.handle, 0);
            if (sockets_status > 0)
            {
                auto iter_server = socket_set.tcp_sockets.begin();
                unsigned player = 0;
                for (auto iter = std::next(iter_server); iter != socket_set.tcp_sockets.end(); ++iter, ++player)
                {
                    if (::SDLNet_SocketReady(iter->handle))
                    {
                        unsigned char control;
                        if (!::SDLNet_TCP_Recv(iter->handle, &control, 1)) socket_set.del_tcp_socket(iter--);
                        else if (control == 0xFF)
                        {
                            static constexpr int framebuffer_size = 256 * 240 * sizeof (nes::emulator::px32);
                            if (::SDLNet_TCP_Send(iter->handle, framebuffer_temp, framebuffer_size) < framebuffer_size)
                                socket_set.del_tcp_socket(iter--);
                        }
                        else
                        {
                            switch (player)
                            {
                                case 0: controller.set_port_keys<0>(control); break;
                                case 1: controller.set_port_keys<1>(control); break;
                            }
                        }
                    }
                }
                if (socket_set.tcp_sockets.size() < 4 && ::SDLNet_SocketReady(iter_server->handle))
                {
                    auto new_tcp_socket{SDLNetTCPsocket::accept(iter_server->handle)};
                    socket_set.add_tcp_socket(std::move(new_tcp_socket));
                }
            }

            for (; acc_update_time >= secs_per_update; acc_update_time -= secs_per_update)
                for (auto i = instrs_per_update; i--;) cpu.instruction();

            if (ppu.new_frame())
            {
                std::copy_n(framebuffer, 256 * 240, framebuffer_temp);

                Uint32* pixels;
                int pitch;

                ::SDL_LockTexture(texture.handle, nullptr, reinterpret_cast<void**>(&pixels), &pitch);

                for (int i = 0; i < 240 * 256; ++i)
                    pixels[i] = 0xFF000000 | framebuffer_temp[i];

                ::SDL_UnlockTexture(texture.handle);
            }

            ::SDL_RenderClear(renderer.handle);
            ::SDL_RenderCopy(renderer.handle, texture.handle, nullptr, nullptr);
            ::SDL_RenderPresent(renderer.handle);
        }
    }

    void make_client(const char*  ip, int port)
    {
        const SDLNet sdl_net;

        IPaddress ip_address;
        if (::SDLNet_ResolveHost(&ip_address, ip, port) == -1)
            throw std::runtime_error{::SDLNet_GetError()};

        SDLNetSocketSet socket_set{1};
        socket_set.add_tcp_socket(SDLNetTCPsocket::open(&ip_address));

        auto& server_tcp_socket = socket_set.tcp_sockets.front();

        constexpr unsigned char garbage = 0xFF;
        if (!::SDLNet_TCP_Send(server_tcp_socket.handle, &garbage, 1))
            throw std::runtime_error{::SDLNet_GetError()};

        const SDL sdl{SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER};
        const SDLwindow window{"emunes-client", 0, 0, 256 * 2, 240 * 2};
        const SDLrenderer renderer{window.handle};
        const SDLtexture texture{renderer.handle, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 256, 240};

        SDL_Event event;

        bool key_states[SDL_NUM_SCANCODES]{};

        for (bool running = true; running;)
        {
            while (::SDL_PollEvent(&event))
            {
                switch (event.type)
                {
                    case SDL_QUIT: running = false;                              break;
                    case SDL_KEYDOWN: key_states[event.key.keysym.scancode] = 1; break;
                    case SDL_KEYUP:   key_states[event.key.keysym.scancode] = 0; break;
                }
            }

            static unsigned prev_control = 0;
            const unsigned char control = key_states[SDL_SCANCODE_SPACE  ] << 0 |
                                          key_states[SDL_SCANCODE_F      ] << 1 |
                                          key_states[SDL_SCANCODE_Q      ] << 2 |
                                          key_states[SDL_SCANCODE_RETURN ] << 3 |
                                          key_states[SDL_SCANCODE_W      ] << 4 |
                                          key_states[SDL_SCANCODE_S      ] << 5 |
                                          key_states[SDL_SCANCODE_A      ] << 6 |
                                          key_states[SDL_SCANCODE_D      ] << 7;
            if (prev_control != control)
            {
                if (!::SDLNet_TCP_Send(socket_set.tcp_sockets.front().handle, &control, 1))
                    throw std::runtime_error{::SDLNet_GetError()};
                prev_control = control;
            }

            if (::SDLNet_CheckSockets(socket_set.handle, 0) > 0)
            {
                if (::SDLNet_SocketReady(server_tcp_socket.handle))
                {
                    static constexpr int framebuffer_size = 256 * 240 * sizeof (nes::emulator::px32);
                    nes::emulator::px32 framebuffer[framebuffer_size];
                    int received_size = ::SDLNet_TCP_Recv(server_tcp_socket.handle, framebuffer, framebuffer_size);
                    for (unsigned i = 3; i && received_size < framebuffer_size; --i)
                    {
                        if (::SDLNet_CheckSockets(socket_set.handle, 1000) > 0 && ::SDLNet_SocketReady(server_tcp_socket.handle))
                        {
                            received_size += 
                                ::SDLNet_TCP_Recv(server_tcp_socket.handle, framebuffer      + received_size,
                                                                            framebuffer_size - received_size);
                        }
                    }
                    if (received_size < framebuffer_size)
                        throw std::runtime_error{"received_size < framebuffer_size"};

                    if (!::SDLNet_TCP_Send(server_tcp_socket.handle, &garbage, 1))
                        throw std::runtime_error{::SDLNet_GetError()};

                    Uint32* pixels;
                    int pitch;

                    ::SDL_LockTexture(texture.handle, nullptr, reinterpret_cast<void**>(&pixels), &pitch);

                    for (int i = 0; i < 240 * 256; ++i)
                        pixels[i] = 0xFF000000 | framebuffer[i];

                    ::SDL_UnlockTexture(texture.handle);
                }
            }

            ::SDL_RenderClear(renderer.handle);
            ::SDL_RenderCopy(renderer.handle, texture.handle, nullptr, nullptr);
            ::SDL_RenderPresent(renderer.handle);

            ::SDL_Delay(1);
        }
    }
}

/*
 * --server rom port
 * --client  ip port
 *
 * --help
 */
int main(int argc, char** argv)
{
    try
    {
        switch (argc)
        {
            case 2:
                if (!std::strcmp(argv[1], "--help"))
                    std::cout << "options:\n--server rom port\n--client ip port" << std::endl;
                break;
            case 4:
                     if (!std::strcmp(argv[1], "--server")) {make_server(argv[2], std::atoi(argv[3])); break;}
                else if (!std::strcmp(argv[1], "--client")) {make_client(argv[2], std::atoi(argv[3])); break;}
            default:
                throw std::runtime_error{"use '--help' to help yourself"};
        }
    }
    catch (const std::exception& ex)
    {
        std::clog << ex.what() << std::endl;
    }
    return 0;
}
