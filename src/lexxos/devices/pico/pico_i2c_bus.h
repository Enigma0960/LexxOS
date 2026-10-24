#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include <hardware/i2c.h>

#include <lexxos/devices/bus/i2c_bus.h>

#if defined(LEXXOS_KERNEL_FREERTOS)
	#include <FreeRTOS.h>
	#include <semphr.h>
#endif

namespace lexxos::devices {

class I2cBusMutex {
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
		return xSemaphoreTakeRecursive(_mutex, pdMS_TO_TICKS(100)) == pdTRUE;
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

class I2cBusGuard {
public:
	explicit I2cBusGuard(I2cBusMutex& mutex)
	    : _mutex(mutex)
	    , _locked(_mutex.lock()) {
	}

	~I2cBusGuard() {
		if (_locked) {
			_mutex.unlock();
		}
	}

	[[nodiscard]] bool locked() const {
		return _locked;
	}

private:
	I2cBusMutex& _mutex;
	bool _locked{false};
};

class PicoI2cBus final : public I2cBus {
public:
	bool initialize(lexxos::platform::I2cPins const& pins) override;
	[[nodiscard]] bool isReady() const override;
	bool probe(std::uint8_t address) override;
	bool write(std::uint8_t address, std::uint8_t const* data, std::size_t size) override;
	bool read(std::uint8_t address, std::uint8_t* data, std::size_t size) override;
	bool beginTransaction() override;
	void endTransaction() override;

private:
	void _configureI2cPins();
	void _recoverBus();

	lexxos::platform::I2cPins _pins{};
	i2c_inst_t* _instance{nullptr};
	I2cBusMutex _mutex;
	bool _ready{false};
};

} // namespace lexxos::devices
