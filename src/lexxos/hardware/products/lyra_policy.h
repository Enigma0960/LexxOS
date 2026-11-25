#pragma once

#include <lexxos/hardware/products/storage_policy.h>

namespace lexxos::hardware::products {

inline constexpr StoragePolicy lyraSystemStoragePolicy{
    .deviceName = "sd0",
    .mountPath = nullptr,
    .role = StorageRole::System,
    .hotSwappable = false,
    .autoMount = false,
    .readOnlyByDefault = true,
};

} // namespace lexxos::hardware::products
