#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

#include <lexxos/drivers/core/display/ili9488_commands.h>
#include <lexxos/drivers/core/display/ili9488_init.h>

namespace {

using lexxos::drivers::core::Ili9488InitTransport;
using lexxos::drivers::core::Ili9488PixelFormat;
using lexxos::drivers::core::initializeIli9488;

void check(bool ok) {
	if (!ok) {
		std::cerr << "ILI9488 init trace failed\n";
		std::abort();
	}
}

struct Trace {
	std::vector<unsigned> events;
	unsigned calls{};
	unsigned failAt{};
};

bool write(void* context, std::uint8_t command, std::uint8_t const* data, std::size_t size) {
	auto& trace = *static_cast<Trace*>(context);
	check((size == 0) == (data == nullptr));
	trace.events.push_back(0x100U | command);
	for (std::size_t i = 0; i < size; ++i) {
		trace.events.push_back(data[i]);
	}

	return ++trace.calls != trace.failAt;
}

void sleep(void* context, std::uint32_t ms) {
	static_cast<Trace*>(context)->events.push_back(0x10000U | ms);
}

// Зафиксированная последовательность legacy до выделения core.
std::vector<unsigned> expected(unsigned format, bool inverted) {
	return {
	    0x1E0, 0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78, 0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F,
	    0x1E1, 0x00, 0x16, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45, 0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37,
	    0x1C0, 0x17, 0x15, 0x1C1, 0x41, 0x1C5, 0x00, 0x12, 0x80,
	    0x136, 0x48, 0x13A, format, 0x1B0, 0, 0x1B1, 0xA0,
	    inverted ? 0x121U : 0x120U, 0x1B4, 2, 0x1B6, 2, 2, 0x3B,
	    0x1B7, 0xC6, 0x1E9, 0, 0x1F7, 0xA9, 0x51, 0x2C, 0x82,
	    0x111, 0x10078, 0x129, 0x10078, 0x136, 0x48};
}

} // namespace

int main() {
	using lexxos::drivers::core::configureIli9488Scroll;
	using lexxos::drivers::core::Ili9488PowerMode;
	using lexxos::drivers::core::setIli9488Power;
	using lexxos::drivers::core::setIli9488ScrollStart;
	using lexxos::drivers::core::setIli9488Window;
	{
		Trace trace;
		Ili9488InitTransport transport{&trace, write, sleep};
		check(setIli9488Window(transport, 0x123, 2, 0x145, 0xFFFF));
		check(trace.events == std::vector<unsigned>({0x12A, 1, 0x23, 1, 0x45, 0x12B, 0, 2, 255, 255, 0x12C}));
		trace = {};
		check(!setIli9488Window(transport, 2, 0, 1, 1) && trace.events.empty());
		check(configureIli9488Scroll(transport, 16, 320, 144));
		check(trace.events == std::vector<unsigned>({0x133, 0, 16, 1, 64, 0, 144}));
		trace = {};
		check(setIli9488ScrollStart(transport, 0x123));
		check(trace.events == std::vector<unsigned>({0x137, 1, 0x23}));
		trace = {};
		check(!configureIli9488Scroll(transport, 0, 0, 0));
		check(!configureIli9488Scroll(transport, -1, 1, 0));
		check(!configureIli9488Scroll(transport, 1, std::numeric_limits<int>::max(), 1));
		check(!setIli9488ScrollStart(transport, -1));
		check(!setIli9488ScrollStart(transport, 65536));
		check(trace.events.empty());
		for (unsigned failure = 1; failure <= 3; ++failure) {
			trace = {.failAt = failure};
			check(!setIli9488Window(transport, 0, 0, 1, 1) && trace.calls == failure);
		}

		for (auto previous : {Ili9488PowerMode::On, Ili9488PowerMode::Off, Ili9488PowerMode::Sleep}) {
			for (auto next : {Ili9488PowerMode::On, Ili9488PowerMode::Off, Ili9488PowerMode::Sleep}) {
				trace = {};
				check(setIli9488Power(transport, previous, next));
				std::vector<unsigned> expectedPower;
				if (previous != next) {
					if (next == Ili9488PowerMode::Sleep) {
						expectedPower = {0x128, 0x10014, 0x110, 0x10078};
					} else {
						if (previous == Ili9488PowerMode::Sleep) {
							expectedPower = {0x111, 0x10078};
						}

						expectedPower.push_back(next == Ili9488PowerMode::On ? 0x129 : 0x128);
						if (next == Ili9488PowerMode::On) {
							expectedPower.push_back(0x10014);
						}
					}
				}

				check(trace.events == expectedPower);
				auto calls = trace.calls;
				for (unsigned failure = 1; failure <= calls; ++failure) {
					trace = {.failAt = failure};
					check(!setIli9488Power(transport, previous, next) && trace.calls == failure);
					check(trace.events.back() < 0x10000);
				}
			}
		}
		trace = {};
		check(!setIli9488Power(transport, Ili9488PowerMode::On, Ili9488PowerMode::Invalid));
		check(trace.events.empty());
	}

	for (auto format : {Ili9488PixelFormat::Rgb565, Ili9488PixelFormat::Rgb666}) {
		for (bool inverted : {false, true}) {
			Trace trace;
			Ili9488InitTransport transport{&trace, write, sleep};
			check(initializeIli9488(transport, format, inverted));
			check(trace.calls == 18 && trace.events == expected(static_cast<unsigned>(format), inverted));
			for (unsigned failure = 1; failure <= 18; ++failure) {
				Trace failed{.failAt = failure};
				transport.context = &failed;
				check(!initializeIli9488(transport, format, inverted));
				check(failed.calls == failure);
				// Следом за отказавшей командой не должно быть ни задержки, ни записи.
				auto last = failed.events.back();
				check(last < 0x10000);
				check(failed.events.size() <= trace.events.size());
				for (std::size_t i = 0; i < failed.events.size(); ++i) {
					check(failed.events[i] == trace.events[i]);
				}
			}
		}
	}
	Trace trace;
	check(!initializeIli9488({&trace, nullptr, sleep}, Ili9488PixelFormat::Rgb666, false));
	check(!initializeIli9488({&trace, write, nullptr}, Ili9488PixelFormat::Rgb666, false));
	check(!initializeIli9488({&trace, write, sleep}, static_cast<Ili9488PixelFormat>(0), false));
	check(trace.events.empty());
}
