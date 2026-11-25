#include <lexxos/composition/products/dev_pico_stage_policy.h>
#include <lexxos/hardware/legacy/dev_pico_board.h>
#include <lexxos/platform/board.h>

namespace lexxos::platform {

LegacyBoardDescriptor const& activeBoardDescriptor() {
	static constexpr auto descriptor = hardware::legacy::devPicoBoard(
	    composition::products::devPicoDisplaySpiHz);

	return descriptor;
}

} // namespace lexxos::platform
