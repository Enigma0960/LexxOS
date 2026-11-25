#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <lexxos/composition/detail/ili9488_workspace.h>
#include <lexxos/composition/products/lyra_spi_queues.h>
#include <lexxos/devices/display/ili9488_display.h>
#include <lexxos/devices/display/legacy_ili9488_backend.h>
#include <lexxos/devices/rk3506/picocalc_lcd_spi0.h>
#include <lexxos/platform/platform_services.h>

namespace {

void check(bool ok) {
	if (!ok) {
		std::cerr << "LCD transaction contract failed\n";
		std::abort();
	}
}

auto& queue = lexxos::composition::products::lyraSpi0Queue();
bool selected = false;
bool failNext = false;
unsigned writes = 0;
bool capture = false;
std::vector<std::uint8_t> payload;
std::uint32_t speed = 0;

bool gpio(lexxos::platform::GpioPin pin, bool value) {
	check(queue.busy());
	if (pin.number == 10) {
		selected = !value;
	}
	return true;
}

class FakePanelBackend final : public lexxos::devices::Ili9488Backend {
public:
	unsigned initializations{};
	unsigned transfers{};
	bool selected{};
	bool fail{};
	bool data{};
	std::vector<std::uint8_t> bytes;
	std::vector<std::uint8_t> pixelFormats;
	std::uint8_t command{};
	mutable std::vector<std::pair<char, std::uint32_t>> resetEvents;
	std::uint32_t speed{1000000};

	bool initialize() override {
		++initializations;
		return true;
	}

	bool write(std::uint8_t const* buffer, std::size_t size) override {
		check(selected && queue.busy() && size > 0);
		if (!data) {
			command = buffer[0];
		} else if (command == 0x3A) {
			pixelFormats.push_back(buffer[0]);
		}

		bytes.insert(bytes.end(), buffer, buffer + size);
		++transfers;
		return !fail;
	}

	bool configureBusSpeed(std::uint32_t hz) override {
		check(queue.busy());
		speed = hz;
		return true;
	}

	std::uint32_t busSpeedHz() const override { return speed; }

	bool hardwareSpi() const override { return true; }

	void select(bool value) override {
		check(queue.busy());
		selected = value;
	}

	void dataMode(bool value) override { data = value; }

	void resetLine(bool high) override {
		check(queue.busy());
		resetEvents.emplace_back('r', high);
	}

	void sleepMs(std::uint32_t ms) const override { resetEvents.emplace_back('s', ms); }
};

lexxos::platform::PlatformServices const fakePlatform{
    .gpio = {.configureOutput = gpio, .write = gpio},
};

lexxos::platform::SpiTftDisplayPins pins() {
	lexxos::platform::SpiTftDisplayPins result{};
	result.enabled = true;
	result.spiIndex = 0;
	result.spiBaudRate = 1000000;
	result.sck = {7};
	result.mosi = {6};
	result.cs = {10};
	result.dc = {11};
	result.reset = {12};
	result.width = 2;
	result.height = 2;
	return result;
}

} // namespace

namespace lexxos::platform {

PlatformServices const& services() {
	return fakePlatform;
}

// Physical display drivers report diagnostics through the platform console.
// The host transaction fixture intentionally discards those messages.
void writeConsole(std::string_view) {
}

} // namespace lexxos::platform

namespace lexxos::platform::rk3506 {

bool lcdSpi0Initialize(std::uint32_t hz) {
	check(queue.busy());
	speed = hz;
	return true;
}

bool lcdSpi0SetClockHz(std::uint32_t hz) {
	check(queue.busy());
	speed = hz;
	return true;
}

std::uint32_t lcdSpi0ClockHz() {
	return speed;
}

// This transaction fixture exercises packed byte transport; direct16 programs
// and their source lifetime are covered by lcd_dma_tests.
bool lcdSpi0Rgb565Direct16Enabled() {
	return false;
}

bool lcdSpi0WriteRgb565Words(std::uint16_t const*, std::size_t) {
	check(false); // A backend without this capability must never receive words.
	return false;
}

bool lcdSpi0Write(std::uint8_t const* data, std::size_t count) {
	check(queue.busy() && selected);
	++writes;
	if (capture) {
		payload.insert(payload.end(), data, data + count);
	}

	if (failNext) {
		failNext = false;
		return false;
	}

	return true;
}

} // namespace lexxos::platform::rk3506

int main() {
	lexxos::devices::LegacyIli9488Backend backend{pins(), fakePlatform};
	std::array<std::uint8_t, 6> pixelBuffer{};
	std::array<std::uint8_t, 3> repeatedBuffer{};
	lexxos::devices::Ili9488Buffers buffers{pixelBuffer, repeatedBuffer};
	check(buffers.valid());
	{
		lexxos::devices::Ili9488Display invalid{
		    {2, 2},
		    backend, {},
		    &queue
        };
		check(!invalid.initialize() && writes == 0 && !queue.busy());
		lexxos::devices::Ili9488Buffers overlap{pixelBuffer, pixelBuffer};
		check(!overlap.valid());
		lexxos::devices::Ili9488Buffers partial{
		    pixelBuffer, std::span<std::uint8_t>{pixelBuffer.data() + 3, 3}
        };
		check(!partial.valid());
		lexxos::devices::Ili9488Buffers tooSmall{
		    std::span<std::uint8_t>{pixelBuffer.data(), 2},
		    repeatedBuffer
        };
		check(!tooSmall.valid());
	}
	{
		lexxos::composition::detail::Ili9488Workspace first, second;
		auto a = first.buffers(), b = second.buffers();
		check(a.valid() && b.valid());
		check(a.pixels.size() == 16384U * 3 && a.repeated.size() == 4096U * 3);
		a.pixels[0] = 0xAB;
		a.repeated[0] = 0xCD;
		check(b.pixels[0] == 0 && b.repeated[0] == 0);
	}
	{
		FakePanelBackend fake;
		lexxos::devices::Ili9488Display panel{
		    {2, 2},
		    fake, buffers, &queue
        };
		check(panel.initialize() && fake.initializations == 1 && fake.transfers > 0);
		check(fake.resetEvents == std::vector<std::pair<char, std::uint32_t>>{
		          {'r',   1},
                  {'s',  10},
                  {'r',   0},
                  {'s',  10},
                  {'r',   1},
                  {'s', 200},
                  {'s', 120},
                  {'s', 120}
        });
		check(panel.configureBusSpeed(2000000) && panel.busSpeedHz() == 2000000);
		check(panel.beginFrame() && panel.beginRgb565Stream({0, 0, 1, 1}));
		std::uint16_t value = 0xF800;
		fake.fail = true;
		check(!panel.writeRgb565Stream(&value, 1));
		check(!panel.endFrame() && !fake.selected && !queue.busy());
		auto count = fake.initializations;
		lexxos::devices::Ili9488Display invalidGeometry{
		    {2, 2, -1, 0},
		    fake, buffers, &queue
        };
		check(!invalidGeometry.initialize() && fake.initializations == count);
	}
	// Форматы выбираются независимо для экземпляров в одной сборке.
	for (auto format : {lexxos::drivers::core::Ili9488PixelFormat::Rgb565,
	         lexxos::drivers::core::Ili9488PixelFormat::Rgb666}) {
		FakePanelBackend fake;
		lexxos::devices::Ili9488Panel config{2, 2};
		config.wireFormat = format;
		config.reset = {true, false, 3, 7, 11};
		lexxos::devices::Ili9488Display panel{config, fake, buffers, &queue};
		check(panel.initialize());
		check(fake.pixelFormats == std::vector<std::uint8_t>{static_cast<std::uint8_t>(format)});
		std::vector<std::pair<char, std::uint32_t>> expectedReset{
		    {'r',  0},
		    {'s',  3},
		    {'r',  1},
		    {'s',  7},
		    {'r',  0},
		    {'s', 11}
        };
		check(fake.resetEvents.size() >= expectedReset.size());
		check(std::equal(expectedReset.begin(), expectedReset.end(), fake.resetEvents.begin()));
		bool rgb565 = format == lexxos::drivers::core::Ili9488PixelFormat::Rgb565;
		check(panel.wireBytesPerPixel() == (rgb565 ? 2 : 3));
		check(panel.wireFormat() == (rgb565 ? lexxos::devices::DisplayWireFormat::Rgb565 : lexxos::devices::DisplayWireFormat::Rgb666));
		check(panel.beginFrame() && panel.beginRgb565Stream({0, 0, 1, 1}));
		fake.bytes.clear();
		std::uint16_t red = 0xF800;
		check(panel.writeRgb565Stream(&red, 1));
		check(fake.bytes == (rgb565 ? std::vector<std::uint8_t>{0xF8, 0} : std::vector<std::uint8_t>{255, 0, 0}));
		check(panel.endFrame());
		config.reset.enabled = false;
		fake.resetEvents.clear();
		lexxos::devices::Ili9488Display noReset{config, fake, buffers, &queue};
		check(noReset.initialize());
		check(fake.resetEvents == std::vector<std::pair<char, std::uint32_t>>{
		          {'s', 120},
                  {'s', 120}
        });
		config.wireFormat = static_cast<lexxos::drivers::core::Ili9488PixelFormat>(0);
		auto calls = fake.initializations;
		lexxos::devices::Ili9488Display invalidFormat{config, fake, buffers, &queue};
		check(!invalidFormat.initialize() && fake.initializations == calls && !queue.busy());
	}
	{
		FakePanelBackend fake;
		std::array<std::uint8_t, 12> area{};
		lexxos::devices::Ili9488Display packed{
		    {   2,              2},
		    fake, {area, repeatedBuffer},
		    &queue
        };

		check(packed.initialize() && packed.beginFrame());
		std::uint16_t pixels[]{0xF800, 0x07E0, 0x1234, 0x001F, 0xFFFF, 0x1234};
		fake.bytes.clear();
		auto const before = fake.transfers;
		check(packed.blitRgb565({0, 0, 2, 2}, pixels, 3));
		check(fake.transfers == before + 6); // Пять записей окна и одна всей области.
		std::vector<std::uint8_t> expected{255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255};
		check(fake.bytes.size() == 11 + expected.size());
		check(std::equal(expected.begin(), expected.end(), fake.bytes.end() - expected.size()));
		check(packed.endFrame());
	}
	lexxos::devices::Ili9488Display display{
	    {2, 2},
	    backend, buffers, &queue
    };
	{
		lexxos::devices::BusTransaction occupied{queue};
		check(!display.initialize() && writes == 0);
	}
	check(display.initialize() && !queue.busy() && !selected);

	check(display.beginFrame() && display.beginRgb565Stream({0, 0, 2, 2}));
	capture = true;
	std::uint16_t colors[]{0xF800, 0x07E0, 0x001F, 0xFFFF};
	check(display.writeRgb565Stream(colors, 4));
	auto const expected = display.wireBytesPerPixel() == 2
	    ? std::vector<std::uint8_t>{0xF8, 0, 7, 0xE0, 0, 0x1F, 255, 255}
	    : std::vector<std::uint8_t>{255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255};
	check(payload == expected);
	capture = false;
	check(display.endRgb565Stream());
	check(display.beginRgb565Stream({0, 0, 2, 2}));
	payload.clear();
	capture = true;
	std::uint8_t packed[]{0xF8, 0, 7, 0xE0, 0, 0x1F, 255, 255};
	check(display.writeRgb565BytesStream(packed, 4) && payload == expected);
	capture = false;
	check(display.endRgb565Stream());
	check(display.beginRgb565Stream({0, 0, 2, 2}));
	payload.clear();
	capture = true;
	std::uint8_t rgb[]{255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255};
	check(display.writeRgb888Stream(rgb, 4) && payload == expected);
	capture = false;
	check(display.endFrame() && !queue.busy());
	check(display.configureBusSpeed(2000000) && speed == 2000000 && !queue.busy());
	check(display.beginFrame() && queue.busy());
	check(!display.beginFrame() && queue.busy());
	{
		lexxos::devices::BusTransaction sd{lexxos::composition::products::lyraSpi1Queue()};
		check(bool(sd));
		lexxos::devices::BusTransaction competing{queue};
		check(!competing);
	}
	check(display.beginRgb565Stream({0, 0, 1, 1}) && selected);
	std::uint16_t pixel = 0;
	failNext = true;
	check(!display.writeRgb565Stream(&pixel, 1));
	check(!display.endRgb565Stream() && !selected && queue.busy());
	check(!display.endFrame() && !queue.busy());
	failNext = true;
	check(!display.initialize() && !queue.busy() && !selected);
	check(display.initialize());
	check(display.beginFrame());
	check(display.beginRgb565Stream({0, 0, 1, 1}));
	check(display.writeRgb565Stream(&pixel, 1));
	check(display.endFrame() && !selected && !queue.busy());
	{
		std::array<std::uint8_t, 3> temporaryPixels{}, temporaryRepeated{};
		lexxos::devices::LegacyIli9488Backend temporaryBackend{pins(), fakePlatform};
		lexxos::devices::Ili9488Display temporary{
		    {              2,                 2},
		    temporaryBackend, {temporaryPixels, temporaryRepeated},
		    &queue
        };
		check(temporary.initialize() && temporary.beginFrame());
		check(temporary.beginRgb565Stream({0, 0, 1, 1}));
	}
	check(!selected && !queue.busy());
	check(display.configureVerticalScroll(0, 2, 0));
	check(display.setVerticalScrollStart(-1) && !queue.busy());
	check(display.setPowerMode(lexxos::devices::DisplayPowerMode::Sleep));
	check(display.setPowerMode(lexxos::devices::DisplayPowerMode::On));
	failNext = true;
	check(!display.setPowerMode(lexxos::devices::DisplayPowerMode::Off));
	check(display.powerMode() == lexxos::devices::DisplayPowerMode::On);
	check(!selected && !queue.busy());
}
