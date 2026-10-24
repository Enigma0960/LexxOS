#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/devices/input/detail/picocalc_keyboard_diagnostics.h>
#include <lexxos/devices/input/detail/picocalc_keyboard_event_bridge.h>
#include <lexxos/devices/input/detail/picocalc_keyboard_fifo_poller.h>
#include <lexxos/devices/input/detail/picocalc_keyboard_initializer.h>
#include <lexxos/devices/input/detail/picocalc_keyboard_recovery_policy.h>
#include <lexxos/devices/input/detail/picocalc_keyboard_runtime_service.h>
#include <lexxos/devices/input/detail/picocalc_keyboard_transport.h>
#include <lexxos/devices/input_device.h>
#include <lexxos/devices/picocalc/picocalc_southbridge.h>
#include <lexxos/devices/picocalc/picocalc_southbridge_service.h>

namespace lexxos::platform {
struct PlatformServices;
} // namespace lexxos::platform

namespace lexxos::devices {
class I2cBus;
} // namespace lexxos::devices

namespace lexxos::devices::input_detail {

class PicoCalcKeyboardController final {
public:
	static constexpr std::size_t eventQueueCapacity = PicoCalcKeyboardEventBridge::eventQueueCapacity;

	PicoCalcKeyboardController(
	    lexxos::devices::I2cBus& bus,
	    lexxos::platform::PlatformServices const& platform,
	    std::uint8_t address);

	[[nodiscard]] bool initialize();
	void update();
	void setRawEventReporting(bool enabled);

	[[nodiscard]] std::string_view name() const;
	[[nodiscard]] bool isReady() const;
	[[nodiscard]] bool pollEvent(lexxos::devices::InputEvent& event);
	[[nodiscard]] bool isPressed(lexxos::devices::KeyCode key) const;
	[[nodiscard]] PicoCalcSouthbridgeRuntimeService& runtimeService();
	[[nodiscard]] PicoCalcSouthbridgeRuntimeService const& runtimeService() const;
	[[nodiscard]] std::uint32_t rawState() const;
	[[nodiscard]] std::uint32_t rawKeyCount() const;
	[[nodiscard]] std::uint32_t rawRowCount() const;
	[[nodiscard]] std::uint32_t rawColumnCount() const;
	[[nodiscard]] std::uint32_t staleScanCodeReleases() const;
	[[nodiscard]] std::uint32_t staleReleaseCount() const;

private:
	friend class PicoCalcKeyboardRuntimeService;

	static constexpr std::uint32_t southbridgeRefreshQuietMs = 1000;

	void _resetRuntimeState();
	void _handleCommunicationFailure(std::string_view stage);
	void _recordCommunicationFailure(std::string_view stage, std::uint32_t now);
	void _recordSouthbridgeRefreshFailure(std::uint32_t now);
	void _tryRecoverSouthbridge(std::uint32_t now);
	[[nodiscard]] PicoCalcKeyboardDiagnosticContext _diagnosticContext() const;
	[[nodiscard]] bool _canRefreshSouthbridge(std::uint32_t now) const;
	[[nodiscard]] std::uint32_t _uptimeMs() const;

	lexxos::devices::I2cBus& _bus;
	lexxos::platform::PlatformServices const& _platformServices;
	PicoCalcKeyboardRuntimeService _runtimeService;
	PicoCalcKeyboardRecoveryPolicy _recovery;
	PicoCalcKeyboardDiagnostics _diagnostics;
	PicoCalcSouthbridge _southbridge;
	PicoCalcKeyboardTransport _transport;
	PicoCalcKeyboardInitializer _initializer{};
	PicoCalcKeyboardFifoPoller _fifoPoller{};
	PicoCalcSouthbridgeService _southbridgeService;
	bool _ready{false};
	PicoCalcKeyboardEventBridge _eventBridge{};
	std::uint32_t _lastKeyboardActivityMs{0};
	std::uint8_t _lastKeyboardStatus{0};
	std::uint8_t _lastKeyboardCount{0};
	PicoCalcKeyboardFifoPoller::EventBuffer _fifoEvents{};
};

} // namespace lexxos::devices::input_detail
