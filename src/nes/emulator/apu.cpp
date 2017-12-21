#include "apu.h"

#include <stdexcept>

using namespace nes::emulator;

APU::APU()
{
	const blargg_err_t error = buffer.sample_rate(44100);
	if (error)
        throw std::runtime_error{"APU initialization error"};
	buffer.clock_rate(1789773);
	handle.output(&buffer);
}

