#include <array>
#include <cstdlib>
#include <map>
#include <vector>

#include <lexxos/devices/rk3506/lcd_dma_controller.h>
#include <lexxos/devices/rk3506/picocalc_lcd_dma_pl330.h>
#include <lexxos/drivers/core/dma/pl330_tx_program.h>
#include <lexxos/platform/rockchip/rk3506/dma_resources.h>
#include <lexxos/platform/rockchip/rk3506/dmac0_controller.h>

extern "C" bool rk3506_lcd_dma_irq_handle(std::uint32_t);

namespace {

void check(bool condition) {
	if (!condition) {
		std::abort();
	}
}

constexpr std::uintptr_t dma = 0xFF000000U, spi = 0xFF120000U;
std::map<std::uintptr_t, std::uint32_t> registers;
std::vector<std::uint8_t> program;
std::uint32_t ticks{}, sctlr{};
unsigned launches{}, kills{}, drainWaits{};
bool deliverIrq{true};
bool injectSdFaultIrq{};
std::uintptr_t excludedSource{};
std::vector<std::uint32_t> sources;
std::size_t emittedBytes{};
unsigned ddrChecks{};
bool irqEnabled{}, pending{}, timeout{}, injectFault{};
bool ignoreKill{};
unsigned quarantineWaits{};

void complete(std::uint32_t) {
	using lexxos::platform::rk3506::lcdDmaQuarantined;
	using lexxos::platform::rk3506::lcdDmaSourceRetained;
	if (lcdDmaQuarantined()) {
		check(lcdDmaSourceRetained());
		auto const before = launches;
		auto const saved = program;
		std::uint8_t scratch[16]{};
		check(!lexxos::platform::rk3506::lcdDmaProbe());
		check(!lexxos::platform::rk3506::lcdDmaSetEnabled(false));
		check(!lexxos::platform::rk3506::lcdDmaPrepareRamReload());
		check(!lexxos::platform::rk3506::lcdDmaRegisterSource(scratch, sizeof(scratch)));
		check(!lexxos::platform::rk3506::lcdDmaWrite(scratch, sizeof(scratch)));
		check(launches == before && program == saved);
		if (++quarantineWaits == 3) {
			registers[dma + 0x110] = 0; // Hardware eventually confirms stopped.
		}

		return;
	}

	if (drainWaits > 0 && --drainWaits == 0) {
		registers[dma + 0x110] = 0;
		registers[spi + 0x024] = 0;
		registers[spi + 0x01C] = 0;
	}

	if (!pending || timeout) {
		return;
	}

	if (injectFault) {
		registers[dma + 0x034] = 4;
		return;
	}

	if (injectSdFaultIrq) {
		registers[dma + 0x034] = 3; // SD channels 0/1 on the shared abort line.
		check(!rk3506_lcd_dma_irq_handle(149));
		registers[dma + 0x034] = 0;
	}

	registers[dma + 0x028] |= 4;
	if (deliverIrq) {
		check(irqEnabled && rk3506_lcd_dma_irq_handle(148));
		check((registers[dma + 0x028] & 4) == 0);
	}

	registers[dma + 0x110] = 1;
	registers[spi + 0x024] = 1;
	registers[spi + 0x01C] = 8;
	drainWaits = 2;
	pending = false;
}

} // namespace

extern "C" std::uint32_t rk3506_timer_ticks() {
	return ++ticks;
}

extern "C" std::uint64_t rk3506_timer_counter() {
	return static_cast<std::uint64_t>(++ticks) * 24000;
}

extern "C" bool rk3506_lcd_dma_irq_configure(bool enabled) {
	irqEnabled = enabled;
	return true;
}

extern "C" std::uint32_t lcd_dma_test_read(std::uintptr_t address) {
	return registers[address];
}

extern "C" bool lcd_dma_test_ddr(std::uintptr_t address, std::size_t size) {
	++ddrChecks;
	return address && size && address != excludedSource;
}

extern "C" std::uint32_t lcd_dma_test_sctlr() {
	return sctlr;
}

extern "C" void lcd_dma_test_program(std::uint8_t const* bytes, std::size_t size) {
	program.assign(bytes, bytes + size);
	check(program[0] == 0xBC && program[1] == 0);
	std::uint32_t source{};
	for (unsigned i = 0; i < 4; ++i) {
		source |= std::uint32_t(program[2 + i]) << (8 * i);
	}

	sources.push_back(source);
	check(program.size() <= 256);
	check(program[program.size() - 4] == 0x13); // DMAWMB до DMASEV.
	check(program[program.size() - 3] == 0x34);
	check(program[program.size() - 2] == (2 << 3));
	check(program.back() == 0);
	// Интерпретируем циклы, проверяя фактическое число DMASTP и единственный SEV.
	std::uint32_t loops[2]{};
	unsigned events{}, steps{};
	emittedBytes = 0;
	for (std::size_t pc = 0; pc < program.size();) {
		check(++steps < 2000000);
		auto const op = program[pc];
		if (op == 0xBC) {
			pc += 6;
		} else if (op == 0x20 || op == 0x22) {
			loops[(op >> 1) & 1] = program[pc + 1] + 1U;
			pc += 2;
		} else if (op == 0x38 || op == 0x3C) {
			auto& left = loops[(op >> 2) & 1];
			check(left > 0);
			if (--left) {
				check(program[pc + 1] <= pc);
				pc -= program[pc + 1];
			} else {
				pc += 2;
			}
		} else if (op == 0x2B) {
			++emittedBytes;
			pc += 2;
		} else if (op == 0x34) {
			++events;
			check(program[pc + 1] == 16);
			pc += 2;
		} else if (op == 0x32 || op == 0x35) {
			check(program[pc + 1] == 0);
			pc += 2;
		} else if (op == 0x07 || op == 0x13) {
			++pc;
		} else {
			check(op == 0 && pc + 1 == program.size());
			++pc;
		}
	}

	check(events == 1);
}

extern "C" void lcd_dma_test_write(std::uintptr_t address, std::uint32_t value) {
	if (address == dma + 0x02C) {
		check(value == 4); // Не очищать события SD.
		registers[dma + 0x028] &= ~value;
	} else {
		registers[address] = value;
	}

	if (address == dma + 0xD04) {
		auto const opcode = (registers[dma + 0xD08] >> 16) & 255;
		if (opcode == 0xA2) {
			++launches;
			registers[dma + 0x110] = 1;
			pending = true;
		} else {
			check(opcode == 1);
			++kills;
			if (!ignoreKill) {
				registers[dma + 0x110] = 0;
			}

			pending = false;
		}
	}
}

extern "C" std::uint32_t dmac0_test_read(std::uintptr_t address) {
	return lcd_dma_test_read(address);
}

extern "C" void dmac0_test_write(std::uintptr_t address, std::uint32_t value) {
	lcd_dma_test_write(address, value);
}

extern "C" std::uintptr_t dmac0_test_enter() {
	return 0;
}

extern "C" void dmac0_test_leave(std::uintptr_t) {
}

namespace lexxos::platform::rk3506 {

std::uint32_t lcdSpi0ClockHz() {
	return 46875000;
}

bool lcdSpi0Initialize(std::uint32_t) {
	return true;
}

} // namespace lexxos::platform::rk3506

int main() {
	using lexxos::platform::rk3506::lcdDmaBindController;
	using lexxos::platform::rk3506::LcdDmaController;
	check(!lexxos::platform::rk3506::lcdDmaProbe());
	check(!rk3506_lcd_dma_irq_handle(148));
	static LcdDmaController owner;
	LcdDmaController unrelated;
	check(lcdDmaBindController(owner) && lcdDmaBindController(owner));
	check(!lcdDmaBindController(unrelated));
	check(!unrelated.probe() && !unrelated.setEnabled(false));
	check(!unrelated.enabled() && unrelated.statistics().chunks == 0);
#if defined(LEXXOS_RK3506_FREERTOS)
	check(!owner.probe() && !owner.setEnabled(true));
	check(!lexxos::platform::rk3506::dmac0StartNonsecure(
	    lexxos::platform::rk3506::Dmac0Client::Lcd, 2, 0x02001000));
	check(!lexxos::platform::rk3506::dmac0StartNonsecure(
	    lexxos::platform::rk3506::Dmac0Client::Sd, 0, 0x02001000));
	check(registers.empty() && launches == 0 && kills == 0);
	return 0;
#endif
	using lexxos::platform::rk3506::Dmac0Client;
	using lexxos::platform::rk3506::dmac0Kill;
	using lexxos::platform::rk3506::dmac0StartNonsecure;
	check(!dmac0StartNonsecure(Dmac0Client::Sd, 2, 0x02001000));
	check(!dmac0Kill(Dmac0Client::Lcd, 0));
	check(!dmac0Kill(static_cast<Dmac0Client>(99), 0));
	check(registers.empty() && launches == 0 && kills == 0);
	// Pure encoder: explicit controller resources, bounded output, no MMIO.
	using lexxos::drivers::pl330::buildTxProgram;
	using lexxos::drivers::pl330::maxTxBytes;
	using lexxos::drivers::pl330::txProgramCapacity;
	using lexxos::drivers::pl330::TxTransfer;
	namespace resources = lexxos::platform::rk3506::dma;
	static_assert(!resources::hasFault(0, resources::sdChannels, resources::lcdChannels));
	static_assert(!resources::hasFault(0, resources::lcdChannels, resources::sdChannels));
	static_assert(resources::hasFault(1, 0, resources::lcdChannels));
	static_assert(resources::hasFault(1, 0, resources::sdChannels));
	static_assert(resources::hasFault(0, resources::lcdChannels, resources::lcdChannels));
	static_assert(resources::hasFault(0, resources::sdChannels, resources::sdChannels));
	std::array<std::uint8_t, txProgramCapacity + 1> code{};
	code.fill(0xAA);
	TxTransfer transfer{0x02000000, 0xFF120400, 2, 2, 3, 7};
	auto const size = buildTxProgram(std::span{code}.first(txProgramCapacity), transfer);
	check(size > 0 && code.back() == 0xAA);
	check(code[19] == (3 << 3) && code[size - 2] == (7 << 3));
	check(code[14] == 3 && code[15] == 0x82 && code[16] == 0x80); // CCR width/security.
	auto const saved = code;
	check(buildTxProgram(std::span{code}.first(10), transfer) == 0 && code == saved);
	for (auto bad : {
	         TxTransfer{0, 0, 0},
	         TxTransfer{0, 0, maxTxBytes + 1},
	         TxTransfer{1, 0, 2, 2},
	         TxTransfer{0, 1, 2, 2},
	         TxTransfer{0, 0, 3, 2},
	         TxTransfer{0, 0, 4, 4},
	         TxTransfer{0, 0, 1, 1, 32},
	         TxTransfer{0, 0, 1, 1, 0, 32},
	         TxTransfer{UINT32_MAX, 0, 2}
    }) {
		check(buildTxProgram(code, bad) == 0 && code == saved);
	}

	// The encoder must program matching PL330 source/destination bursts.
	// Invalid burst sizes or partial bursts leave the microcode untouched.
	std::array<std::uint8_t, txProgramCapacity> burstCode{};
	burstCode.fill(0xAA);

	auto burstTransfer = transfer;
	burstTransfer.bytes = 64U;
	burstTransfer.burstUnits = 4U;

	auto const burstSize = buildTxProgram(burstCode, burstTransfer);
	check(burstSize != 0U);
	check(burstCode[14] == 0x33U && burstCode[15] == 0x82U && burstCode[16] == 0x8CU);

	auto const validBurstCode = burstCode;
	burstTransfer.bytes = 6U;
	check(buildTxProgram(burstCode, burstTransfer) == 0U && burstCode == validBurstCode);
	burstTransfer.bytes = 64U;
	burstTransfer.burstUnits = 3U;
	check(buildTxProgram(burstCode, burstTransfer) == 0U && burstCode == validBurstCode);

	using lexxos::platform::rk3506::lcdDmaProbe;
	using lexxos::platform::rk3506::lcdDmaSetEnabled;
	using lexxos::platform::rk3506::lcdDmaStatistics;
	using lexxos::platform::rk3506::lcdDmaWrite;
	registers[dma + 0xFE0] = 0x30;
	registers[dma + 0xFE4] = 0x13;
	registers[dma + 0xFE8] = 4;
	registers[dma + 0xE00] = 5 | (2 << 4) | (1 << 12) | (2 << 17);
	registers[dma + 0xE10] = 1;
	registers[dma + 0xE0C] = 4;
	registers[0xFF2880A8] = 1;
	registers[0xFF940000] = 1 << 4;
	lexxos::platform::rk3506::lcdDmaSetWaitFunction(complete);
	sctlr = 4;
	check(!lcdDmaProbe() && launches == 0);
	sctlr = 0;
	check(lcdDmaProbe() && launches == 1 && irqEnabled);
	check(lcdDmaSetEnabled(true));
	registers[dma + 0x020] |= 3; // Одновременное владение SD.
	registers[dma + 0x028] = 3;
	std::array<std::uint8_t, 8193> pixels{};
	sources.clear();
	check(lcdDmaWrite(pixels.data(), pixels.size()));
	check(sources.size() == 3);
	for (unsigned i = 0; i < 3; ++i) {
		check(sources[i] == static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pixels.data() + i * 4096)));
	}

	auto stats = lcdDmaStatistics();
	check(stats.chunks == 3 && stats.bytes == pixels.size() && stats.irqInterrupts == 3);
	check(stats.irqPollFallbacks == 0 && kills == 0);
	check(registers[dma + 0x020] == 3 && registers[dma + 0x028] == 3);
	check(!rk3506_lcd_dma_irq_handle(148));
	check(drainWaits == 0 && registers[spi + 0x01C] == 0);
	deliverIrq = false;
	check(lcdDmaWrite(pixels.data(), 128));
	check(lcdDmaStatistics().irqPollFallbacks == 1 && lcdDmaStatistics().irqInterrupts == 3);
	deliverIrq = true;
	excludedSource = reinterpret_cast<std::uintptr_t>(pixels.data());
	check(lcdDmaWrite(pixels.data(), 128));
	check(sources.back() != static_cast<std::uint32_t>(excludedSource));
	excludedSource = 0;
	static std::array<std::uint8_t, lexxos::platform::rk3506::lcdDmaMaxAreaBytes> area{};
	using lexxos::platform::rk3506::lcdDmaRegisterSource;
	sctlr = 4;
	check(!lcdDmaRegisterSource(area.data(), area.size()));
	sctlr = 0;
	check(lcdDmaRegisterSource(area.data(), area.size()));
	check(!lcdDmaRegisterSource(pixels.data(), pixels.size()));
	auto const checksBefore = ddrChecks;

	for (std::size_t size : {1U, 255U, 256U, 257U, 4096U, 65535U, 65536U, 65537U, 307199U, 307200U}) {
		auto const beforeLaunch = launches;
		auto const beforeIrq = lcdDmaStatistics().irqInterrupts;
		check(lcdDmaWrite(area.data(), size));
		check(launches == beforeLaunch + 1 && lcdDmaStatistics().irqInterrupts == beforeIrq + 1);
		check(emittedBytes == size && ddrChecks == checksBefore);
	}

	// Both DMA paths reject a changed memory policy even after successful probe.
	auto const beforeMemoryPolicy = launches;
	for (auto policy : {1U, 4U, 5U}) {
		sctlr = policy;
		check(!lcdDmaWrite(area.data(), 16));
		check(!lcdDmaWrite(pixels.data(), 16));
		check(launches == beforeMemoryPolicy);
	}
	sctlr = 0;
	injectSdFaultIrq = true;
	check(lcdDmaWrite(pixels.data(), 16));
	injectSdFaultIrq = false;

	alignas(2) static std::array<std::uint16_t, 320 * 320> frame{};
	using lexxos::platform::rk3506::lcdDmaWrite16;
	for (auto count : {1U, 255U, 256U, 65536U, 102400U}) {
		auto const before16 = launches;
		check(lcdDmaWrite16(frame.data(), count));
		check(launches == before16 + 1 && emittedBytes == count);
		check(program[14] == 3 && program[15] == 0x82 && program[16] == 0x80);
	}
	check(lcdDmaStatistics().direct16Chunks == 5);
	auto const beforeInvalid16 = launches;
	check(!lcdDmaWrite16(frame.data(), frame.size() + 1));
	excludedSource = reinterpret_cast<std::uintptr_t>(frame.data());
	check(!lcdDmaWrite16(frame.data(), 1));
	excludedSource = 0;
	sctlr = 4;
	check(!lcdDmaWrite16(frame.data(), 1));
	sctlr = 0;
	check(launches == beforeInvalid16);

	auto const beforeOversize = launches;
	check(!lcdDmaWrite(area.data(), area.size() + 1) && launches == beforeOversize);
	timeout = true;
	auto const before = launches;
	check(!lcdDmaWrite(pixels.data(), pixels.size()));
	check(launches == before + 1 && kills == 1 && lcdDmaStatistics().timeouts == 1);
	check(!lcdDmaWrite(pixels.data(), pixels.size()) && launches == before + 1);
	check(registers[dma + 0x020] == 3 && registers[dma + 0x028] == 3);
	timeout = false;
	check(lcdDmaProbe() && lcdDmaSetEnabled(true));
	injectFault = true;
	check(!lcdDmaWrite(pixels.data(), 4096) && kills == 2);
	check(lexxos::platform::rk3506::lcdDmaLastFailure().valid);
	check(lcdDmaSetEnabled(false) && !irqEnabled);
	injectFault = false;
	registers[dma + 0x034] = 0;
	check(lcdDmaProbe() && lcdDmaSetEnabled(true));
	ignoreKill = true;
	timeout = true;
	check(!lcdDmaWrite(pixels.data(), 128));
	check(quarantineWaits == 3 && !owner.sourceRetained() && !owner.quarantined());
	check(!owner.enabled() && owner.lastFailure().valid);
	check(unrelated.statistics().chunks == 0 && !unrelated.lastFailure().valid);
	ignoreKill = false;
	timeout = false;
	check(lcdDmaProbe() && lcdDmaSetEnabled(true));
	check(lcdDmaWrite(pixels.data(), 128) && !owner.sourceRetained());
	check(owner.prepareRamReload());
	// Fail a borrowed transfer with a successful abort to force a fresh probe.
	check(lcdDmaSetEnabled(true));
	timeout = true;
	check(!lcdDmaWrite(pixels.data(), 128));
	ignoreKill = true;
	auto const beforePreflightWaits = quarantineWaits;
	check(!lcdDmaProbe()); // Owned scratch can remain pinned without blocking boot.
	check(owner.quarantined() && owner.sourceRetained());
	check(quarantineWaits == beforePreflightWaits);
	check(!owner.prepareRamReload());
	check(!lcdDmaProbe());
	registers[dma + 0x110] = 0; // Only a confirmed stop permits reuse/reload.
	check(owner.prepareRamReload() && !owner.sourceRetained());
	ignoreKill = false;
	timeout = false;
	check(lcdDmaProbe());
}
