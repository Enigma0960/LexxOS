#include <lexxos/devices/rtc/software_rtc.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>

#include <lexxos/kernel/scheduler.h>
#include <lexxos/platform/platform.h>
#include <lexxos/platform/platform_services.h>

namespace lexxos::devices::rtc::software {
namespace {

	constexpr std::uint32_t validFlag = 1U << 0U;
	constexpr std::uint32_t lastNetworkAttemptValidFlag = 1U << 1U;
	constexpr std::uint32_t lastNetworkAttemptOkFlag = 1U << 2U;
	constexpr std::size_t maxRuntimeNetworkSyncServerLength = 127;
	constexpr std::size_t runtimeNetworkSyncServerSlots = 4;

	struct Snapshot {
		std::uint32_t flags{0};
		std::uint32_t baseUnixSeconds{0};
		std::uint32_t baseUptimeMs{0};
		std::uint32_t lastSetUptimeMs{0};
		std::uint32_t lastNetworkSyncUptimeMs{0};
		std::uint32_t lastNetworkAttemptUptimeMs{0};
		std::uint32_t setCount{0};
		std::uint32_t networkSyncCount{0};
		SetSource lastSetSource{SetSource::None};
	};

	struct RuntimeState {
		std::atomic<std::uint32_t> sequence{0};
		std::atomic<std::uint32_t> writer{0};
		std::atomic<std::uint32_t> flags{0};
		std::atomic<std::uint32_t> baseUnixSeconds{0};
		std::atomic<std::uint32_t> baseUptimeMs{0};
		std::atomic<std::uint32_t> lastSetUptimeMs{0};
		std::atomic<std::uint32_t> lastNetworkSyncUptimeMs{0};
		std::atomic<std::uint32_t> lastNetworkAttemptUptimeMs{0};
		std::atomic<std::uint32_t> setCount{0};
		std::atomic<std::uint32_t> networkSyncCount{0};
		std::atomic<std::uint32_t> lastSetSource{static_cast<std::uint32_t>(SetSource::None)};
	};

	RuntimeState g_state{};

	struct RuntimeConfigOverride {
		std::atomic<std::uint32_t> sequence{0};
		std::atomic<std::uint32_t> writer{0};
		bool active{false};
		lexxos::platform::SoftwareRtcConfig config{};
		std::array<std::array<char, maxRuntimeNetworkSyncServerLength + 1U>, runtimeNetworkSyncServerSlots> servers{};
		std::atomic<std::uint32_t> activeSlot{0};
	};

	RuntimeConfigOverride g_configOverride{};

	lexxos::platform::BoardDescriptor const& boardFor(lexxos::platform::PlatformServices const& platform) {
		auto const& board = platform.board;
		return board.current != nullptr ? board.current() : lexxos::platform::board();
	}

	std::uint32_t uptimeMs(lexxos::platform::PlatformServices const& platform) {
		auto const& clock = platform.clock;
		return clock.uptimeMs != nullptr ? clock.uptimeMs() : 0U;
	}

	void acquireWriter() {
		for (;;) {
			std::uint32_t expected = 0;
			if (g_state.writer.compare_exchange_weak(expected, 1, std::memory_order_acquire, std::memory_order_relaxed)) {
				return;
			}
			lexxos::kernel::yield();
		}
	}

	void releaseWriter() {
		g_state.writer.store(0, std::memory_order_release);
	}

	void acquireConfigWriter() {
		for (;;) {
			std::uint32_t expected = 0;
			if (g_configOverride.writer.compare_exchange_weak(expected, 1, std::memory_order_acquire, std::memory_order_relaxed)) {
				return;
			}
			lexxos::kernel::yield();
		}
	}

	void releaseConfigWriter() {
		g_configOverride.writer.store(0, std::memory_order_release);
	}

	void beginWrite() {
		auto const sequence = g_state.sequence.load(std::memory_order_relaxed);
		g_state.sequence.store(sequence + 1U, std::memory_order_release);
	}

	void endWrite() {
		auto sequence = g_state.sequence.load(std::memory_order_relaxed);
		if ((sequence & 1U) == 0U) {
			++sequence;
		}
		g_state.sequence.store(sequence + 1U, std::memory_order_release);
	}

	void beginConfigWrite() {
		auto const sequence = g_configOverride.sequence.load(std::memory_order_relaxed);
		g_configOverride.sequence.store(sequence + 1U, std::memory_order_release);
	}

	void endConfigWrite() {
		auto sequence = g_configOverride.sequence.load(std::memory_order_relaxed);
		if ((sequence & 1U) == 0U) {
			++sequence;
		}
		g_configOverride.sequence.store(sequence + 1U, std::memory_order_release);
	}

	bool snapshot(Snapshot& output) {
		for (std::uint8_t attempt = 0; attempt < 8U; ++attempt) {
			auto const start = g_state.sequence.load(std::memory_order_acquire);
			if ((start & 1U) != 0U) {
				lexxos::kernel::yield();
				continue;
			}

			Snapshot value{};
			value.flags = g_state.flags.load(std::memory_order_relaxed);
			value.baseUnixSeconds = g_state.baseUnixSeconds.load(std::memory_order_relaxed);
			value.baseUptimeMs = g_state.baseUptimeMs.load(std::memory_order_relaxed);
			value.lastSetUptimeMs = g_state.lastSetUptimeMs.load(std::memory_order_relaxed);
			value.lastNetworkSyncUptimeMs = g_state.lastNetworkSyncUptimeMs.load(std::memory_order_relaxed);
			value.lastNetworkAttemptUptimeMs = g_state.lastNetworkAttemptUptimeMs.load(std::memory_order_relaxed);
			value.setCount = g_state.setCount.load(std::memory_order_relaxed);
			value.networkSyncCount = g_state.networkSyncCount.load(std::memory_order_relaxed);
			value.lastSetSource = static_cast<SetSource>(g_state.lastSetSource.load(std::memory_order_relaxed));

			auto const end = g_state.sequence.load(std::memory_order_acquire);
			if (start == end && (end & 1U) == 0U) {
				output = value;
				return true;
			}
		}
		return false;
	}

	bool runtimeConfigOverrideSnapshot(lexxos::platform::SoftwareRtcConfig& output) {
		for (std::uint8_t attempt = 0; attempt < 8U; ++attempt) {
			auto const start = g_configOverride.sequence.load(std::memory_order_acquire);
			if ((start & 1U) != 0U) {
				lexxos::kernel::yield();
				continue;
			}

			bool const active = g_configOverride.active;
			auto const value = g_configOverride.config;

			auto const end = g_configOverride.sequence.load(std::memory_order_acquire);
			if (start == end && (end & 1U) == 0U) {
				if (!active) {
					return false;
				}
				output = value;
				return true;
			}
		}
		return false;
	}

	bool currentUnixSeconds(lexxos::platform::PlatformServices const& platform, Snapshot const& snapshot, std::uint32_t& output) {
		if ((snapshot.flags & validFlag) == 0U) {
			return false;
		}

		std::uint32_t const elapsedMs = uptimeMs(platform) - snapshot.baseUptimeMs;
		std::uint64_t const seconds = static_cast<std::uint64_t>(snapshot.baseUnixSeconds) + static_cast<std::uint64_t>(elapsedMs / 1000U);
		if (seconds > std::numeric_limits<std::uint32_t>::max()) {
			return false;
		}

		output = static_cast<std::uint32_t>(seconds);
		return true;
	}

} // namespace

bool configured(lexxos::platform::BoardDescriptor const& board) {
	return board.softwareRtc.isValid();
}

bool configured(lexxos::platform::PlatformServices const& platform) {
	return config(platform).isValid();
}

bool preferredAsDefault(lexxos::platform::BoardDescriptor const& board) {
	return configured(board) && board.softwareRtc.preferAsDefault;
}

bool preferredAsDefault(lexxos::platform::PlatformServices const& platform) {
	auto const rtcConfig = config(platform);
	return rtcConfig.isValid() && rtcConfig.preferAsDefault;
}

lexxos::platform::SoftwareRtcConfig config(lexxos::platform::PlatformServices const& platform) {
	lexxos::platform::SoftwareRtcConfig overrideConfig{};
	if (runtimeConfigOverrideSnapshot(overrideConfig)) {
		return overrideConfig;
	}
	return boardFor(platform).softwareRtc;
}

bool applyRuntimeConfigOverride(
    lexxos::platform::PlatformServices const& platform,
    lexxos::platform::SoftwareRtcConfig const& newConfig,
    std::string_view networkSyncServer) {
	if (!configured(boardFor(platform)) || networkSyncServer.empty() || networkSyncServer.size() > maxRuntimeNetworkSyncServerLength) {
		return false;
	}
	for (char const character : networkSyncServer) {
		if (character == '\r' || character == '\n' || character == '\0') {
			return false;
		}
	}

	acquireConfigWriter();

	std::uint32_t const currentSlot = g_configOverride.activeSlot.load(std::memory_order_acquire);
	std::uint32_t const nextSlot = (currentSlot + 1U) % runtimeNetworkSyncServerSlots;
	auto& server = g_configOverride.servers[nextSlot];
	std::memcpy(server.data(), networkSyncServer.data(), networkSyncServer.size());
	server[networkSyncServer.size()] = '\0';
	for (std::size_t index = networkSyncServer.size() + 1U; index < server.size(); ++index) {
		server[index] = '\0';
	}

	auto overrideConfig = newConfig;
	overrideConfig.networkSyncServer = server.data();

	beginConfigWrite();
	g_configOverride.config = overrideConfig;
	g_configOverride.active = true;
	g_configOverride.activeSlot.store(nextSlot, std::memory_order_release);
	endConfigWrite();
	releaseConfigWriter();
	return true;
}

void clearRuntimeConfigOverride() {
	acquireConfigWriter();
	beginConfigWrite();
	g_configOverride.active = false;
	endConfigWrite();
	releaseConfigWriter();
}

bool runtimeConfigOverrideActive() {
	lexxos::platform::SoftwareRtcConfig ignored{};
	return runtimeConfigOverrideSnapshot(ignored);
}

bool read(lexxos::platform::PlatformServices const& platform, DateTime& output) {
	std::uint32_t unixSeconds = 0;
	if (!readUnixSeconds(platform, unixSeconds)) {
		return false;
	}
	return unixSecondsToDateTime(unixSeconds, output);
}

bool readUnixSeconds(lexxos::platform::PlatformServices const& platform, std::uint32_t& output) {
	if (!configured(platform)) {
		return false;
	}

	Snapshot value{};
	return snapshot(value) && currentUnixSeconds(platform, value, output);
}

bool set(lexxos::platform::PlatformServices const& platform, DateTime const& value, SetSource source) {
	std::uint32_t unixSeconds = 0;
	if (!dateTimeToUnixSeconds(value, unixSeconds)) {
		return false;
	}
	return setUnixSeconds(platform, unixSeconds, source);
}

bool setUnixSeconds(lexxos::platform::PlatformServices const& platform, std::uint32_t unixSeconds, SetSource source) {
	if (!configured(platform)) {
		return false;
	}

	DateTime decoded{};
	if (!unixSecondsToDateTime(unixSeconds, decoded)) {
		return false;
	}
	(void) decoded;

	std::uint32_t const nowMs = uptimeMs(platform);
	acquireWriter();
	beginWrite();
	g_state.baseUnixSeconds.store(unixSeconds, std::memory_order_relaxed);
	g_state.baseUptimeMs.store(nowMs, std::memory_order_relaxed);
	g_state.lastSetUptimeMs.store(nowMs, std::memory_order_relaxed);
	g_state.lastSetSource.store(static_cast<std::uint32_t>(source), std::memory_order_relaxed);
	g_state.setCount.store(g_state.setCount.load(std::memory_order_relaxed) + 1U, std::memory_order_relaxed);
	std::uint32_t flags = g_state.flags.load(std::memory_order_relaxed) | validFlag;
	if (source == SetSource::Network) {
		g_state.lastNetworkSyncUptimeMs.store(nowMs, std::memory_order_relaxed);
		g_state.lastNetworkAttemptUptimeMs.store(nowMs, std::memory_order_relaxed);
		g_state.networkSyncCount.store(g_state.networkSyncCount.load(std::memory_order_relaxed) + 1U, std::memory_order_relaxed);
		flags |= lastNetworkAttemptValidFlag | lastNetworkAttemptOkFlag;
	}
	g_state.flags.store(flags, std::memory_order_relaxed);
	endWrite();
	releaseWriter();
	return true;
}

void recordNetworkAttempt(lexxos::platform::PlatformServices const& platform, bool ok) {
	if (!configured(platform)) {
		return;
	}

	acquireWriter();
	beginWrite();
	std::uint32_t flags = g_state.flags.load(std::memory_order_relaxed);
	flags |= lastNetworkAttemptValidFlag;
	if (ok) {
		flags |= lastNetworkAttemptOkFlag;
	} else {
		flags &= ~lastNetworkAttemptOkFlag;
	}
	g_state.lastNetworkAttemptUptimeMs.store(uptimeMs(platform), std::memory_order_relaxed);
	g_state.flags.store(flags, std::memory_order_relaxed);
	endWrite();
	releaseWriter();
}

Status status(lexxos::platform::PlatformServices const& platform) {
	auto const rtcConfig = config(platform);
	Status result{};
	result.configured = rtcConfig.isValid();
	result.autoNetworkSync = rtcConfig.autoNetworkSync;
	result.networkSyncServer = rtcConfig.networkSyncServer;
	result.networkSyncTimeoutMs = rtcConfig.networkSyncTimeoutMs;
	result.networkSyncIntervalMs = rtcConfig.networkSyncIntervalMs;
	result.networkSyncRetryIntervalMs = rtcConfig.networkSyncRetryIntervalMs;
	result.startupDelayMs = rtcConfig.startupDelayMs;
	result.timezoneOffsetSeconds = rtcConfig.timezoneOffsetSeconds;
	if (!result.configured) {
		return result;
	}

	Snapshot value{};
	if (!snapshot(value)) {
		return result;
	}

	result.valid = (value.flags & validFlag) != 0U;
	result.baseUnixSeconds = value.baseUnixSeconds;
	result.baseUptimeMs = value.baseUptimeMs;
	result.lastSetUptimeMs = value.lastSetUptimeMs;
	result.lastNetworkSyncUptimeMs = value.lastNetworkSyncUptimeMs;
	result.lastNetworkAttemptUptimeMs = value.lastNetworkAttemptUptimeMs;
	result.setCount = value.setCount;
	result.networkSyncCount = value.networkSyncCount;
	result.lastSetSource = value.lastSetSource;
	result.lastNetworkAttemptValid = (value.flags & lastNetworkAttemptValidFlag) != 0U;
	result.lastNetworkAttemptOk = (value.flags & lastNetworkAttemptOkFlag) != 0U;

	if (currentUnixSeconds(platform, value, result.unixSeconds)) {
		(void) unixSecondsToDateTime(result.unixSeconds, result.now);
	}
	return result;
}

char const* toString(SetSource source) {
	switch (source) {
		case SetSource::Manual:
			return "manual";
		case SetSource::Network:
			return "network";
		case SetSource::None:
			break;
	}
	return "none";
}

} // namespace lexxos::devices::rtc::software
