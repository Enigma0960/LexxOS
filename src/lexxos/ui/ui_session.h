#pragma once

#include <memory>

#include <lexxos/ui/guest_ui.h>

namespace lexxos::ui {

class UiSession {
public:
	[[nodiscard]] bool active() const { return _view.active(); }

	[[nodiscard]] std::uint32_t owner() const { return _owner; }

	[[nodiscard]] UiBudget budget() const { return _budget; }

	// Host задаёт бюджет процесса до показа первого окна.
	bool configureBudget(UiBudget requested);

private:
	friend class UiService;
	GuestView _view{};
	UiBudget _budget{};
	TreeHeaderV3 _treeHeader{};
	std::unique_ptr<TreeNode[]> _treeNodes{};
	std::uint32_t _owner{0};
	bool _needsRedraw{false};
};

} // namespace lexxos::ui
