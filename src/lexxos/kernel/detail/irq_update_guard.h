#pragma once

#include <lexxos/kernel/irq_controller.h>

namespace lexxos::kernel::detail {

class IrqUpdateGuard {
public:
	explicit IrqUpdateGuard(IrqController& controller) noexcept;

	~IrqUpdateGuard();

	IrqUpdateGuard(IrqUpdateGuard const&) = delete;
	IrqUpdateGuard& operator=(IrqUpdateGuard const&) = delete;

private:
	IrqController& _controller;
	IrqController::UpdateState _state;
};

} // namespace lexxos::kernel::detail
