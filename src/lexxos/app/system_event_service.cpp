#include <lexxos/app/system_event_service.h>

#include <string_view>

#include <lexxos/app/console_output.h>
#include <lexxos/feature.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/kernel/system.h>
#include <lexxos/platform/platform_services.h>

#if FEATURE_ENABLED(SD_CARD)
	#include <lexxos/app/sd_automount_service.h>
	#include <lexxos/devices/sd_card.h>
#endif

#if FEATURE_ENABLED(SD_CARD)
namespace sd_card = lexxos::devices::sd_card;
#endif

namespace {

#if FEATURE_ENABLED(SD_CARD)
constexpr std::uint8_t sdDetectDebounceSamples = 3;
constexpr std::uint32_t sdDetectDebounceIntervalMs = 25;
#endif
constexpr std::uint32_t systemEventIntervalMs = 100;

void writeConsole(lexxos::platform::PlatformServices const& platform, std::string_view text) {
	lexxos::app::console_output::write(platform, text);
}

[[maybe_unused]] void writeConsoleLine(lexxos::platform::PlatformServices const& platform, std::string_view text) {
	writeConsole(platform, text);
	writeConsole(platform, "\r\n");
}

} // namespace

lexxos::app::SystemEventService::SystemEventService()
    : SystemEventService(lexxos::platform::services()) {
}

lexxos::app::SystemEventService::SystemEventService(lexxos::platform::PlatformServices const& platform)
    : SystemEventService(platform, lexxos::lifecycle::defaultEventBus()) {
}

lexxos::app::SystemEventService::SystemEventService(
    lexxos::platform::PlatformServices const& platform,
    lexxos::lifecycle::EventBus& events)
    : _platformServices(platform)
    , _events(events) {
}

#if FEATURE_ENABLED(SD_CARD)
void lexxos::app::SystemEventService::bindSdAutomount(SdAutomountService& service) {
	_sdAutomount = &service;
}
#endif

void lexxos::app::SystemEventService::setCapabilities(lexxos::runtime::CapabilitySet capabilities) {
	_capabilities = capabilities;
}

void lexxos::app::SystemEventService::initialize() {
	_events.reset(_eventCursor);
#if FEATURE_ENABLED(SD_CARD)
	if (storageHotPlugAvailable()) {
		initializeSdCardDetectState();
		armSdCardDetectInterrupt();
	}
#endif
}

bool lexxos::app::SystemEventService::post(SystemEvent event) {
	return _events.publish(event);
}

bool lexxos::app::SystemEventService::processPending() {
	bool processed = false;
	SystemEvent event{};
	while (_events.poll(_eventCursor, event)) {
		processed = true;
		handle(event);
	}
	return processed;
}

#if FEATURE_ENABLED(SD_CARD)
void lexxos::app::SystemEventService::pollSdCardDetect() {
	if (!storageHotPlugAvailable()) {
		return;
	}
	if (!sd_card::hasCardDetect()) {
		return;
	}

	bool const currentSample = sd_card::isCardInserted();
	if (!_sdDetectInitialized) {
		_sdDetectInitialized = true;
		_stableSdInserted = currentSample;
		_previousSdSample = currentSample;
		_stableSdSamples = 0;
		return;
	}

	if (currentSample == _previousSdSample) {
		if (_stableSdSamples < sdDetectDebounceSamples) {
			++_stableSdSamples;
		}
	} else {
		_previousSdSample = currentSample;
		_stableSdSamples = 1;
	}

	if (_stableSdSamples < sdDetectDebounceSamples || currentSample == _stableSdInserted) {
		return;
	}

	postSdCardDetectChange(currentSample);
}
#endif

void lexxos::app::SystemEventService::run() {
#if FEATURE_ENABLED(SD_CARD)
	if (storageHotPlugAvailable() && _sdAutomount != nullptr && (!sd_card::hasCardDetect() || sd_card::isCardInserted())) {
		_sdAutomount->mount("boot");
	}
#endif

	while (!lexxos::kernel::isShutdownRequested()) {
#if FEATURE_ENABLED(SD_CARD)
		if (storageHotPlugAvailable() && _sdDetectInterruptEnabled) {
			if (_sdDetectInterruptPending.exchange(false)) {
				processSdCardDetectInterrupt();
			}
		} else if (storageHotPlugAvailable()) {
			pollSdCardDetect();
		}
#endif
		[[maybe_unused]] bool const processed = processPending();
		lexxos::kernel::sleepMs(systemEventIntervalMs);
	}
}

#if FEATURE_ENABLED(SD_CARD)
void lexxos::app::SystemEventService::sdCardDetectInterrupt(lexxos::platform::GpioPin, bool value, void* userData) {
	if (userData == nullptr) {
		return;
	}
	auto& service = *static_cast<SystemEventService*>(userData);
	service.notifySdCardDetectInterrupt(value);
}
#endif

void lexxos::app::SystemEventService::handle(SystemEvent const& event) {
	switch (event.type) {
		case SystemEventType::None:
			break;
#if FEATURE_ENABLED(SD_CARD)
		case SystemEventType::SdCardDetectChanged:
			if (storageHotPlugAvailable()) {
				processSdCardDetectInterrupt();
			}
			break;
		case SystemEventType::SdCardInserted:
			writeConsoleLine(_platformServices, "sd card: inserted");
			if (storageHotPlugAvailable() && _sdAutomount != nullptr) {
				_sdAutomount->mount("inserted");
			}
			armSdCardDetectInterrupt();
			break;
		case SystemEventType::SdCardRemoved:
			writeConsoleLine(_platformServices, "sd card: removed");
			if (storageHotPlugAvailable() && _sdAutomount != nullptr) {
				_sdAutomount->unmount("removed");
			}
			armSdCardDetectInterrupt();
			break;
#else
		case SystemEventType::SdCardDetectChanged:
		case SystemEventType::SdCardInserted:
		case SystemEventType::SdCardRemoved:
			break;
#endif
		case SystemEventType::Input:
		case SystemEventType::PowerOffRequested:
		case SystemEventType::ShutdownRequested:
			break;
	}
}

bool lexxos::app::SystemEventService::storageHotPlugAvailable() const {
	return _capabilities.has(lexxos::runtime::Capability::StorageRemovable)
	    && _capabilities.has(lexxos::runtime::Capability::StorageBlock);
}

#if FEATURE_ENABLED(SD_CARD)
void lexxos::app::SystemEventService::initializeSdCardDetectState() {
	if (!storageHotPlugAvailable()) {
		return;
	}
	if (!sd_card::hasCardDetect()) {
		return;
	}

	bool const inserted = sd_card::isCardInserted();
	_sdDetectInitialized = true;
	_stableSdInserted = inserted;
	_previousSdSample = inserted;
	_stableSdSamples = sdDetectDebounceSamples;
	_sdDetectInterruptPending.store(false);
}

void lexxos::app::SystemEventService::armSdCardDetectInterrupt() {
	if (!storageHotPlugAvailable()) {
		_sdDetectInterruptEnabled = false;
		return;
	}
	if (!sd_card::hasCardDetect()) {
		_sdDetectInterruptEnabled = false;
		return;
	}

	auto const& board = _platformServices.board;
	auto const& gpio = _platformServices.gpio;
	if (board.current == nullptr || gpio.configureInterrupt == nullptr) {
		_sdDetectInterruptEnabled = false;
		return;
	}

	_sdDetectInterruptEnabled = gpio.configureInterrupt(
	    board.current().sdCardDetect.detect,
	    lexxos::platform::GpioInterruptEdge::Both,
	    sdCardDetectInterrupt,
	    this);
}

void lexxos::app::SystemEventService::notifySdCardDetectInterrupt(bool value) {
	if (!storageHotPlugAvailable()) {
		return;
	}
	(void) _events.publishFromIsr({.type = SystemEventType::SdCardDetectChanged, .asserted = value});
	_sdDetectInterruptPending.store(true);
}

void lexxos::app::SystemEventService::processSdCardDetectInterrupt() {
	if (!storageHotPlugAvailable()) {
		return;
	}
	bool inserted = false;
	if (!readDebouncedSdCardState(inserted)) {
		return;
	}

	if (!_sdDetectInitialized) {
		_sdDetectInitialized = true;
		_stableSdInserted = inserted;
		_previousSdSample = inserted;
		_stableSdSamples = sdDetectDebounceSamples;
		return;
	}

	if (inserted != _stableSdInserted) {
		postSdCardDetectChange(inserted);
	}
}

bool lexxos::app::SystemEventService::readDebouncedSdCardState(bool& inserted) {
	if (!storageHotPlugAvailable()) {
		return false;
	}
	if (!sd_card::hasCardDetect()) {
		return false;
	}

	bool stableSample = sd_card::isCardInserted();
	std::uint8_t stableSamples = 1;
	while (stableSamples < sdDetectDebounceSamples && !lexxos::kernel::isShutdownRequested()) {
		lexxos::kernel::sleepMs(sdDetectDebounceIntervalMs);
		bool const sample = sd_card::isCardInserted();
		if (sample == stableSample) {
			++stableSamples;
		} else {
			stableSample = sample;
			stableSamples = 1;
		}
	}

	inserted = stableSample;
	return stableSamples >= sdDetectDebounceSamples;
}

void lexxos::app::SystemEventService::postSdCardDetectChange(bool inserted) {
	if (!storageHotPlugAvailable()) {
		return;
	}
	_stableSdInserted = inserted;
	_previousSdSample = inserted;
	_stableSdSamples = sdDetectDebounceSamples;
	if (!post({.type = _stableSdInserted ? SystemEventType::SdCardInserted : SystemEventType::SdCardRemoved})) {
		writeConsoleLine(_platformServices, "system event: queue full, sd card event dropped");
	}
}
#endif
