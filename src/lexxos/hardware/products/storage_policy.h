#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace lexxos::hardware::products {

enum class StorageRole : std::uint8_t {
	System,
	Removable,
};

struct StoragePolicy {
	char const* deviceName;
	char const* mountPath;
	StorageRole role;
	bool hotSwappable;
	bool autoMount;
	bool readOnlyByDefault;
};

enum class StoragePolicyError : std::uint8_t {
	None,
	InvalidName,
	InvalidRole,
	InvalidMountPath,
	DuplicateName,
	DuplicateMountPath,
	UnsafeSystemMedia,
};

// Rules for the migrated Lyra products, not a general VFS path validator.
[[nodiscard]] constexpr StoragePolicyError validateStoragePolicy(std::span<StoragePolicy const> slots) {
	for (std::size_t i = 0; i < slots.size(); ++i) {
		auto const& slot = slots[i];
		if (slot.deviceName == nullptr || std::string_view{slot.deviceName}.empty() || std::string_view{slot.deviceName}.find('/') != std::string_view::npos) {
			return StoragePolicyError::InvalidName;
		}

		if (slot.role != StorageRole::System && slot.role != StorageRole::Removable) {
			return StoragePolicyError::InvalidRole;
		}

		if ((slot.autoMount && slot.mountPath == nullptr)
		    || (slot.mountPath != nullptr
		        && (slot.mountPath[0] != '/' || slot.mountPath[1] == '\0'))) {
			return StoragePolicyError::InvalidMountPath;
		}

		if (slot.role == StorageRole::System
		    && (!slot.readOnlyByDefault || slot.autoMount || slot.hotSwappable || slot.mountPath != nullptr)) {
			return StoragePolicyError::UnsafeSystemMedia;
		}

		for (std::size_t j = 0; j < i; ++j) {
			if (std::string_view{slot.deviceName} == slots[j].deviceName) {
				return StoragePolicyError::DuplicateName;
			}

			if (slot.mountPath != nullptr && slots[j].mountPath != nullptr && std::string_view{slot.mountPath} == slots[j].mountPath) {
				return StoragePolicyError::DuplicateMountPath;
			}
		}
	}

	return StoragePolicyError::None;
}

} // namespace lexxos::hardware::products
