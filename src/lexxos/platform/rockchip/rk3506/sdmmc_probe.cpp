#include <lexxos/platform/rockchip/rk3506/sdmmc_probe.h>

/* RK3506 SDMMC bring-up: explicit, read-only DesignWare MSHC diagnostics.
 * No filesystem mount, DMA, SDMMC interrupts, or card write commands.
 * The U-Boot pinmux, CRU parent clock, and card power rail are inherited.
 * Do not issue `sdmmc probe` until the target is booted from microSD.
 */
#include <cstddef>
#include <cstdint>

namespace lexxos::platform::rk3506 {

void sdmmcInvalidateValidatedDisk();

namespace {

	constexpr std::uintptr_t kBase = 0xFF480000U;
	constexpr std::uint32_t kSectorBytes = 512;
	constexpr std::uint32_t kInitDivider = 255; // Conservative: <=400 kHz for CIU parents <=200 MHz.

	// RK3506 TRM and U-Boot DesignWare MSHC register definitions.
	enum Register : std::uintptr_t {
		Ctrl = 0x000,
		PowerEnable = 0x004,
		ClockDivider = 0x008,
		ClockSource = 0x00C,
		ClockEnable = 0x010,
		Timeout = 0x014,
		CardType = 0x018,
		BlockSize = 0x01C,
		ByteCount = 0x020,
		InterruptMask = 0x024,
		CommandArg = 0x028,
		Command = 0x02C,
		Response0 = 0x030,
		InterruptStatus = 0x044,
		Status = 0x048,
		FifoThreshold = 0x04C,
		CardDetect = 0x050,
		Version = 0x06C,
		HostConfig = 0x070,
		BusMode = 0x080,
		IdmaInterruptEnable = 0x090,
		Data = 0x200,
	};

	constexpr std::uint32_t kCtrlReset = 0x7U;
	constexpr std::uint32_t kStart = 1U << 31U;
	constexpr std::uint32_t kWaitPreviousData = 1U << 13U;
	constexpr std::uint32_t kSendInitialization = 1U << 15U;
	constexpr std::uint32_t kUpdateClock = 1U << 21U;
	constexpr std::uint32_t kResponseExpected = 1U << 6U;
	constexpr std::uint32_t kResponseLong = 1U << 7U;
	constexpr std::uint32_t kResponseCrc = 1U << 8U;
	constexpr std::uint32_t kReadData = 1U << 9U;
	constexpr std::uint32_t kCommandDone = 1U << 2U;
	constexpr std::uint32_t kDataDone = 1U << 3U;
	constexpr std::uint32_t kErrorMask =
	    (1U << 1U) | (1U << 7U) | (1U << 8U) | (1U << 9U) | (1U << 10U) | (1U << 11U) | (1U << 12U) | (1U << 13U) | (1U << 15U);

	extern "C" std::uint32_t rk3506_timer_ticks();

	std::uint32_t g_rca{};
	std::uint32_t g_ocr{};
	std::uint32_t g_lastInterrupt{};
	std::uint32_t g_lastCommand{};
	bool g_ready{};

	std::uint32_t read(Register reg) {
		return *reinterpret_cast<std::uint32_t volatile*>(kBase + reg);
	}

	void write(Register reg, std::uint32_t value) {
		*reinterpret_cast<std::uint32_t volatile*>(kBase + reg) = value;
	}

	bool timedOut(std::uint32_t start, std::uint32_t ms) {
		return static_cast<std::uint32_t>(rk3506_timer_ticks() - start) >= ms;
	}

	bool waitClear(Register reg, std::uint32_t mask, std::uint32_t timeoutMs) {
		auto const start = rk3506_timer_ticks();
		do {
			if ((read(reg) & mask) == 0) {
				return true;
			}
		} while (!timedOut(start, timeoutMs));

		return false;
	}

	bool updateClock() {
		write(Command, kStart | kWaitPreviousData | kUpdateClock);
		return waitClear(Command, kStart, 200);
	}

	bool initializeController() {
		// Never assume the handoff clock rate: leave CRU/pinmux untouched.
		// Stop DMA and use only the FIFO. All timeout paths are bounded.
		write(InterruptMask, 0);
		write(IdmaInterruptEnable, 0);
		write(BusMode, 0);
		write(Ctrl, kCtrlReset);
		if (!waitClear(Ctrl, kCtrlReset, 200)) {
			return false;
		}

		write(ClockEnable, 0);
		write(ClockSource, 0);
		write(ClockDivider, kInitDivider);
		if (!updateClock()) {
			return false;
		}

		write(ClockEnable, 1);
		if (!updateClock()) {
			return false;
		}

		write(CardType, 0); // 1-bit SD bus until wider transfer is validated.
		write(Timeout, 0xFFFFFFFFU);
		write(FifoThreshold, (2U << 28U) | (7U << 16U) | 8U);
		write(PowerEnable, 1);
		write(InterruptStatus, 0xFFFFFFFFU);
		return true;
	}

	// R1/R3/R6/R7/R2 flags are specific to the DW-MSHC command register.
	bool send(std::uint8_t index, std::uint32_t argument, bool response,
	    bool crc, bool longResponse, bool data, std::uint32_t& value) {
		g_lastCommand = index;
		auto const start = rk3506_timer_ticks();
		while ((read(Status) & (1U << 9U)) != 0U) {
			if (timedOut(start, 500)) {
				return false;
			}
		}

		write(InterruptStatus, 0xFFFFFFFFU);
		if (data) {
			write(Ctrl, read(Ctrl) | (1U << 1U)); // Reset FIFO only.
			if (!waitClear(Ctrl, 1U << 1U, 200)) {
				return false;
			}

			write(BlockSize, kSectorBytes);
			write(ByteCount, kSectorBytes);
		}

		write(CommandArg, argument);
		std::uint32_t flags = kStart | kWaitPreviousData | index;
		if (index == 0) {
			flags |= kSendInitialization;
		}

		if (response) {
			flags |= kResponseExpected;
		}

		if (crc && response) {
			flags |= kResponseCrc;
		}

		if (longResponse) {
			flags |= kResponseLong;
		}

		if (data) {
			flags |= kReadData;
		}

		write(Command, flags);
		auto const commandStart = rk3506_timer_ticks();
		while (true) {
			auto const status = read(InterruptStatus);
			g_lastInterrupt = status;
			if (status & kErrorMask) {
				return false;
			}

			if (status & kCommandDone) {
				break;
			}

			if (timedOut(commandStart, 500)) {
				return false;
			}
		}

		value = response ? read(Response0) : 0;
		// Do not clear CMD_DONE before the data reader has had a chance to inspect
		// the FIFO and any possible data error. Next command clears it.
		return true;
	}

	bool command(std::uint8_t index, std::uint32_t arg,
	    std::uint32_t& response, bool checkCrc = true, bool longResp = false) {
		return send(index, arg, index != 0, checkCrc, longResp, false, response);
	}

	bool probe() {
		sdmmcInvalidateValidatedDisk(); // Re-probe revokes prior GPT validation.

		g_ready = false;
		g_rca = 0;
		g_ocr = 0;

		// A definitely unmapped/clock-gated register must not be treated as a card.
		auto const version = read(Version);
		if (version == 0 || version == 0xFFFFFFFFU) {
			return false;
		}

		if (!initializeController()) {
			return false;
		}

		std::uint32_t response = 0;
		if (!command(0, 0, response)) {
			return false;
		}

		if (!command(8, 0x1AAU, response) || (response & 0xFFFU) != 0x1AAU) {
			return false; // Stage 8 supports only SD 2.0+; no MMC/SD-v1 fallback.
		}

		auto const start = rk3506_timer_ticks();
		do {
			if (!command(55, 0, response)) {
				return false;
			}

			// R3/OCR has no response CRC in SD protocol.
			if (!command(41, 0x40FF8000U, response, false)) {
				return false;
			}

			g_ocr = response;
			if (response & (1U << 31U)) {
				break;
			}
		} while (!timedOut(start, 2000));

		if (!(g_ocr & (1U << 31U))) {
			return false;
		}

		if (!command(2, 0, response, true, true)) {
			return false;
		}

		if (!command(3, 0, response)) {
			return false;
		}

		g_rca = response & 0xFFFF0000U;
		if (g_rca == 0) {
			return false;
		}

		if (!command(7, g_rca, response)) {
			return false;
		}

		if (!(g_ocr & (1U << 30U))) {
			if (!command(16, kSectorBytes, response)) {
				return false;
			}
		}

		g_ready = true;
		return true;
	}

	bool readSector(std::uint32_t lba, std::uint8_t* dest) {
		if (!g_ready || !dest || lba > 0x007FFFFFU) {
			return false;
		}

		std::uint32_t response = 0;
		std::uint32_t const address = (g_ocr & (1U << 30U)) ? lba : lba * kSectorBytes;
		if (!send(17, address, true, true, false, true, response)) {
			return false;
		}

		auto const start = rk3506_timer_ticks();
		std::size_t words = 0;
		while (true) {
			auto const irq = read(InterruptStatus);
			g_lastInterrupt = irq;
			if (irq & kErrorMask) {
				return false;
			}

			auto count = (read(Status) >> 17U) & 0x1FFFU;
			while (count != 0 && words < kSectorBytes / 4) {
				auto const value = read(Data);
				dest[words * 4] = static_cast<std::uint8_t>(value);
				dest[words * 4 + 1] = static_cast<std::uint8_t>(value >> 8U);
				dest[words * 4 + 2] = static_cast<std::uint8_t>(value >> 16U);
				dest[words * 4 + 3] = static_cast<std::uint8_t>(value >> 24U);
				++words;
				--count;
			}

			if ((irq & kDataDone) && words == kSectorBytes / 4) {
				return true;
			}

			if ((irq & kDataDone) && words != kSectorBytes / 4 && count == 0) {
				return false;
			}

			if (timedOut(start, 1000)) {
				return false;
			}
		}
	}

} // namespace

// Share one tested CMD17 implementation between diagnostics and BlockDevice.
bool sdmmcReady() {
	return g_ready;
}

bool sdmmcReadSector(std::uint32_t lba, std::uint8_t* destination) {
	return readSector(lba, destination);
}

SdmmcStatus sdmmcStatus() {
	return {
	    .version = read(Version),
	    .hostConfig = read(HostConfig),
	    .control = read(Ctrl),
	    .powerEnable = read(PowerEnable),
	    .clockDivider = read(ClockDivider),
	    .clockEnable = read(ClockEnable),
	    .cardType = read(CardType),
	    .cardDetect = read(CardDetect),
	    .status = read(Status),
	    .interruptStatus = read(InterruptStatus),
	    .ready = g_ready,
	};
}

SdmmcResult sdmmcResult() {
	return {g_rca, g_ocr, g_lastInterrupt, g_lastCommand};
}

bool sdmmcProbe() {
	return probe();
}

} // namespace lexxos::platform::rk3506
