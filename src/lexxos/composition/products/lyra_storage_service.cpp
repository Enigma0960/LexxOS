#include <lexxos/composition/products/lyra_storage_service.h>

#include <lexxos/app/rk3506/picosd_storage_service.h>
#include <lexxos/composition/products/lyra_sd_protocol.h>
#include <lexxos/devices/rk3506/native_picosd_transport.h>
#include <lexxos/fs/fat/system_fat_mount_backend.h>

namespace lexxos::composition::products {

devices::rk3506::NativePicoSdTransport& lyraStorageTransport() {
	static devices::rk3506::NativePicoSdTransport transport{lyraSpi1Queue(), lyraSdProtocol()};
	return transport;
}

app::rk3506::PicoSdStorageService& lyraStorageService() {
	static fs::fat::SystemFatMountBackend filesystem;
	static app::rk3506::PicoSdStorageService service{lyraStorageTransport(), filesystem};
	return service;
}

platform::rk3506::PicoSdMediaHooks lyraStorageMediaHooks(app::rk3506::PicoSdStorageService& service) {
	return {
	    .context = &service,
	    .isMounted = [](void* context) { return static_cast<app::rk3506::PicoSdStorageService*>(context)->fatMounted(); },
	    .suppressAutoProbe = [](void* context) { static_cast<app::rk3506::PicoSdStorageService*>(context)->suppressUntilRemoval(); },
	};
}

} // namespace lexxos::composition::products
