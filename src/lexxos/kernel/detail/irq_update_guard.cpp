#include <lexxos/kernel/detail/irq_update_guard.h>

namespace lexxos::kernel::detail {

IrqUpdateGuard::IrqUpdateGuard(IrqController& controller) noexcept
    : _controller(controller)
    , _state(controller.beginUpdate()) {
}

IrqUpdateGuard::~IrqUpdateGuard() {
	_controller.endUpdate(_state);
}

} // namespace lexxos::kernel::detail
