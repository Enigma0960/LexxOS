#pragma once

#include <lexxos/devices/bus/i2c_bus.h>

namespace lexxos::devices {

// Single-master open-drain GPIO bus. Hooks must not yield during transfers.
// Serialize whole register transactions externally on preemptive/SMP platforms.
struct SoftwareI2cIo {
	void* context{};
	bool (*configure)(void*, lexxos::platform::I2cPins const&){};
	void (*release)(void*, lexxos::platform::GpioPin, bool){};
	bool (*read)(void*, lexxos::platform::GpioPin){};
	std::uint64_t (*counter)(void*){};
	std::uint32_t (*frequency)(void*){};
};

class SoftwareI2cBus final : public I2cBus {
public:
	explicit SoftwareI2cBus(SoftwareI2cIo io)
	    : _io(io) {}

	bool initialize(lexxos::platform::I2cPins const& pins) override;

	bool isReady() const override { return _ready; }

	// Caller owns the bus; no slave address or register write is emitted.
	bool clearBus();
	bool probe(std::uint8_t address) override;
	bool write(std::uint8_t address, std::uint8_t const* data, std::size_t size) override;
	bool read(std::uint8_t address, std::uint8_t* data, std::size_t size) override;

private:
	SoftwareI2cIo _io;
	lexxos::platform::I2cPins _pins{};
	bool _ready{false};
	std::uint64_t _halfPeriod{};
	std::uint64_t _timeout{};
	std::uint64_t _started{};
	void _begin();
	bool _expired() const;
	void _delay() const;
	bool _raiseClock() const;
	bool _stop() const;
	bool _finish(bool ok) const;
	bool _start() const;
	bool _writeBit(bool high) const;
	bool _readBit(bool& high) const;
	bool _writeByte(std::uint8_t value) const;
	bool _readByte(std::uint8_t& value, bool last) const;
};

} // namespace lexxos::devices
