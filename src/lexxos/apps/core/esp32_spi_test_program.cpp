#include <lexxos/apps/core/esp32_spi_test_program.h>

#include <array>
#include <cstdint>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/devices/bus/spi_bus.h>
#include <lexxos/os/app_context.h>

namespace {

void writeSpiPins(lexxos::os::AppContext& context, lexxos::platform::SpiPins const& pins) {
	context.write("spi: index=");
	lexxos::apps::writeUnsigned(context, static_cast<std::uint32_t>(pins.index));
	context.write(" sck=");
	lexxos::apps::writePin(context, pins.sck);
	context.write(" mosi=");
	lexxos::apps::writePin(context, pins.mosi);
	context.write(" miso=");
	lexxos::apps::writePin(context, pins.miso);
	context.write(" cs=");
	lexxos::apps::writePin(context, pins.cs);
	context.write(" baud=");
	lexxos::apps::writeUnsigned(context, pins.baudRate);
	context.writeLine("");
}

} // namespace

namespace lexxos::apps::system_programs {

lexxos::program::ProgramDescriptor Esp32SpiTestProgram::descriptor() {
	return {
	    .name = "esp32-spi-test",
	    .help = "initialize and smoke-test the ESP32 SPI bus",
	    .description = "Initializes the board SPI bus used by the M5Stack LCD/TF slot and sends dummy clocks with chip select inactive.",
	    .usage = "esp32-spi-test",
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int Esp32SpiTestProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	auto const& pins = activeBoard(context).sdSpi;
	if (!pins.isValid()) {
		context.writeErrorLine("esp32-spi-test: board SPI pins are not configured");
		return 1;
	}

	writeSpiPins(context, pins);

	auto& bus = lexxos::devices::sdCardSpiBus();
	if (!bus.isReady()) {
		context.writeLine("spi: initializing");
		if (!bus.initialize(pins)) {
			context.writeErrorLine("esp32-spi-test: failed to initialize SPI bus");
			return 1;
		}
	} else {
		context.writeLine("spi: already initialized");
	}

	if (!bus.setClockMode(lexxos::devices::SpiClockMode::Mode0)) {
		context.writeErrorLine("esp32-spi-test: failed to set SPI mode 0");
		return 1;
	}
	if (!bus.setBaudRate(pins.baudRate)) {
		context.writeErrorLine("esp32-spi-test: failed to set SPI baudrate");
		return 1;
	}

	context.write("spi: ready=");
	context.writeLine(bus.isReady() ? "yes" : "no");
	context.write("spi: mode=");
	lexxos::apps::writeUnsigned(context, static_cast<std::uint32_t>(bus.clockMode()));
	context.writeLine("");

	// Verify manual CS control without talking to a card while CS is asserted.
	bus.select();
	bus.deselect();
	context.writeLine("spi: cs toggle ok");

	// Send idle clocks while CS is inactive. This checks the ESP-IDF device path
	// without attempting to initialize or read the TF card.
	std::array<std::uint8_t, 10> idleClocks{};
	idleClocks.fill(0xFF);
	if (!bus.write(idleClocks.data(), idleClocks.size())) {
		context.writeErrorLine("esp32-spi-test: dummy transfer failed");
		return 1;
	}

	context.writeLine("spi: dummy transfer ok");
	return 0;
}

} // namespace lexxos::apps::system_programs
