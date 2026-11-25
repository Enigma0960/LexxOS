#pragma once

#include <lexxos/composition/products/lyra_spi_queues.h>
#include <lexxos/devices/rk3506/picosd_media_hooks.h>

namespace lexxos::devices::rk3506 {

class NativePicoSdTransport;

} // namespace lexxos::devices::rk3506

namespace lexxos::app::rk3506 {

class PicoSdStorageService;

} // namespace lexxos::app::rk3506

namespace lexxos::composition::products {

// Единственный владелец на время загрузки для runtime и legacy accessors.
devices::rk3506::NativePicoSdTransport& lyraStorageTransport();
app::rk3506::PicoSdStorageService& lyraStorageService();

// Callback обращается только к переданному сервису; он должен пережить hooks.
platform::rk3506::PicoSdMediaHooks lyraStorageMediaHooks(app::rk3506::PicoSdStorageService& service);

} // namespace lexxos::composition::products
