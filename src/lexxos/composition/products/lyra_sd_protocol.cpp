#include <lexxos/composition/products/lyra_sd_protocol.h>

#include <lexxos/composition/products/lyra_sd_dma.h>
#include <lexxos/composition/products/lyra_sd_spi.h>
#include <lexxos/devices/rk3506/native_sd_protocol_io.h>
#include <lexxos/devices/rk3506/sd_protocol.h>

namespace lexxos::composition::products {

platform::rk3506::SdProtocol& lyraSdProtocol() {
	static platform::rk3506::NativeSdProtocolIo io{lyraSdSpi(), lyraSdDma()};
	static platform::rk3506::SdProtocol protocol{io};
	return protocol;
}

} // namespace lexxos::composition::products
