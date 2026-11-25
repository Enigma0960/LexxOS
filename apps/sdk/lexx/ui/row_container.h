#pragma once

#include <lexx/ui/layout_group.h>

namespace lexx::ui {

class RowContainer final : public LayoutGroup {
public:
	explicit RowContainer(uint16_t id)
	    : LayoutGroup(id, kind_row) {}
};

} // namespace lexx::ui
