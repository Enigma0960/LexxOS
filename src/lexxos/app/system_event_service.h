#pragma once

#include <atomic>
#include <cstdint>

#include <lexxos/feature.h>
#include <lexxos/lifecycle/event_bus.h>
#include <lexxos/platform/board.h>
#include <lexxos/runtime/capabilities.h>

namespace lexxos::platform {

struct PlatformServices;

} // namespace lexxos::platform

namespace lexxos::app {

class SdAutomountService;

using SystemEventType = lexxos::lifecycle::EventType;
using SystemEvent = lexxos::lifecycle::Event;

class SystemEventService {
public:
	SystemEventService();
	explicit SystemEventService(lexxos::platform::PlatformServices const& platform);
	SystemEventService(lexxos::platform::PlatformServices const& platform, lexxos::lifecycle::EventBus& events);

#if FEATURE_ENABLED(SD_CARD)
	void bindSdAutomount(SdAutomountService& service);
#endif
	void setCapabilities(lexxos::runtime::CapabilitySet capabilities);
	void initialize();
	[[nodiscard]] bool post(SystemEvent event);
	[[nodiscard]] bool processPending();
#if FEATURE_ENABLED(SD_CARD)
	void pollSdCardDetect();
#endif
	void run();

private:
#if FEATURE_ENABLED(SD_CARD)
	static void sdCardDetectInterrupt(lexxos::platform::GpioPin pin, bool value, void* userData);
#endif

	void handle(SystemEvent const& event);
	[[nodiscard]] bool storageHotPlugAvailable() const;
#if FEATURE_ENABLED(SD_CARD)
	void initializeSdCardDetectState();
	void armSdCardDetectInterrupt();
	void notifySdCardDetectInterrupt(bool value);
	void processSdCardDetectInterrupt();
	[[nodiscard]] bool readDebouncedSdCardState(bool& inserted);
	void postSdCardDetectChange(bool inserted);
#endif

#if FEATURE_ENABLED(SD_CARD)
	SdAutomountService* _sdAutomount{nullptr};
#endif
	lexxos::platform::PlatformServices const& _platformServices;
	lexxos::lifecycle::EventBus& _events;
	lexxos::lifecycle::EventCursor _eventCursor{};
	lexxos::runtime::CapabilitySet _capabilities{};
#if FEATURE_ENABLED(SD_CARD)
	bool _sdDetectInitialized{false};
	bool _stableSdInserted{false};
	bool _previousSdSample{false};
	std::uint8_t _stableSdSamples{0};
	bool _sdDetectInterruptEnabled{false};
	std::atomic_bool _sdDetectInterruptPending{false};
#endif
};

} // namespace lexxos::app
