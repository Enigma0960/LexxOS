#include <lexxos/ui/guest_ui_tree_layout.h>

#include <algorithm>
#include <cstring>

namespace lexxos::ui::tree {

Resolver::Resolver(char const* title, std::uint16_t count, std::uint16_t version,
    TreeNode const* nodes, ResolvedDocument& target, int width, int height)
    : _source{title, count, version, nodes}
    , _target(target)
    , _width(width)
    , _height(height) {
}

Resolver::Resolver(TreeHeaderV3 const& source, TreeNode const* nodes,
    ResolvedDocument& target, int width, int height)
    : Resolver(source.title, source.count, source.version, nodes, target, width, height) {
}

Resolver::Resolver(TreeDocument const& source, ResolvedDocument& target, int width, int height)
    : Resolver(source.title, source.count, source.version, source.nodes, target, width, height) {
}

bool Resolver::resolve(int& scrollMax, int& listIndex) {
	// Проверка формата документа, узлов и связей с родителями.
	if ((_source.version != 2U && _source.version != 3U) || _source.count > (_source.version == 2U ? maxTreeNodes : treeNodeCapacity) || _source.nodes == nullptr || _source.title[31] != '\0' || _width < 96 || _height < 64 || _width > 32767 || _height > 32767) {
		return false;
	}

	for (std::uint16_t i = 0; i < _source.count; ++i) {
		auto const& node = _source.nodes[i];
		auto const& control = node.control;
		bool const container = control.kind == 8U || control.kind == 9U;
		if (control.id == 0U || node.reserved != 0U || control.text[31] != '\0' || (control.flags & ~layout::allowedFlags) != 0U || (control.flags & layout::autoLayout) == 0U || (control.flags & layout::sameRow) != 0U || control.x != 0 || control.y != 0 || control.width != 0 || control.height < 0 || (container && (control.flags != layout::autoLayout || control.height != 0))) {
			return false;
		}

		if (!container && controls::find(control.kind) == nullptr) {
			return false;
		}

		if ((control.flags & layout::tabActive) != 0U && control.kind != 2U) {
			return false;
		}

		bool validParent = node.parentId == 0U;
		for (std::uint16_t j = 0; j < i; ++j) {
			auto const& previous = _source.nodes[j].control;
			if (previous.id == control.id) {
				return false;
			}

			if (previous.id == node.parentId && (previous.kind == 8U || previous.kind == 9U)) {
				validParent = true;
			}
		}

		if (!validParent) {
			return false;
		}
	}

	// Расчёт размеров и размещение проверенного дерева.
	_target = {};
	std::memcpy(_target.title, _source.title, sizeof(_target.title));
	_target.version = _source.version;
	_listIndex = -1;
	int contentHeight = 0;
	if (!measure(0U, false, _width - 2 * layout::left, 0, contentHeight) || contentHeight > 32767 - layout::top || !place(0U, false, layout::left, layout::top, _width - 2 * layout::left, 0)) {
		return false;
	}

	scrollMax = std::max(0, layout::top + contentHeight - (_height - 4));
	listIndex = _listIndex;
	return true;
}

int Resolver::childCount(std::uint16_t parentId) const {
	int count = 0;
	for (std::uint16_t i = 0; i < _source.count; ++i) {
		count += _source.nodes[i].parentId == parentId ? 1 : 0;
	}

	return count;
}

int Resolver::childWidth(int total, int columns, int column) {
	return (total / columns) + (column < (total % columns) ? 1 : 0);
}

bool Resolver::measure(std::uint16_t parentId, bool horizontal, int width, int depth, int& height) const {
	if (depth > 4 || width <= 0) {
		return false;
	}

	int const count = childCount(parentId);
	if (count == 0) {
		height = 0;
		return true;
	}

	int const available = width - (horizontal ? layout::gap * (count - 1) : 0);
	if (available <= 0) {
		return false;
	}

	int column = 0;
	int total = 0;
	for (std::uint16_t i = 0; i < _source.count; ++i) {
		auto const& node = _source.nodes[i];
		if (node.parentId != parentId) {
			continue;
		}

		int const itemWidth = horizontal ? childWidth(available, count, column) : width;
		int itemHeight = 0;
		if (node.control.kind == 8U || node.control.kind == 9U) {
			if (!measure(node.control.id, node.control.kind == 9U,
			        itemWidth, depth + 1, itemHeight)) {
				return false;
			}
		} else {
			auto const* handler = controls::find(node.control.kind);
			itemHeight = node.control.height > 0 ? node.control.height : handler->defaultHeight;
			if (itemWidth < handler->minimumWidth || itemHeight < handler->minimumHeight || itemHeight > _height - layout::top - 4) {
				return false;
			}
		}

		if (itemHeight > 32767 - total) {
			return false;
		}

		total = horizontal ? std::max(total, itemHeight) : total + itemHeight;
		++column;
	}

	if (!horizontal && count > 1) {
		if (layout::gap * (count - 1) > 32767 - total) {
			return false;
		}

		total += layout::gap * (count - 1);
	}

	height = total;
	return true;
}

bool Resolver::place(std::uint16_t parentId, bool horizontal, int x, int y, int width, int depth) {
	int const count = childCount(parentId);
	if (count == 0) {
		return true;
	}

	int const available = width - (horizontal ? layout::gap * (count - 1) : 0);
	int column = 0;
	int cursorX = x;
	int cursorY = y;
	for (std::uint16_t i = 0; i < _source.count; ++i) {
		auto const& node = _source.nodes[i];
		if (node.parentId != parentId) {
			continue;
		}

		int const itemWidth = horizontal ? childWidth(available, count, column) : width;
		int itemHeight = 0;
		if (node.control.kind == 8U || node.control.kind == 9U) {
			if (!measure(node.control.id, node.control.kind == 9U, itemWidth, depth + 1, itemHeight) || !place(node.control.id, node.control.kind == 9U, cursorX, cursorY, itemWidth, depth + 1)) {
				return false;
			}
		} else {
			auto const* handler = controls::find(node.control.kind);
			itemHeight = node.control.height > 0 ? node.control.height : handler->defaultHeight;
			if (_target.count >= maxResolvedControls || cursorX < 0 || cursorY < 0 || cursorX > 32767 - itemWidth || cursorY > 32767 - itemHeight) {
				return false;
			}

			auto& control = _target.controls[_target.count];
			control = node.control;
			control.x = static_cast<std::int16_t>(cursorX);
			control.y = static_cast<std::int16_t>(cursorY);
			control.width = static_cast<std::int16_t>(itemWidth);
			control.height = static_cast<std::int16_t>(itemHeight);
			handler->normalize(control, 0U, true);
			if (handler->usesListItems) {
				if (_listIndex >= 0 && _source.version != 3U) {
					return false; // Для v1/v2 сохраняется ограничение на один список.
				}

				if (_listIndex < 0) {
					_listIndex = _target.count;
				}
			}

			++_target.count;
		}

		if (horizontal) {
			cursorX += itemWidth + layout::gap;
		} else {
			cursorY += itemHeight + layout::gap;
		}

		++column;
	}

	return true;
}

bool resolve(TreeDocument const& source, ResolvedDocument& target,
    int width, int height, int& scrollMax, int& listIndex) {
	Resolver resolver{source, target, width, height};
	return resolver.resolve(scrollMax, listIndex);
}

bool resolve(TreeHeaderV3 const& source, TreeNode const* nodes,
    ResolvedDocument& target, int width, int height, int& scrollMax, int& listIndex) {
	Resolver resolver{source, nodes, target, width, height};
	return resolver.resolve(scrollMax, listIndex);
}

} // namespace lexxos::ui::tree
