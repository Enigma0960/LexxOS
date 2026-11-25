#pragma once

#include <lexxos/devices/bus/bus_transaction.h>
#include <lexxos/devices/bus/bus_transaction_queue.h>
#include <lexxos/devices/storage/sd_media_transport.h>

namespace lexxos::platform::rk3506 {

class SdProtocol;

} // namespace lexxos::platform::rk3506

namespace lexxos::devices::rk3506 {

// Адаптер единственного физического SPI1; не создаёт второй контроллер.
// Очередь и протокол принадлежат composition и живут дольше адаптера.
class NativePicoSdTransport final : public SdMediaTransport {
public:
	NativePicoSdTransport(BusTransactionQueue& queue, platform::rk3506::SdProtocol& protocol);

	// Диагностика использует уже захваченную очередь без рекурсивного acquire.
	bool prepareDiagnostics(BusTransaction const& transaction);
	bool readForDiagnostics(BusTransaction const& transaction, std::uint64_t lba, std::uint8_t* data, std::size_t count);
	bool ready() override;
	bool inserted() override;
	bool probe() override;
	char const* failureStage() override;
	std::uint8_t lastResponse() override;
	void invalidateRemoved() override;
	std::uint64_t sectorCount() override;
	bool read(std::uint32_t lba, std::uint8_t* data, std::size_t count) override;
	bool write(std::uint32_t lba, std::uint8_t const* data, std::size_t count) override;
	bool configured() override;
	bool highCapacity() override;
	std::uint32_t clockMHz() override;
	bool setClockMHz(std::uint32_t value) override;
	bool dmaAvailable() override;
	bool dmaEnabled() override;

private:
	void finishRemoval();
	BusTransactionQueue& _queue;
	platform::rk3506::SdProtocol& _protocol;
	bool _removalPending{false};
};

} // namespace lexxos::devices::rk3506
