#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/hardware/products/dev_pico_display.h>
#include <lexxos/hardware/products/dev_pico_input.h>
#include <lexxos/hardware/products/dev_pico_policy.h>
#include <lexxos/hardware/products/dev_pico_product.h>
#include <lexxos/hardware/products/dev_pico_storage.h>
#include <lexxos/platform/board_legacy.h>

namespace lexxos::hardware::legacy {

namespace dev_pico_detail {

	constexpr void applyInterfaces(platform::LegacyBoardDescriptor& result) {
		using namespace products::dev_pico;
		result.debugUart = {
		    .index = debugUart.controller.index,
		    .tx = {debugUart.tx.number},
		    .rx = {debugUart.rx.number},
		    .baudRate = debugUartBaudRate,
		};
		result.debugOledI2c = {
		    .index = debugI2c.controller.index,
		    .sda = {debugI2c.sda.number},
		    .scl = {debugI2c.scl.number},
		    .baudRate = debugI2cBaudRate,
		};
	}

	constexpr void applyStorage(platform::LegacyBoardDescriptor& result) {
		using namespace products::dev_pico;
		result.sdInterface = platform::SdInterface::Spi;
		result.sdSpi = {
		    .index = storageSpi.controller.index,
		    .sck = {storageSpi.clock.number},
		    .mosi = {storageSpi.mosi.number},
		    .miso = {storageSpi.miso.number},
		    .cs = {sdCard.endpoint.chipSelect.number},
		    .baudRate = storageSpiBaudRate,
		};
		result.sdCardDetect = {
		    .detect = {sdCard.cardDetect.number},
		    .insertedLevel = sdCard.detectLowMeansInserted
		        ? platform::GpioActiveLevel::Low
		        : platform::GpioActiveLevel::High,
		};
		result.psram = {
		    .spi = {
		            .index = peripheralSpi.controller.index,
		            .sck = {peripheralSpi.clock.number},
		            .mosi = {peripheralSpi.mosi.number},
		            .miso = {peripheralSpi.miso.number},
		            .cs = {psram.endpoint.chipSelect.number},
		            .baudRate = psramSpiBaudRate,
		            },
		    .sio2 = {psram.sio2.number},
		    .sio3 = {psram.sio3.number},
		};
	}

	constexpr void applyDisplay(platform::LegacyBoardDescriptor& result, std::uint32_t displaySpiHz) {
		using namespace products::dev_pico;
		result.spiTftDisplay = {
		    .enabled = true,
		    .spiIndex = display.endpoint.controller.index,
		    .spiBaudRate = displaySpiHz,
		    .sck = {peripheralSpi.clock.number},
		    .mosi = {peripheralSpi.mosi.number},
		    .cs = {display.endpoint.chipSelect.number},
		    .dc = {display.dataCommand.number},
		    .reset = {display.reset.number},
		    .backlight = {display.backlight.number},
		    .width = display.width,
		    .height = display.height,
		    .invertColors = display.invertColors,
		};
	}

	constexpr void applyInput(platform::LegacyBoardDescriptor& result) {
		using namespace products::dev_pico;
		result.keyboardExpander = {
		    .enabled = true,
		    .name = keyboardExpander.name,
		    .i2cIndex = keyboardExpander.bus.index,
		    .address = keyboardExpander.address,
		    .interrupt = {keyboardExpander.interrupt.number},
		    .interruptActiveLevel = platform::GpioActiveLevel::Low,
		};
		result.keyboard = {
		    .enabled = true,
		    .name = keyboard.name,
		    .expanderName = keyboard.expanderName,
		    .rowCount = static_cast<std::uint8_t>(keyboard.rows.size()),
		    .columnCount = static_cast<std::uint8_t>(keyboard.columns.size()),
		    .rowActiveLevel = platform::GpioActiveLevel::Low,
		    .columnActiveLevel = platform::GpioActiveLevel::Low,
		    .scanPeriodMs = keyboardScanMs,
		    .debounceMs = keyboardDebounceMs,
		};
		for (std::size_t i = 0; i < keyboard.rows.size(); ++i) {
			result.keyboard.rows[i] = keyboard.rows[i];
		}

		for (std::size_t i = 0; i < keyboard.columns.size(); ++i) {
			result.keyboard.columns[i] = keyboard.columns[i];
		}

		result.segaGamepad = {
		    .enabled = true,
		    .name = segaGamepad.name,
		    .expanderName = segaGamepad.expanderName,
		    .up = segaGamepad.up,
		    .down = segaGamepad.down,
		    .left = segaGamepad.left,
		    .right = segaGamepad.right,
		    .buttonBa = segaGamepad.buttonBa,
		    .buttonCStart = segaGamepad.buttonCStart,
		    .select = {segaGamepad.select.number},
		};
	}

} // namespace lexxos::hardware::legacy::dev_pico_detail

// Единственная проекция проверенных деклараций dev_pico в старый формат.
// Новые устройства добавляются в hardware/components и products, не сюда.
[[nodiscard]] constexpr platform::LegacyBoardDescriptor devPicoBoard(std::uint32_t displaySpiHz) {
	using namespace products::dev_pico;
	platform::LegacyBoardDescriptor result{};
	result.boardId = id;
	result.boardName = name;
	result.vendor = vendor;
	result.processorFamily = platform::ProcessorFamily::Rp2040;
	result.processorName = processor;
	result.statusLed = {statusLed.number};
	dev_pico_detail::applyInterfaces(result);
	dev_pico_detail::applyStorage(result);
	dev_pico_detail::applyDisplay(result, displaySpiHz);
	dev_pico_detail::applyInput(result);

	return result;
}

// Контракты legacy не меняются, включая controller/madctl/offset по умолчанию.
static_assert(devPicoBoard(25000000).sdSpi.cs.number == 17);
static_assert(devPicoBoard(25000000).spiTftDisplay.spiBaudRate == 25000000);
static_assert(devPicoBoard(25000000).spiTftDisplay.controller == platform::SpiTftController::Ili934x);
static_assert(devPicoBoard(25000000).keyboard.rows[5] == 13);
static_assert(devPicoBoard(25000000).segaGamepad.buttonCStart == 15);
static_assert(std::string_view{devPicoBoard(25000000).boardId} == "dev_pico");

} // namespace lexxos::hardware::legacy
