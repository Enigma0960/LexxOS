#include <cstdlib>
#include <iostream>
#include <string_view>

#include <lexxos/hardware/legacy/picocalc_lyra_board.h>

namespace {

// Frozen pre-migration descriptor; do not derive expected values from new data.
constexpr lexxos::platform::LegacyBoardDescriptor originalBoard{
    .boardId = "picocalc_lyra",
    .boardName = "PicoCalc / Luckfox Lyra",
    .vendor = "Luckfox",
    .processorFamily = lexxos::platform::ProcessorFamily::Rk3506,
    .processorName = "Rockchip RK3506G2 (Cortex-A7, CPU0)",
    // Board descriptor records the boot default; `debug` controls the
    // active console route at runtime (UART1 is nominally 1500000 baud).
    .interfaces = {
                   .consoleUart = {.index = 0, .baudRate = 1500000},
                   // PicoCalc STM32: I2C2 wiring, GPIO0_B3 SDA / GPIO0_B2 SCL.
        .systemI2c = {.index = 2, .sda = {11}, .scl = {10}, .baudRate = 100000},
                   },
    // GPIO0_A2..A4 = RESET/DC/CS; RM_IO6/7 = SPI0 MOSI/CLK.
    .spiTftDisplay = {
                   .enabled = true,
                   .spiIndex = 0,
                   .spiBaudRate = 23437500,
                   .sck = {7},
                   .mosi = {6},
                   .cs = {4},
                   .dc = {3},
                   .reset = {2},
                   .width = 320,
                   .height = 320,
                   .invertColors = true,
                   },
    // No auto-detection or write access to the Luckfox boot SDMMC.
    .storageSlots = {
                   {.deviceName = "sd0", .mountPath = nullptr, .role = lexxos::platform::SdMediaRole::System, .hotSwappable = false, .autoMount = false, .readOnlyByDefault = true},
                   {
            .deviceName = "sd1",
            .mountPath = "/mnt/sd",
            .role = lexxos::platform::SdMediaRole::Removable,
            .hotSwappable = true,
            .autoMount = true,
            // Card write + CRC16 readback and FAT32 operations are verified.
            // Applies only to the removable PicoCalc socket, NEVER boot sd0.
            .readOnlyByDefault = false,
        },
                   },
    .storageSlotCount = 2,
};

void expect(bool value, char const* message) {
	if (!value) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

} // namespace

int main() {
	using lexxos::hardware::legacy::picoCalcLyraBoard;
	using lexxos::hardware::products::hasValidDistinctPins;
	using lexxos::hardware::products::LyraPinDomain;
	constexpr auto migrated = picoCalcLyraBoard(true);
	constexpr auto identity = lexxos::platform::boardIdentity(migrated);
	static_assert(std::string_view{identity.boardId} == "picocalc_lyra");
	static_assert(identity.processorFamily == lexxos::platform::ProcessorFamily::Rk3506);
	static_assert(migrated.interfaces.systemI2c.sda.number == 11);
	static_assert(migrated.spiTftDisplay.sck.number == 7);
	expect(std::string_view{migrated.boardId} == originalBoard.boardId
	        && std::string_view{migrated.boardName} == originalBoard.boardName
	        && std::string_view{migrated.vendor} == originalBoard.vendor
	        && std::string_view{migrated.processorName} == originalBoard.processorName
	        && migrated.processorFamily == originalBoard.processorFamily,
	    "identity changed");
	auto const& uart = migrated.interfaces.consoleUart;
	expect(uart.index == 0 && uart.baudRate == 1500000 && uart.tx.number == -1 && uart.rx.number == -1, "boot UART changed");
	auto const& i2c = migrated.interfaces.systemI2c;
	expect(i2c.index == 2 && i2c.baudRate == 100000 && i2c.sda.number == 11 && i2c.scl.number == 10, "southbridge wiring changed");
	auto const& display = migrated.spiTftDisplay;
	auto const& originalDisplay = originalBoard.spiTftDisplay;
	expect(display.enabled && display.spiIndex == 0 && display.spiBaudRate == 23437500 && display.sck.number == 7 && display.mosi.number == 6 && display.cs.number == 4 && display.dc.number == 3 && display.reset.number == 2 && display.backlight.number == -1 && display.width == 320 && display.height == 320 && display.invertColors && display.controller == originalDisplay.controller && display.madctl == originalDisplay.madctl && display.addressWindowSwapsAxes == originalDisplay.addressWindowSwapsAxes && display.columnOffset == 0 && display.rowOffset == 0, "LCD compatibility changed");
	expect(migrated.storageSlotCount == 2, "storage count changed");
	for (unsigned i = 0; i < migrated.storageSlotCount; ++i) {
		auto const& actual = migrated.storageSlots[i];
		auto const& expected = originalBoard.storageSlots[i];
		expect(std::string_view{actual.deviceName} == expected.deviceName
		        && actual.role == expected.role
		        && actual.hotSwappable == expected.hotSwappable
		        && actual.autoMount == expected.autoMount
		        && actual.readOnlyByDefault == expected.readOnlyByDefault,
		    "storage policy changed");
		expect(expected.mountPath == nullptr ? actual.mountPath == nullptr
		                                     : actual.mountPath != nullptr && std::string_view{actual.mountPath} == expected.mountPath,
		    "mount path changed");
	}

	constexpr auto disabled = picoCalcLyraBoard(false);
	expect(!disabled.spiTftDisplay.enabled && disabled.spiTftDisplay.spiIndex == -1 && disabled.spiTftDisplay.sck.number == -1 && disabled.spiTftDisplay.width == 128 && disabled.spiTftDisplay.spiBaudRate == 16000000 && disabled.storageSlotCount == 2, "disabled display defaults changed");
	static_assert(picoCalcLyraBoard(true, 12000000).spiTftDisplay.spiBaudRate == 12000000);
	auto wiring = lexxos::hardware::products::picoCalcLyraWiring;
	expect(hasValidDistinctPins(wiring), "default wiring rejected");
	wiring.displayDc = wiring.displayCs;
	expect(!hasValidDistinctPins(wiring), "duplicate pin accepted");
	wiring = lexxos::hardware::products::picoCalcLyraWiring;
	wiring.displayDc.number = -1;
	expect(!hasValidDistinctPins(wiring), "negative pin accepted");
	wiring.displayDc.number = 32;
	expect(!hasValidDistinctPins(wiring), "out-of-range pin accepted");
	wiring.displayDc = {static_cast<LyraPinDomain>(255), 3};
	expect(!hasValidDistinctPins(wiring), "unknown namespace accepted");
	return 0;
}
