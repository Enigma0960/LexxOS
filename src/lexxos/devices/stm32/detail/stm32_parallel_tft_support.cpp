#include <lexxos/devices/stm32/detail/stm32_parallel_tft_support.h>

#include <cstddef>

#include <lexxos/platform/platform_services.h>
#include <lexxos/platform/stm32/detail/stm32_gpio.h>

namespace lexxos::devices::stm32::detail {
namespace {

	constexpr std::uint32_t peripheralBase = 0x40000000U;
	constexpr std::uint32_t ahb1Base = peripheralBase + 0x00020000U;
	constexpr std::uint32_t rccBase = ahb1Base + 0x3800U;
	constexpr std::uint32_t fsmcControlBase = 0xA0000000U;
	constexpr std::uintptr_t fsmcBank1Ne4Base = 0x6C000000U;
	constexpr std::uintptr_t fsmcAddressLine10Offset = 1U << 11U;

#define REG32(address) (*reinterpret_cast<volatile std::uint32_t*>(address))
#define REG16(address) (*reinterpret_cast<volatile std::uint16_t*>(address))

#define RCC_AHB3ENR REG32(rccBase + 0x38U)
#define FSMC_BCR4 REG32(fsmcControlBase + 0x18U)
#define FSMC_BTR4 REG32(fsmcControlBase + 0x1CU)

	constexpr std::uint32_t rccAhb3enrFsmcen = 1U << 0U;
	constexpr std::uint32_t fsmcBcrMbken = 1U << 0U;
	constexpr std::uint32_t fsmcBcrMtypSram = 0U << 2U;
	constexpr std::uint32_t fsmcBcrMwid16 = 1U << 4U;
	constexpr std::uint32_t fsmcBcrWren = 1U << 12U;

	constexpr std::uint16_t commandSoftwareReset = 0x01;
	constexpr std::uint16_t commandSleepOut = 0x11;
	constexpr std::uint16_t commandDisplayOn = 0x29;
	constexpr std::uint16_t commandColumnAddressSet = 0x2A;
	constexpr std::uint16_t commandPageAddressSet = 0x2B;
	constexpr std::uint16_t commandMemoryWrite = 0x2C;
	constexpr std::uint16_t commandMemoryAccessControl = 0x36;
	constexpr std::uint16_t commandPixelFormatSet = 0x3A;

	void writeData8(std::uint8_t data) {
		writeParallelTftData(static_cast<std::uint16_t>(data));
	}

	void writeDataSequence(std::uint8_t const* data, std::size_t size) {
		if (data == nullptr) {
			return;
		}
		for (std::size_t index = 0; index < size; ++index) {
			writeData8(data[index]);
		}
	}

	std::uint8_t highByte(std::uint16_t value) {
		return static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
	}

	std::uint8_t lowByte(std::uint16_t value) {
		return static_cast<std::uint8_t>(value & 0xFFU);
	}

	void sleepMs(lexxos::platform::PlatformServices const& platform, std::uint32_t milliseconds) {
		auto const& clock = platform.clock;
		if (clock.sleepMs != nullptr) {
			clock.sleepMs(milliseconds);
		}
	}

} // namespace

bool configureParallelTftBus(lexxos::platform::ParallelTftDisplayPins const& config) {
	using lexxos::platform::GpioPull;
	using lexxos::platform::stm32::detail::configureAlternateFunction;

	// Сначала подключаем управляющие и data-линии к AF12, только затем разрешаем FSMC.
	if (!configureAlternateFunction(config.chipSelect, 12U, GpioPull::None, 3U)
	    || !configureAlternateFunction(config.registerSelect, 12U, GpioPull::None, 3U)
	    || !configureAlternateFunction(config.read, 12U, GpioPull::None, 3U)
	    || !configureAlternateFunction(config.write, 12U, GpioPull::None, 3U)) {
		return false;
	}
	for (auto const pin : config.data) {
		if (!configureAlternateFunction(pin, 12U, GpioPull::None, 3U)) {
			return false;
		}
	}

	RCC_AHB3ENR |= rccAhb3enrFsmcen;
	(void) RCC_AHB3ENR;

	// Настраиваем NE4 как 16-битную асинхронную SRAM-шину, используемую контроллером дисплея.
	FSMC_BCR4 = 0U;
	FSMC_BTR4 = (3U << 0U) | (15U << 8U) | (1U << 16U);
	FSMC_BCR4 = fsmcBcrMbken | fsmcBcrMtypSram | fsmcBcrMwid16 | fsmcBcrWren;
	(void) FSMC_BCR4;
	return true;
}

void writeParallelTftCommand(std::uint16_t value) {
	REG16(fsmcBank1Ne4Base) = value;
}

void writeParallelTftData(std::uint16_t value) {
	REG16(fsmcBank1Ne4Base + fsmcAddressLine10Offset) = value;
}

void initializeIli9341() {
	initializeIli9341(lexxos::platform::services());
}

void initializeIli9341(lexxos::platform::PlatformServices const& platform) {
	// Команды идут в порядке из проверенной board-конфигурации: power, VCOM, pixel format, gamma, display on.
	writeParallelTftCommand(commandSoftwareReset);
	sleepMs(platform, 150);

	writeParallelTftCommand(0xCF);
	{
		std::uint8_t const data[]{0x00, 0xC1, 0x30};
		writeDataSequence(data, sizeof(data));
	}
	writeParallelTftCommand(0xED);
	{
		std::uint8_t const data[]{0x64, 0x03, 0x12, 0x81};
		writeDataSequence(data, sizeof(data));
	}
	writeParallelTftCommand(0xE8);
	{
		std::uint8_t const data[]{0x85, 0x00, 0x78};
		writeDataSequence(data, sizeof(data));
	}
	writeParallelTftCommand(0xCB);
	{
		std::uint8_t const data[]{0x39, 0x2C, 0x00, 0x34, 0x02};
		writeDataSequence(data, sizeof(data));
	}
	writeParallelTftCommand(0xF7);
	writeData8(0x20);
	writeParallelTftCommand(0xEA);
	{
		std::uint8_t const data[]{0x00, 0x00};
		writeDataSequence(data, sizeof(data));
	}
	writeParallelTftCommand(0xC0);
	writeData8(0x23);
	writeParallelTftCommand(0xC1);
	writeData8(0x10);
	writeParallelTftCommand(0xC5);
	{
		std::uint8_t const data[]{0x3E, 0x28};
		writeDataSequence(data, sizeof(data));
	}
	writeParallelTftCommand(0xC7);
	writeData8(0x86);
	writeParallelTftCommand(commandMemoryAccessControl);
	writeData8(0x28);
	writeParallelTftCommand(commandPixelFormatSet);
	writeData8(0x55);
	writeParallelTftCommand(0xB1);
	{
		std::uint8_t const data[]{0x00, 0x18};
		writeDataSequence(data, sizeof(data));
	}
	writeParallelTftCommand(0xB6);
	{
		std::uint8_t const data[]{0x08, 0x82, 0x27};
		writeDataSequence(data, sizeof(data));
	}
	writeParallelTftCommand(0xF2);
	writeData8(0x00);
	writeParallelTftCommand(0x26);
	writeData8(0x01);

	writeParallelTftCommand(0xE0);
	{
		std::uint8_t const data[]{0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00};
		writeDataSequence(data, sizeof(data));
	}
	writeParallelTftCommand(0xE1);
	{
		std::uint8_t const data[]{0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F};
		writeDataSequence(data, sizeof(data));
	}

	writeParallelTftCommand(commandSleepOut);
	sleepMs(platform, 120);
	writeParallelTftCommand(commandDisplayOn);
	sleepMs(platform, 50);
}

void setIli9341AddressWindow(Rect const& rect) {
	auto const x0 = static_cast<std::uint16_t>(rect.x);
	auto const y0 = static_cast<std::uint16_t>(rect.y);
	auto const x1 = static_cast<std::uint16_t>(rect.x + rect.width - 1);
	auto const y1 = static_cast<std::uint16_t>(rect.y + rect.height - 1);

	writeParallelTftCommand(commandColumnAddressSet);
	writeData8(highByte(x0));
	writeData8(lowByte(x0));
	writeData8(highByte(x1));
	writeData8(lowByte(x1));

	writeParallelTftCommand(commandPageAddressSet);
	writeData8(highByte(y0));
	writeData8(lowByte(y0));
	writeData8(highByte(y1));
	writeData8(lowByte(y1));

	writeParallelTftCommand(commandMemoryWrite);
}

} // namespace lexxos::devices::stm32::detail
