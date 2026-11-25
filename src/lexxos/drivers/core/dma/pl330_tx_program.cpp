#include <lexxos/drivers/core/dma/pl330_tx_program.h>

namespace lexxos::drivers::pl330 {

namespace {

	struct Code {
		std::span<std::uint8_t> output;
		std::uint8_t request;
		std::uint32_t used{};

		void emit(std::uint8_t byte) { output[used++] = byte; }

		void mov(std::uint8_t reg, std::uint32_t value) {
			emit(0xBCU); // DMAMOV
			emit(reg);
			for (unsigned shift = 0; shift < 32U; shift += 8U) {
				emit(static_cast<std::uint8_t>(value >> shift));
			}
		}

		void byteLoop(std::uint32_t count) {
			emit(0x20U); // DMALP LC0
			emit(static_cast<std::uint8_t>(count - 1));
			auto const body = used;
			emit(0x32U);
			emit(request << 3U); // DMAWFP B
			emit(0x07U); // DMALD B
			emit(0x2BU);
			emit(request << 3U); // DMASTP B
			auto const backward = used - body;
			emit(0x38U);
			emit(static_cast<std::uint8_t>(backward)); // DMALPEND LC0
		}

		void sendUnits(std::uint32_t size) {
			while (size >= 256U) {
				auto const groups = size / 256U > 256U ? 256U : size / 256U;
				emit(0x22U); // DMALP LC1: до 256 групп по 256 байт.
				emit(static_cast<std::uint8_t>(groups - 1));
				auto const body = used;
				byteLoop(256);
				auto const backward = used - body;
				emit(0x3CU);
				emit(static_cast<std::uint8_t>(backward)); // DMALPEND LC1
				size -= groups * 256U;
			}

			if (size) {
				byteLoop(size);
			}
		}
	};

} // namespace

std::size_t buildTxProgram(std::span<std::uint8_t> output, TxTransfer const& transfer) {
	if (
	    output.size() < txProgramCapacity
	    || transfer.bytes == 0
	    || transfer.bytes > maxTxBytes
	    || (transfer.unitBytes != 1 && transfer.unitBytes != 2)
	    || (transfer.burstUnits != 1U && transfer.burstUnits != 2U && transfer.burstUnits != 4U)
	    || transfer.bytes % (transfer.unitBytes * transfer.burstUnits) != 0
	    || transfer.source % transfer.unitBytes != 0
	    || transfer.destination % transfer.unitBytes != 0
	    || transfer.request >= 32
	    || transfer.event >= 32
	    || transfer.bytes - 1U > UINT32_MAX - transfer.source) {
		return 0;
	}

	static_assert(20U + ((maxTxBytes + 65535U) / 65536U) * 13U + 9U + 4U <= txProgramCapacity);
	Code code{output, transfer.request};
	code.mov(0U, transfer.source);
	code.mov(2U, transfer.destination);

	auto const width = transfer.unitBytes == 2U ? ((1U << 1U) | (1U << 15U)) : 0U;
	// PL330 DMACCR: src burst length [7:4], dst burst length [21:18].
	// Both sides must describe the same number of peripheral data beats.
	auto const burst = static_cast<std::uint32_t>(transfer.burstUnits - 1U);
	code.mov(1U, (1U << 9U) | (1U << 23U) | 1U | width | (burst << 4U) | (burst << 18U));
	code.emit(0x35U); // DMAFLUSHP
	code.emit(transfer.request << 3U);

	// One DMAWFP B / DMALD B / DMASTP B iteration now transfers a burst.
	code.sendUnits(transfer.bytes / (transfer.unitBytes * transfer.burstUnits));
	code.emit(0x13U); // DMAWMB
	code.emit(0x34U); // DMASEV
	code.emit(transfer.event << 3U);
	code.emit(0x00U); // DMAEND
	return code.used;
}

} // namespace lexxos::drivers::pl330
