#pragma once

#include <cstdint>
#include <string_view>

#include <lexxos/devices/sensors/sensor.h>
#include <lexxos/platform/board.h>
#include <lexxos/platform/platform_services.h>

namespace lexxos::devices {

class I2cBus;

class Bmi270Sensor final : public MotionSensor {
public:
	Bmi270Sensor(I2cBus& bus, lexxos::platform::MotionSensorPins config, lexxos::platform::PlatformServices const& platform);

	bool initialize();

	[[nodiscard]] std::string_view name() const override;
	[[nodiscard]] SensorState state() const override;
	bool poll() override;
	[[nodiscard]] MotionSensorSample lastSample() const override;
	bool readSample(MotionSensorSample& sample) override;

private:
	[[nodiscard]] bool _softReset();
	[[nodiscard]] bool _writeConfigFile();
	[[nodiscard]] bool _uploadConfigChunk(std::uint16_t index, std::uint8_t const* data, std::size_t size);
	[[nodiscard]] bool _readInternalStatus(std::uint8_t& status);
	[[nodiscard]] bool _readRegister(std::uint8_t reg, std::uint8_t& value);
	[[nodiscard]] bool _readBlock(std::uint8_t reg, std::uint8_t* data, std::size_t size);
	[[nodiscard]] bool _writeRegister(std::uint8_t reg, std::uint8_t value);
	[[nodiscard]] bool _writeBlock(std::uint8_t reg, std::uint8_t const* data, std::size_t size);
	[[nodiscard]] bool _configureBasicSampling();
	[[nodiscard]] std::uint32_t _uptimeMs() const;
	void _sleepMs(std::uint32_t milliseconds) const;

	I2cBus& _bus;
	lexxos::platform::MotionSensorPins _config{};
	lexxos::platform::PlatformServices const* _platform{nullptr};
	SensorState _state{SensorState::Unavailable};
	MotionSensorSample _lastSample{};
};

} // namespace lexxos::devices
