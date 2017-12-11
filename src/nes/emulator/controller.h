#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "int_alias.h"

#include <mutex>

namespace nes::emulator
{
    class Controller final
    {
        unsigned char port_1,     port_2,
                      port_1_buf, port_2_buf;
        bool strobe;

        std::mutex mx;

        template<bool port>
        unsigned get_port_keys() noexcept
        {
            std::lock_guard guard{mx};
            unsigned temp;
            if constexpr (port) temp = port_2_buf;
            else                temp = port_1_buf;
            return temp;
        }
    public:
        void write(u8 value) noexcept
        {
            strobe = value & 1;
            if (strobe)
            {
                port_1 = get_port_keys<0>();
                port_2 = get_port_keys<1>();
            }
        }

        template<bool port>
        u8 read() noexcept
        {
            u8* port_i;
            if constexpr (port) port_i = &port_2;
            else                port_i = &port_1;
            if (strobe) return *port_i & 1;
            const u8 temp    = *port_i & 1; *port_i >>= 1;
            return   temp;
        }

        template<bool port>
        void set_port_keys(unsigned char keys) noexcept
        {
            std::lock_guard guard{mx};
            if constexpr (port) port_2_buf = keys;
            else                port_1_buf = keys;
        }
    };
}

#endif
