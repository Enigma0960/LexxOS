#pragma once

#include <lexxos/hardware/products/lyra_product.h>

namespace lexxos::composition::products {

struct LyraRuntimeProviders {
	void* context;
	void (*systemStorage)(void*);
	void (*debug)(void*);
	void (*removableStorage)(void*);
	void (*removableDiagnostics)(void*);
	bool (*startHotplug)(void*);
	bool (*configureStorage)(void*, std::span<hardware::products::StoragePolicy const>);
	bool (*startDisplay)(void*){nullptr};
};

enum class LyraRuntimeResult {
	ProvidersInvoked,
	NoTasks,
	InvalidProduct,
	MissingProvider,
	StorageConfigurationFailed,
	TaskCreationFailed,
};

LyraRuntimeResult registerLyraRuntimePrograms(hardware::products::LyraProduct const& product,
    bool diagnosticsEnabled, LyraRuntimeProviders const& providers);
LyraRuntimeResult startLyraRuntimeTasks(hardware::products::LyraProduct const& product,
    LyraRuntimeProviders const& providers);

} // namespace lexxos::composition::products
