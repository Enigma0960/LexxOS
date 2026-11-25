#pragma once

#include <array>

#include <lexxos/ui/controls/button.h>
#include <lexxos/ui/controls/checkbox.h>
#include <lexxos/ui/controls/dropdown.h>
#include <lexxos/ui/controls/label.h>
#include <lexxos/ui/controls/list_view.h>
#include <lexxos/ui/controls/progress_bar.h>
#include <lexxos/ui/controls/radio_button.h>
#include <lexxos/ui/controls/scroll_list.h>
#include <lexxos/ui/controls/slider.h>
#include <lexxos/ui/controls/text_field.h>

namespace lexxos::ui::controls {

// Indexed by the unchanged guest ABI kind. This is the only control-type
// routing table: GuestView is ignorant of individual widget implementations.
inline constexpr std::array<Handler const*, 13> handlers{
    nullptr,
    &label::handler,
    &button::handler,
    &checkbox::handler,
    &text_field::handler,
    &progress_bar::handler,
    &slider::handler,
    &list_view::handler,
    nullptr, // Column container.
    nullptr, // Row container.
    &scroll_list::handler,
    &dropdown::handler,
    &radio_button::handler,
};

[[nodiscard]] inline Handler const* find(std::uint8_t kind) {
	return kind < handlers.size() ? handlers[kind] : nullptr;
}

[[nodiscard]] inline bool canFocus(Control const& control) {
	auto const* handler = find(control.kind);
	return handler != nullptr && handler->focusable && (control.flags & 1U) == 0U;
}

} // namespace lexxos::ui::controls
