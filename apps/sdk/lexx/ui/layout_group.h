#pragma once

#include <lexx/ui/tree_document.h>

namespace lexx::ui {

class LayoutGroup {
public:
	LayoutGroup(LayoutGroup const&) = delete;
	LayoutGroup& operator=(LayoutGroup const&) = delete;

	[[nodiscard]] uint16_t id() const { return _id; }

	[[nodiscard]] uint8_t kind() const { return _kind; }

protected:
	explicit LayoutGroup(uint16_t id, uint8_t kind)
	    : _id(id)
	    , _kind(kind) {}

private:
	uint16_t _id;
	uint8_t _kind;
};

} // namespace lexx::ui
