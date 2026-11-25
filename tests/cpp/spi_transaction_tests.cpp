#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>

#include <lexxos/devices/bus/spi_transfer.h>

namespace {

using lexxos::devices::BusTransactionQueue;
using lexxos::devices::DeviceResourcePriority;
using lexxos::devices::queuedSpiTransfer;
using lexxos::devices::SpiBusTransaction;
using lexxos::devices::SpiClockMode;
using lexxos::devices::SpiTransferStatus;

void check(bool ok) {
	if (!ok) {
		std::cerr << "SPI contract failed\n";
		std::abort();
	}
}

void waitMs(std::uint32_t ms) {
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void pending(BusTransactionQueue& queue, unsigned count) {
	for (unsigned n = 0; n < 2000 && queue.pending() != count; ++n) {
		waitMs(1);
	}
	check(queue.pending() == count);
}

class FakeSpi final : public lexxos::devices::SpiBus {
public:
	using SpiBus::SpiBus;
	bool beginOk{true}, settingOk{true}, ioOk{true}, selected{};
	unsigned begins{}, ends{};
	std::vector<int> trace;

	bool initialize(lexxos::platform::SpiPins const&) override { return true; }

	bool isReady() const override { return true; }

	bool beginTransaction() override {
		++begins;
		check(transactionBusy());
		return beginOk;
	}

	void endTransaction() override {
		check(!selected);
		++ends;
	}

	bool setBaudRate(std::uint32_t) override {
		check(transactionBusy() && !selected);
		trace.push_back(2);
		return settingOk;
	}

	bool setClockMode(SpiClockMode) override {
		check(transactionBusy() && !selected);
		trace.push_back(1);
		return settingOk;
	}

	SpiClockMode clockMode() const override { return SpiClockMode::Mode0; }

	void select() override {
		check(transactionBusy() && !selected);
		selected = true;
		trace.push_back(3);
	}

	void deselect() override {
		check(selected);
		selected = false;
		trace.push_back(5);
	}

	bool transfer(std::uint8_t const*, std::uint8_t*, std::size_t) override {
		check(selected && transactionBusy());
		trace.push_back(4);
		return ioOk;
	}

	bool write(std::uint8_t const* data, std::size_t size) override { return transfer(data, nullptr, size); }

	bool read(std::uint8_t, std::uint8_t* data, std::size_t size) override { return transfer(nullptr, data, size); }
};

} // namespace

int main() {
	// Разные CS одного контроллера разделяют очередь, другой контроллер независим.
	BusTransactionQueue queue;
	FakeSpi first{queue}, second{queue}, other;
	first.setTransactionWaitFunction(waitMs);
	SpiBusTransaction owner{first};
	check(owner.active() && second.transactionBusy());
	SpiBusTransaction separate{other};
	check(separate.active());
	std::vector<int> order;
	std::thread background([&] { SpiBusTransaction t{second, DeviceResourcePriority::Background, 3000}; check(t.active()); order.push_back(3); });
	pending(queue, 1);
	std::thread system1([&] { SpiBusTransaction t{second, DeviceResourcePriority::System, 3000}; check(t.active()); order.push_back(1); });
	pending(queue, 2);
	std::thread system2([&] { SpiBusTransaction t{first, DeviceResourcePriority::System, 3000}; check(t.active()); order.push_back(2); });
	pending(queue, 3);
	check(order.empty()); // Текущая операция не вытесняется даже System-запросом.
	owner.release();
	background.join();
	system1.join();
	system2.join();
	check((order == std::vector<int>{1, 2, 3}) && !queue.busy());
	{
		SpiBusTransaction held{first};
		SpiBusTransaction timedOut{second, DeviceResourcePriority::Critical, 2};
		check(!timedOut && queue.pending() == 0 && queue.busy());
		SpiBusTransaction moved{std::move(held)};
		check(moved.active() && !held);
		moved.release();
		moved.release();
	}

	check(!queue.busy());
	// Отказ backend не оставляет очередь занятой и не вызывает endTransaction.
	first.beginOk = false;
	unsigned ends = first.ends;
	{
		SpiBusTransaction failed{first};
		check(!failed);
	}

	check(!queue.busy() && first.ends == ends);
	first.beginOk = true;
	// Перемещение освобождает предыдущее владение ровно один раз.
	{
		SpiBusTransaction a{first};
		separate = std::move(a);
		check(!other.transactionBusy() && queue.busy() && !a);
	}

	separate.release();
	check(!queue.busy());
	// Без wait callback конкурирующий запрос возвращается сразу.
	{
		FakeSpi noWait;
		SpiBusTransaction held{noWait};
		SpiBusTransaction denied{noWait};
		check(!denied && noWait.pendingTransactions() == 0);
	}

	std::uint8_t byte{};
	for (bool write : {false, true}) {
		first.trace.clear();
		check(queuedSpiTransfer(first, SpiClockMode::Mode1, 1000000, write ? &byte : nullptr, write ? nullptr : &byte, 1) == SpiTransferStatus::Ok);
		check(first.trace == std::vector<int>{1, 2, 3, 4, 5});
	}

	first.ioOk = false;
	check(queuedSpiTransfer(first, SpiClockMode::Mode0, 1000000, &byte, &byte, 1) == SpiTransferStatus::IoError);
	check(!first.selected && !queue.busy());
	first.settingOk = false;
	first.trace.clear();
	check(queuedSpiTransfer(first, SpiClockMode::Mode0, 1000000, &byte, nullptr, 1) == SpiTransferStatus::IoError);
	check((first.trace == std::vector<int>{1}) && !queue.busy());
	{
		SpiBusTransaction held{second};
		first.trace.clear();
		check(queuedSpiTransfer(first, SpiClockMode::Mode0, 1000000, &byte, nullptr, 1, DeviceResourcePriority::Normal, 0) == SpiTransferStatus::Busy);
		check(first.trace.empty());
	}
}
