#pragma once

#include <lexxos/devices/bus/i2c_bus.h>

namespace lexxos::devices {

// Platform selects OSC24M / 1 and owns pinmux, reset and GIC masking.
// Register offsets are relative to I2C; 10/100 kHz are supported.
// Callbacks must not yield: CPU0 cooperative transactions remain indivisible.
struct Rk3506I2cIo {
	void* context{};
	bool (*configure)(void*, lexxos::platform::I2cPins const&){};
	std::uint32_t (*read)(void*, std::uint32_t){};
	void (*write)(void*, std::uint32_t, std::uint32_t){};
	std::uint64_t (*counter)(void*){};
	std::uint32_t (*frequency)(void*){};
	bool (*recover)(void*, lexxos::platform::I2cPins const&){};
};

class Rk3506I2cBus final : public I2cBus {
public:
	explicit Rk3506I2cBus(Rk3506I2cIo io)
	    : _io(io) {}

	bool initialize(lexxos::platform::I2cPins const& pins) override;

	bool isReady() const override { return _ready; }

	bool probe(std::uint8_t address) override;

	I2cFailureSnapshot lastFailure() const override {
		auto snapshot = _lastFailure;
		snapshot.recoveryAttempts = _recoveryAttempts;
		snapshot.recoverySuccesses = _recoverySuccesses;
		return snapshot;
	}

	bool write(std::uint8_t address, std::uint8_t const* data, std::size_t size) override;
	bool read(std::uint8_t address, std::uint8_t* data, std::size_t size) override;

private:
	Rk3506I2cIo _io;
	mutable I2cFailureSnapshot _lastFailure{};
	bool _ready{false};
	bool _needsRecovery{};
	bool _recoveryCoolingDown{};
	std::uint64_t _lastRecoveryAttempt{}, _recoveryBackoff{};
	std::uint32_t _recoveryAttempts{}, _recoverySuccesses{};
	lexxos::platform::I2cPins _pins{};
	std::uint32_t _clockDivider{};
	std::uint64_t _timeout{};
	std::uint32_t _read(std::uint32_t offset) const;
	void _write(std::uint32_t offset, std::uint32_t value) const;
	bool _begin();
	bool _wait(std::uint32_t event, bool checkNack = true) const;
	bool _start(std::uint32_t control) const;
	bool _finish(bool ok);
	bool _transmit(std::uint8_t address, std::uint8_t const* data, std::size_t size);
};

} // namespace lexxos::devices
