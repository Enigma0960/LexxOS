#include <lexxos/composition/board_runtime.h>

#if defined(LEXXOS_PLATFORM_RK3506)
	#include <lexxos/composition/products/lyra_runtime_providers.h>
	#include <lexxos/composition/products/selected_lyra_product.h>
	#include <lexxos/platform/platform.h>
#endif

namespace lexxos::composition {

void registerBoardRuntimePrograms(os::OperatingSystem& operatingSystem) {
#if defined(LEXXOS_PLATFORM_RK3506)
	constexpr bool diagnosticsEnabled =
	#if defined(LEXXOS_RK3506_SD_DIAGNOSTICS)
	    true;
	#else
	    false;
	#endif
	auto const result = products::registerLyraRuntimePrograms(products::selectedLyraProduct,
	    diagnosticsEnabled, products::lyraRuntimeProviders(operatingSystem));
	if (result != products::LyraRuntimeResult::ProvidersInvoked) {
		platform::writeConsole("Lyra: runtime composition rejected product or missing provider\n");
	}
#else
	(void) operatingSystem;
#endif
}

bool startBoardRuntimeTasks(os::OperatingSystem& operatingSystem) {
#if defined(LEXXOS_PLATFORM_RK3506)
	auto const result = products::startLyraRuntimeTasks(products::selectedLyraProduct,
	    products::lyraRuntimeProviders(operatingSystem));
	return result == products::LyraRuntimeResult::ProvidersInvoked || result == products::LyraRuntimeResult::NoTasks;
#else
	(void) operatingSystem;
	return true;
#endif
}

} // namespace lexxos::composition
