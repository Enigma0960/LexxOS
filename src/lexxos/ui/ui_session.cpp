#include <lexxos/ui/ui_session.h>

namespace lexxos::ui {

bool UiSession::configureBudget(UiBudget requested) {
	if (_view.active() || !requested.valid()) {
		return false;
	}
	_budget = requested;
	return true;
}

} // namespace lexxos::ui
