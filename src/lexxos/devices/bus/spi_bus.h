#pragma once

#include <cstddef>
#include <cstdint>

#include <lexxos/platform/board.h>

namespace lexxos::devices {

enum class SpiClockMode : std::uint8_t {
	Mode0 = 0,
	Mode1 = 1,
	Mode2 = 2,
	Mode3 = 3,
};

class SpiBus {
public:
	virtual ~SpiBus() = default;

	virtual bool initialize(lexxos::platform::SpiPins const& pins) = 0;
	[[nodiscard]] virtual bool isReady() const = 0;
	virtual bool setBaudRate(std::uint32_t baudRate) = 0;
	virtual bool setClockMode(SpiClockMode mode) = 0;
	[[nodiscard]] virtual SpiClockMode clockMode() const = 0;
	virtual void select() = 0;
	virtual void deselect() = 0;
	virtual bool transfer(std::uint8_t const* tx, std::uint8_t* rx, std::size_t size) = 0;
	virtual bool write(std::uint8_t const* data, std::size_t size) = 0;
	virtual bool read(std::uint8_t fill, std::uint8_t* data, std::size_t size) = 0;

	virtual bool beginTransaction() {
		return true;
	}

	virtual void endTransaction() {
	}

	[[nodiscard]] virtual bool supportsDma() const {
		return false;
	}

	virtual bool startDmaTransfer(std::uint8_t const* tx, std::uint8_t* rx, std::size_t size) {
		if (tx != nullptr && rx != nullptr) {
			return transfer(tx, rx, size);
		}
		if (tx != nullptr) {
			return write(tx, size);
		}
		if (rx != nullptr) {
			return read(0xFF, rx, size);
		}
		return size == 0;
	}

	virtual bool waitDmaTransfer(std::uint32_t timeoutMs) {
		(void) timeoutMs;
		return true;
	}

	virtual bool transferDma(std::uint8_t const* tx, std::uint8_t* rx, std::size_t size, std::uint32_t timeoutMs) {
		return startDmaTransfer(tx, rx, size) && waitDmaTransfer(timeoutMs);
	}
};

class SpiBusTransaction {
public:
	explicit SpiBusTransaction(SpiBus& bus)
	    : _bus(&bus)
	    , _active(bus.beginTransaction()) {
	}

	SpiBusTransaction(SpiBusTransaction const&) = delete;
	SpiBusTransaction& operator=(SpiBusTransaction const&) = delete;

	SpiBusTransaction(SpiBusTransaction&& other) noexcept
	    : _bus(other._bus)
	    , _active(other._active) {
		other._bus = nullptr;
		other._active = false;
	}

	SpiBusTransaction& operator=(SpiBusTransaction&& other) noexcept {
		if (this != &other) {
			release();
			_bus = other._bus;
			_active = other._active;
			other._bus = nullptr;
			other._active = false;
		}
		return *this;
	}

	~SpiBusTransaction() {
		release();
	}

	[[nodiscard]] bool active() const {
		return _active;
	}

	[[nodiscard]] explicit operator bool() const {
		return active();
	}

	void release() {
		if (_bus != nullptr && _active) {
			_bus->endTransaction();
		}
		_bus = nullptr;
		_active = false;
	}

private:
	SpiBus* _bus{nullptr};
	bool _active{false};
};

SpiBus& sdCardSpiBus();
SpiBus& displaySpiBus();

} // namespace lexxos::devices
