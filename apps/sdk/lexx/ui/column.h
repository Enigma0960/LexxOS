#pragma once

#include <lexx/ui/layout_group.h>

namespace lexx::ui {

class Column final : public LayoutGroup {
public:
	explicit Column(uint16_t id)
	    : LayoutGroup(id, kind_column) {}
};

} // namespace lexx::ui
