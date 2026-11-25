// Legacy platform entrypoints are bound to the selected product here.
#include <lexxos/composition/products/selected_lyra_configuration.h>
#include <lexxos/hardware/legacy/lyra_board.h>
#include <lexxos/platform/platform.h>
#include <lexxos/platform/rockchip/rk3506/gpio_output.h>

namespace {

constexpr auto kSelectedBoard = lexxos::hardware::legacy::tryLyraBoard(
    lexxos::composition::products::selectedLyraProduct,
    lexxos::composition::products::selectedLyraDisplayEnabled,
    lexxos::composition::products::selectedLyraDisplayHz);
static_assert(kSelectedBoard.has_value());
constexpr auto kBoard = [] {
	auto board = *kSelectedBoard;

	auto const& endpoints = lexxos::composition::products::selectedLyraEndpoints;
	if (endpoints.southbridge) {
		board.interfaces.systemI2c = lexxos::hardware::legacy::lyraI2cPins(*endpoints.southbridge);
	}

	return board;
}();
// No user GPIO is exposed by this product yet. Add explicit composition
// registration before extending the compatibility descriptor.
static_assert(kBoard.userGpio.count == 0);

} // namespace

namespace lexxos::platform {

LegacyBoardDescriptor const& activeBoardDescriptor() {
	return kBoard;
}

LegacyBoardDescriptor const& board() {
	return kBoard;
}

bool configureGpioOutput(GpioPin pin, bool initialValue) {
	if (!composition::products::lyraDisplayOwnsGpio(
	        composition::products::selectedLyraEndpoints,
	        composition::products::selectedLyraDisplayEnabled, {0, pin.number})) {
		return false;
	}

	return rk3506::configureGpio0Output(static_cast<unsigned>(pin.number), initialValue);
}

bool writeGpio(GpioPin pin, bool value) {
	if (!composition::products::lyraDisplayOwnsGpio(
	        composition::products::selectedLyraEndpoints,
	        composition::products::selectedLyraDisplayEnabled, {0, pin.number})) {
		return false;
	}

	return rk3506::writeGpio0Output(static_cast<unsigned>(pin.number), value);
}

} // namespace lexxos::platform
