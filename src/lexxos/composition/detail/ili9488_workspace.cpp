#include <lexxos/composition/detail/ili9488_workspace.h>

namespace lexxos::composition::detail {

devices::Ili9488Buffers Ili9488Workspace::buffers() {
	return {_pixels, _repeated};
}

} // namespace lexxos::composition::detail
