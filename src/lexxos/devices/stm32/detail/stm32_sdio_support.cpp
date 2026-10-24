#include <lexxos/devices/stm32/detail/stm32_sdio_support.h>

#include <algorithm>

#include <lexxos/platform/platform.h>
#include <lexxos/platform/stm32/detail/stm32_gpio.h>

namespace lexxos::devices::stm32::detail {
namespace {

	constexpr std::uint32_t peripheralBase = 0x40000000U;
	constexpr std::uint32_t apb2Base = peripheralBase + 0x00010000U;
	constexpr std::uint32_t ahb1Base = peripheralBase + 0x00020000U;
	constexpr std::uint32_t rccBase = ahb1Base + 0x3800U;
	constexpr std::uint32_t sdioBase = apb2Base + 0x2C00U;

#define REG32(address) (*reinterpret_cast<volatile std::uint32_t*>(address))

#define RCC_APB2ENR REG32(rccBase + 0x44U)

#define SDIO_POWER REG32(sdioBase + 0x00U)
#define SDIO_CLKCR REG32(sdioBase + 0x04U)
#define SDIO_ARG REG32(sdioBase + 0x08U)
#define SDIO_CMD REG32(sdioBase + 0x0CU)
#define SDIO_RESP1 REG32(sdioBase + 0x14U)
#define SDIO_RESP2 REG32(sdioBase + 0x18U)
#define SDIO_RESP3 REG32(sdioBase + 0x1CU)
#define SDIO_RESP4 REG32(sdioBase + 0x20U)
#define SDIO_DTIMER REG32(sdioBase + 0x24U)
#define SDIO_DLEN REG32(sdioBase + 0x28U)
#define SDIO_DCTRL REG32(sdioBase + 0x2CU)
#define SDIO_STA REG32(sdioBase + 0x34U)
#define SDIO_ICR REG32(sdioBase + 0x38U)
#define SDIO_FIFO REG32(sdioBase + 0x80U)

	constexpr std::uint32_t rccApb2enrSdioen = 1U << 11U;
	constexpr std::uint32_t sdioClockHz = 48000000U;
	constexpr std::uint32_t sdioPowerOn = 0x03U;
	constexpr std::uint32_t sdioClkcrClken = 1U << 8U;
	constexpr std::uint32_t sdioClkcrWideBus1 = 0U << 11U;
	constexpr std::uint32_t sdioCmdCpsmen = 1U << 10U;
	constexpr std::uint32_t sdioCmdWaitRespShort = 1U << 6U;
	constexpr std::uint32_t sdioCmdWaitRespLong = 3U << 6U;

	constexpr std::uint32_t staCcrcFail = 1U << 0U;
	constexpr std::uint32_t staDcrcFail = 1U << 1U;
	constexpr std::uint32_t staCtimeout = 1U << 2U;
	constexpr std::uint32_t staDtimeout = 1U << 3U;
	constexpr std::uint32_t staTxUnderr = 1U << 4U;
	constexpr std::uint32_t staRxOverr = 1U << 5U;
	constexpr std::uint32_t staCmdRend = 1U << 6U;
	constexpr std::uint32_t staCmdSent = 1U << 7U;
	constexpr std::uint32_t staDataEnd = 1U << 8U;
	constexpr std::uint32_t staStBitErr = 1U << 9U;
	constexpr std::uint32_t staTxFifoHe = 1U << 14U;
	constexpr std::uint32_t staRxFifoHf = 1U << 15U;
	constexpr std::uint32_t staTxFifoE = 1U << 18U;
	constexpr std::uint32_t staRxDavl = 1U << 21U;

	constexpr std::uint32_t clearStaticFlags = 0x00C007FFU;
	constexpr std::uint32_t commandErrorFlags = staCtimeout | staCcrcFail;
	constexpr std::uint32_t dataErrorFlags = staDtimeout | staDcrcFail | staRxOverr | staTxUnderr | staStBitErr;
	constexpr std::uint32_t dataTimeout = 0x00FFFFFFU;
	constexpr std::uint32_t commandPollLimit = 1000000U;
	constexpr std::uint32_t dataPollLimit = 12000000U;
	constexpr std::uint32_t blockSize = 512U;
	constexpr std::uint32_t blockSizeExponent = 9U;

	void clearSdioFlags() {
		SDIO_ICR = clearStaticFlags;
	}

	void prepareSdioDataTransfer(bool read) {
		SDIO_DCTRL = 0;
		clearSdioFlags();
		SDIO_DTIMER = dataTimeout;
		SDIO_DLEN = blockSize;
		SDIO_DCTRL = (blockSizeExponent << 4U) | (read ? (1U << 1U) : 0U) | 1U;
	}

	void storeWordLe(std::uint8_t* buffer, std::size_t wordIndex, std::uint32_t value) {
		std::uint8_t* output = buffer + wordIndex * 4U;
		output[0] = static_cast<std::uint8_t>(value & 0xFFU);
		output[1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
		output[2] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
		output[3] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
	}

	std::uint32_t loadWordLe(std::uint8_t const* buffer, std::size_t wordIndex) {
		std::uint8_t const* input = buffer + wordIndex * 4U;
		return static_cast<std::uint32_t>(input[0])
		    | (static_cast<std::uint32_t>(input[1]) << 8U)
		    | (static_cast<std::uint32_t>(input[2]) << 16U)
		    | (static_cast<std::uint32_t>(input[3]) << 24U);
	}

} // namespace

bool configureSdioPeripheral(lexxos::platform::SdioPins const& pins) {
	using lexxos::platform::GpioPull;
	using lexxos::platform::stm32::detail::configureAlternateFunction;

	// CLK работает без подтяжки, CMD и линии данных удерживаются в неактивном состоянии pull-up.
	if (!configureAlternateFunction(pins.clock, 12U, GpioPull::None, 3U)
	    || !configureAlternateFunction(pins.command, 12U, GpioPull::Up, 3U)
	    || !configureAlternateFunction(pins.data0, 12U, GpioPull::Up, 3U)) {
		return false;
	}
	if (pins.data3.isValid()) {
		(void) configureAlternateFunction(pins.data3, 12U, GpioPull::Up, 3U);
	}

	RCC_APB2ENR |= rccApb2enrSdioen;
	(void) RCC_APB2ENR;

	// После подачи тактирования сбрасываем data path и все накопленные флаги предыдущей сессии.
	SDIO_POWER = sdioPowerOn;
	SDIO_CLKCR = 0;
	SDIO_DCTRL = 0;
	clearSdioFlags();
	return true;
}

bool setSdioClock(std::uint32_t requestedClockHz, std::uint32_t& activeClockHz) {
	std::uint32_t clockHz = requestedClockHz;
	if (clockHz == 0 || clockHz > 24000000U) {
		clockHz = 24000000U;
	}

	std::uint32_t divider = 0;
	if (clockHz < 24000000U) {
		divider = (sdioClockHz + clockHz - 1U) / clockHz;
		divider = divider > 2U ? divider - 2U : 0U;
		divider = std::min<std::uint32_t>(divider, 0xFFU);
	}

	SDIO_CLKCR = sdioClkcrClken | sdioClkcrWideBus1 | divider;
	activeClockHz = sdioClockHz / (divider + 2U);
	return true;
}

void configureSdioOneBitBus() {
	SDIO_CLKCR = (SDIO_CLKCR & ~(3U << 11U)) | sdioClkcrWideBus1;
}

SdioCommandResult sendSdioCommand(std::uint8_t command, std::uint32_t argument, SdioResponseType responseType) {
	clearSdioFlags();
	SDIO_ARG = argument;

	std::uint32_t commandValue = sdioCmdCpsmen | (static_cast<std::uint32_t>(command) & 0x3FU);
	if (responseType == SdioResponseType::Short || responseType == SdioResponseType::ShortNoCrc) {
		commandValue |= sdioCmdWaitRespShort;
	} else if (responseType == SdioResponseType::Long) {
		commandValue |= sdioCmdWaitRespLong;
	}
	SDIO_CMD = commandValue;

	SdioCommandResult result{};
	for (std::uint32_t poll = 0; poll < commandPollLimit; ++poll) {
		std::uint32_t const status = SDIO_STA;
		if (responseType == SdioResponseType::None) {
			if ((status & staCmdSent) != 0U) {
				result.ok = true;
				break;
			}
			if ((status & staCtimeout) != 0U) {
				break;
			}
		} else if (responseType == SdioResponseType::ShortNoCrc) {
			if ((status & (staCmdRend | staCcrcFail)) != 0U) {
				result.ok = (status & staCtimeout) == 0U;
				break;
			}
			if ((status & staCtimeout) != 0U) {
				break;
			}
		} else {
			if ((status & staCmdRend) != 0U) {
				result.ok = true;
				break;
			}
			if ((status & commandErrorFlags) != 0U) {
				break;
			}
		}
	}

	if (result.ok && responseType != SdioResponseType::None) {
		result.response[0] = SDIO_RESP1;
		if (responseType == SdioResponseType::Long) {
			result.response[1] = SDIO_RESP2;
			result.response[2] = SDIO_RESP3;
			result.response[3] = SDIO_RESP4;
		}
	}
	clearSdioFlags();
	return result;
}

void prepareSdioBlockRead() {
	prepareSdioDataTransfer(true);
}

void prepareSdioBlockWrite() {
	prepareSdioDataTransfer(false);
}

void cancelSdioDataTransfer() {
	SDIO_DCTRL = 0;
	clearSdioFlags();
}

bool finishSdioBlockRead(std::uint8_t* buffer, std::size_t size) {
	if (buffer == nullptr || size != blockSize) {
		cancelSdioDataTransfer();
		return false;
	}

	std::size_t wordsRead = 0;
	for (std::uint32_t poll = 0; poll < dataPollLimit && wordsRead < blockSize / 4U; ++poll) {
		std::uint32_t const status = SDIO_STA;
		if ((status & dataErrorFlags) != 0U) {
			cancelSdioDataTransfer();
			return false;
		}

		if ((status & staRxFifoHf) != 0U && wordsRead + 8U <= blockSize / 4U) {
			for (std::uint8_t index = 0; index < 8; ++index) {
				storeWordLe(buffer, wordsRead++, SDIO_FIFO);
			}
		} else if ((status & staRxDavl) != 0U) {
			storeWordLe(buffer, wordsRead++, SDIO_FIFO);
		}
	}

	bool dataEnded = false;
	for (std::uint32_t poll = 0; poll < dataPollLimit; ++poll) {
		std::uint32_t const status = SDIO_STA;
		if ((status & dataErrorFlags) != 0U) {
			break;
		}
		if ((status & staDataEnd) != 0U) {
			dataEnded = true;
			break;
		}
	}

	cancelSdioDataTransfer();
	return wordsRead == blockSize / 4U && dataEnded;
}

bool finishSdioBlockWrite(std::uint8_t const* buffer, std::size_t size) {
	if (buffer == nullptr || size != blockSize) {
		cancelSdioDataTransfer();
		return false;
	}

	std::size_t wordsWritten = 0;
	for (std::uint32_t poll = 0; poll < dataPollLimit && wordsWritten < blockSize / 4U; ++poll) {
		std::uint32_t const status = SDIO_STA;
		if ((status & dataErrorFlags) != 0U) {
			cancelSdioDataTransfer();
			return false;
		}
		if ((status & staTxFifoHe) != 0U) {
			for (std::uint8_t index = 0; index < 8 && wordsWritten < blockSize / 4U; ++index) {
				SDIO_FIFO = loadWordLe(buffer, wordsWritten++);
			}
		}
	}

	bool dataEnded = false;
	for (std::uint32_t poll = 0; poll < dataPollLimit; ++poll) {
		std::uint32_t const status = SDIO_STA;
		if ((status & dataErrorFlags) != 0U) {
			break;
		}
		if ((status & staDataEnd) != 0U && (status & staTxFifoE) != 0U) {
			dataEnded = true;
			break;
		}
	}

	cancelSdioDataTransfer();
	return wordsWritten == blockSize / 4U && dataEnded;
}

std::uint32_t readCsdBits(std::uint32_t const response[4], int highBit, int lowBit) {
	std::uint32_t value = 0;
	for (int bit = highBit; bit >= lowBit; --bit) {
		int const byteIndex = (127 - bit) / 8;
		int const bitInByte = bit % 8;
		std::uint8_t const byte = static_cast<std::uint8_t>((response[byteIndex / 4] >> ((3 - (byteIndex % 4)) * 8)) & 0xFFU);
		value = (value << 1U) | ((byte >> bitInByte) & 1U);
	}
	return value;
}

} // namespace lexxos::devices::stm32::detail
