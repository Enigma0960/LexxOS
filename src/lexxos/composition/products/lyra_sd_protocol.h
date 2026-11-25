#pragma once

namespace lexxos::platform::rk3506 {

class SdProtocol;

} // namespace lexxos::platform::rk3506

namespace lexxos::composition::products {

platform::rk3506::SdProtocol& lyraSdProtocol();

} // namespace lexxos::composition::products
