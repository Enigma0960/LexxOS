#include <lexxos/app/rk3506/picosd_service_state.h>

namespace lexxos::app::rk3506 {

bool PicoSdServiceState::configure(std::span<hardware::products::StoragePolicy const> slots) {
	using hardware::products::StorageRole;
	if (slots.size() != 2 || hardware::products::validateStoragePolicy(slots) != hardware::products::StoragePolicyError::None || std::string_view{slots[0].deviceName} != "sd0" || slots[0].role != StorageRole::System || std::string_view{slots[1].deviceName} != "sd1" || slots[1].role != StorageRole::Removable || !slots[1].hotSwappable || slots[1].mountPath == nullptr || std::string_view{slots[1].mountPath} != "/mnt/sd") {
		return false;
	}

	if (_configured) {
		// Re-registering the same boot configuration must not reset live state.
		// All other fields above have fixed values; these are the variable ones.
		return slots[1].autoMount == _slots[1].autoMount
		    && slots[1].readOnlyByDefault == _slots[1].readOnlyByDefault;
	}

	_slots = {slots[0], slots[1]};
	// Own the snapshot: supported names/paths are canonical static literals,
	// never pointers into caller-owned declaration or temporary string storage.
	_slots[0].deviceName = "sd0";
	_slots[0].mountPath = nullptr;
	_slots[1].deviceName = "sd1";
	_slots[1].mountPath = "/mnt/sd";

	autoEnabled = _slots[1].autoMount;
	writeArmed = false;
	autoSuppressed = false;
	manualReadOnly = false;
	writeFaulted = false;
	_configured = true;

	return true;
}

void PicoSdServiceState::armForMount() {
	auto const* slot = removablePolicy();
	if (slot == nullptr || manualReadOnly || writeFaulted) {
		writeArmed = false;
	} else if (!writeArmed) {
		writeArmed = !slot->readOnlyByDefault;
	}
}

void PicoSdServiceState::cardRemoved() {
	writeArmed = false;
	manualReadOnly = false;
	writeFaulted = false;
}

std::span<hardware::products::StoragePolicy const> PicoSdServiceState::policy() const {
	return _configured ? std::span<hardware::products::StoragePolicy const>{_slots} : std::span<hardware::products::StoragePolicy const>{};
}

hardware::products::StoragePolicy const* PicoSdServiceState::removablePolicy() const {
	return _configured ? &_slots[1] : nullptr;
}

} // namespace lexxos::app::rk3506
