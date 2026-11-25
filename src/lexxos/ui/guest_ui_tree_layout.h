#pragma once

#include <cstdint>

#include <lexxos/ui/guest_ui_layout.h>
#include <lexxos/ui/guest_ui_types.h>

namespace lexxos::ui::tree {

// Документ v2/v3 проверяется и размещается целиком до замены активного представления.
// Без выделения памяти, неограниченной рекурсии и указателей на память гостя.
class Resolver final {
public:
	Resolver(TreeDocument const& source, ResolvedDocument& target, int width, int height);

	Resolver(TreeHeaderV3 const& source, TreeNode const* nodes,
	    ResolvedDocument& target, int width, int height);

	bool resolve(int& scrollMax, int& listIndex);

private:
	int childCount(std::uint16_t parentId) const;

	static int childWidth(int total, int columns, int column);

	bool measure(std::uint16_t parentId, bool horizontal, int width, int depth, int& height) const;

	bool place(std::uint16_t parentId, bool horizontal, int x, int y, int width, int depth);

	struct Source {
		char const* title;
		std::uint16_t count;
		std::uint16_t version;
		TreeNode const* nodes;
	};

	Resolver(char const* title, std::uint16_t count, std::uint16_t version,
	    TreeNode const* nodes, ResolvedDocument& target, int width, int height);

	Source _source;
	ResolvedDocument& _target;
	int _width;
	int _height;
	int _listIndex{-1};
};

bool resolve(TreeDocument const& source, ResolvedDocument& target,
    int width, int height, int& scrollMax, int& listIndex);

bool resolve(TreeHeaderV3 const& source, TreeNode const* nodes,
    ResolvedDocument& target, int width, int height, int& scrollMax, int& listIndex);

} // namespace lexxos::ui::tree
