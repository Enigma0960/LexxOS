#include <lexxos/app/rk3506/picosd_storage_service.h>

#include <lexxos/fs/fat/fat_mount_backend.h>

namespace lexxos::app::rk3506 {

PicoSdStorageService::PicoSdStorageService(devices::SdMediaTransport& transport, fs::fat::FatMountBackend& filesystem)
    : _transport(transport)
    , _filesystem(filesystem)
    , _disk(transport, _state.writeArmed, writeFailed, this) {
}

bool PicoSdStorageService::configure(std::span<hardware::products::StoragePolicy const> slots) {
	return _state.configure(slots);
}

bool PicoSdStorageService::configured() const {
	return _state.removablePolicy() != nullptr;
}

devices::SdCardBackend& PicoSdStorageService::backend() {
	return _disk;
}

void PicoSdStorageService::writeFailed(void* context) {
	auto& service = *static_cast<PicoSdStorageService*>(context);
	service._state.writeFaulted = true;
	service.suppressUntilRemoval();
}

void PicoSdStorageService::suppressUntilRemoval() {
	_state.autoSuppressed = true;
}

void PicoSdStorageService::manualMountFinished(bool success) {
	if (!success) {
		_state.writeArmed = false;
	}
}

void PicoSdStorageService::notifyManualUnmount() {
	_state.autoSuppressed = true;
	_state.writeArmed = false;
}

bool PicoSdStorageService::prepareManualMount() {
	auto const* slot = _state.removablePolicy();
	if (slot == nullptr || !_transport.ready() || _filesystem.isMounted("/mnt/sd")) {
		return false;
	}

	_state.autoSuppressed = true; // Respect explicit manual mounting/unmounting.
	_state.armForMount();
	return true;
}

bool PicoSdStorageService::fatMounted() const {
	return _filesystem.isMounted("/mnt/sd");
}

} // namespace lexxos::app::rk3506
