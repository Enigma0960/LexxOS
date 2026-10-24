#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include <hardware/dma.h>
#include <hardware/spi.h>
#include <pico/sem.h>

#include <lexxos/devices/bus/spi_bus.h>

#if defined(LEXXOS_KERNEL_FREERTOS)
	#include <FreeRTOS.h>
	#include <semphr.h>
#endif

namespace lexxos::devices {

class SpiBusMutex {
public:
	[[nodiscard]] bool initialize() {
#if defined(LEXXOS_KERNEL_FREERTOS)
		if (_mutex == nullptr) {
			_mutex = xSemaphoreCreateRecursiveMutex();
		}
		return _mutex != nullptr;
#else
		return true;
#endif
	}

	[[nodiscard]] bool lock() {
#if defined(LEXXOS_KERNEL_FREERTOS)
		if (_mutex == nullptr) {
			return false;
		}
		return xSemaphoreTakeRecursive(_mutex, portMAX_DELAY) == pdTRUE;
#else
		if (_depth == 0) {
			while (_locked.test_and_set(std::memory_order_acquire)) {
			}
		}
		++_depth;
		return true;
#endif
	}

	void unlock() {
#if defined(LEXXOS_KERNEL_FREERTOS)
		if (_mutex != nullptr) {
			(void) xSemaphoreGiveRecursive(_mutex);
		}
#else
		if (_depth == 0) {
			return;
		}
		--_depth;
		if (_depth == 0) {
			_locked.clear(std::memory_order_release);
		}
#endif
	}

private:
#if defined(LEXXOS_KERNEL_FREERTOS)
	SemaphoreHandle_t _mutex{nullptr};
#else
	std::atomic_flag _locked = ATOMIC_FLAG_INIT;
	std::uint32_t _depth{0};
#endif
};

class SpiBusGuard {
public:
	explicit SpiBusGuard(SpiBusMutex& mutex)
	    : _mutex(mutex)
	    , _locked(_mutex.lock()) {
	}

	~SpiBusGuard() {
		if (_locked) {
			_mutex.unlock();
		}
	}

	[[nodiscard]] bool locked() const {
		return _locked;
	}

private:
	SpiBusMutex& _mutex;
	bool _locked{false};
};

class PicoSpiBus final : public SpiBus {
public:
	~PicoSpiBus() override;

	bool initialize(lexxos::platform::SpiPins const& pins) override;
	[[nodiscard]] bool isReady() const override;
	bool setBaudRate(std::uint32_t baudRate) override;
	bool setClockMode(SpiClockMode mode) override;
	[[nodiscard]] SpiClockMode clockMode() const override;
	void select() override;
	void deselect() override;
	bool transfer(std::uint8_t const* tx, std::uint8_t* rx, std::size_t size) override;
	bool write(std::uint8_t const* data, std::size_t size) override;
	bool read(std::uint8_t fill, std::uint8_t* data, std::size_t size) override;
	[[nodiscard]] bool supportsDma() const override;
	bool startDmaTransfer(std::uint8_t const* tx, std::uint8_t* rx, std::size_t size) override;
	bool waitDmaTransfer(std::uint32_t timeoutMs) override;
	bool beginTransaction() override;
	void endTransaction() override;

	bool handleDmaInterrupt(std::uint32_t irqIndex);

private:
	void releaseDmaBusLock();
	void applyClockMode(SpiClockMode mode);
	bool ensureDmaReady();
	void configureDmaDreq();
	void clearDmaInterruptStatus();
	[[nodiscard]] bool spiIsFaulted() const;

	spi_inst_t* _instance{nullptr};
	lexxos::platform::SpiPins _pins;
	SpiClockMode _mode{SpiClockMode::Mode0};
	bool _ready{false};
	bool _dmaReady{false};
	bool _dmaTransferActive{false};
	bool _dmaBusLocked{false};
	std::uint32_t _selectLockDepth{0};
	int _txDmaChannel{-1};
	int _rxDmaChannel{-1};
	dma_channel_config _txDmaConfig{};
	dma_channel_config _rxDmaConfig{};
	semaphore_t _dmaDone{};
	std::uint8_t _dmaFill{0xFF};
	std::uint8_t _dmaDiscard{0};
	SpiBusMutex _mutex;
};

} // namespace lexxos::devices
