#include <lexxos/devices/picocalc/detail/picocalc_southbridge_snapshot_cache.h>

namespace lexxos::devices::detail {

bool PicoCalcSouthbridgeSnapshotCache::refresh(PicoCalcSouthbridge const& southbridge, std::uint32_t nowMs) {
	PicoCalcSouthbridgeSnapshot snapshot{};
	bool const ok = southbridge.readSnapshot(snapshot);

	_lastRefreshMs = nowMs;
	_lastFullRefreshMs = nowMs;
	_lastRefreshOk = ok;
	_hasFullRefresh = true;
	if (ok) {
		_snapshot = snapshot;
		_hasSnapshot = true;
		++_successfulRefreshes;
	} else {
		++_failedRefreshes;
	}

	return ok;
}

bool PicoCalcSouthbridgeSnapshotCache::refreshDue(std::uint32_t nowMs) const {
	if (!_hasFullRefresh) {
		return true;
	}

	if (_refreshIntervalMs == 0) {
		return true;
	}

	std::uint32_t const elapsedMs = nowMs - _lastFullRefreshMs;
	return elapsedMs >= _refreshIntervalMs;
}

bool PicoCalcSouthbridgeSnapshotCache::refreshIfDue(PicoCalcSouthbridge const& southbridge, std::uint32_t nowMs) {
	return !refreshDue(nowMs) || refresh(southbridge, nowMs);
}

void PicoCalcSouthbridgeSnapshotCache::publishKeyboardState(PicoCalcKeyboardState const& state, std::uint32_t nowMs) {
	_snapshot.setKeyboardState(state);
	_publishSuccessfulPartialRefresh(nowMs);
}

void PicoCalcSouthbridgeSnapshotCache::publishKeyboardState(std::uint8_t state, std::uint8_t value, std::uint32_t nowMs) {
	PicoCalcKeyboardState keyboardState{};
	keyboardState.state = state;
	keyboardState.value = value;
	publishKeyboardState(keyboardState, nowMs);
}

void PicoCalcSouthbridgeSnapshotCache::invalidate() {
	_snapshot = {};
	_hasSnapshot = false;
	_lastRefreshOk = false;
	_hasFullRefresh = false;
	_lastRefreshMs = 0;
	_lastFullRefreshMs = 0;
}

void PicoCalcSouthbridgeSnapshotCache::publishLcdBacklight(std::uint8_t value, std::uint32_t nowMs) {
	_snapshot.lcdBacklight = value;
	_snapshot.setValid(PicoCalcSouthbridgeSnapshot::validLcdBacklight);
	_publishSuccessfulPartialRefresh(nowMs);
}

void PicoCalcSouthbridgeSnapshotCache::publishKeyboardBacklight(std::uint8_t value, std::uint32_t nowMs) {
	_snapshot.keyboardBacklight = value;
	_snapshot.setValid(PicoCalcSouthbridgeSnapshot::validKeyboardBacklight);
	_publishSuccessfulPartialRefresh(nowMs);
}

#if defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
void PicoCalcSouthbridgeSnapshotCache::publishJcsRtcConfig(PicoCalcJcsRtcConfig config, std::uint32_t nowMs) {
	_snapshot.jcsRtcConfig = config;
	_snapshot.setValid(PicoCalcSouthbridgeSnapshot::validJcsRtcConfig);
	_publishSuccessfulPartialRefresh(nowMs);
}

void PicoCalcSouthbridgeSnapshotCache::publishJcsRtcDateTime(
    lexxos::devices::rtc::DateTime const& value,
    std::uint32_t nowMs) {
	_snapshot.jcsRtcDateTime = value;
	_snapshot.setValid(PicoCalcSouthbridgeSnapshot::validJcsRtcDateTime);
	_publishSuccessfulPartialRefresh(nowMs);
}

void PicoCalcSouthbridgeSnapshotCache::publishJcsRtcAlarm(
    lexxos::devices::rtc::DateTime const& value,
    std::uint32_t nowMs) {
	_snapshot.jcsRtcAlarm = value;
	_snapshot.setValid(PicoCalcSouthbridgeSnapshot::validJcsRtcAlarm);
	_publishSuccessfulPartialRefresh(nowMs);
}
#endif

void PicoCalcSouthbridgeSnapshotCache::setRefreshIntervalMs(std::uint32_t intervalMs) {
	_refreshIntervalMs = intervalMs;
}

std::uint32_t PicoCalcSouthbridgeSnapshotCache::refreshIntervalMs() const {
	return _refreshIntervalMs;
}

bool PicoCalcSouthbridgeSnapshotCache::hasSnapshot() const {
	return _hasSnapshot;
}

PicoCalcSouthbridgeSnapshot const& PicoCalcSouthbridgeSnapshotCache::snapshot() const {
	return _snapshot;
}

bool PicoCalcSouthbridgeSnapshotCache::lastRefreshOk() const {
	return _lastRefreshOk;
}

std::uint32_t PicoCalcSouthbridgeSnapshotCache::lastRefreshMs() const {
	return _lastRefreshMs;
}

std::uint32_t PicoCalcSouthbridgeSnapshotCache::lastFullRefreshMs() const {
	return _lastFullRefreshMs;
}

std::uint32_t PicoCalcSouthbridgeSnapshotCache::successfulRefreshes() const {
	return _successfulRefreshes;
}

std::uint32_t PicoCalcSouthbridgeSnapshotCache::failedRefreshes() const {
	return _failedRefreshes;
}

void PicoCalcSouthbridgeSnapshotCache::_publishSuccessfulPartialRefresh(std::uint32_t nowMs) {
	_lastRefreshMs = nowMs;
	_hasSnapshot = true;
	++_successfulRefreshes;
}

} // namespace lexxos::devices::detail
