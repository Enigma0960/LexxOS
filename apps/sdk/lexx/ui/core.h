#pragma once

#if defined(LEXXOS_UI_NATIVE)
	#include <cstddef>
	#include <cstdint>

// Guest SDK types are intentionally simple aliases; the native UI client
// uses the same wire types without linking any RV32I runtime functions.
namespace lexx {

using uint8_t = std::uint8_t;
using uint16_t = std::uint16_t;
using uint32_t = std::uint32_t;
using int16_t = std::int16_t;
using size_t = std::size_t;

} // namespace lexx
#else
	#include <lexx/lexx.h>
#endif

namespace lexx::ui {

// System-owned UI. The guest describes widgets but never renders them.
// graphics capability required; polling also requires keyboard capability.
// The descriptors contain no guest pointers; the OS copies and validates them.
inline constexpr uint16_t protocol_version = 1;
inline constexpr uint16_t max_controls = 8;
inline constexpr uint16_t max_list_items = 8;
inline constexpr uint8_t flag_disabled = 1U;
inline constexpr uint8_t flag_auto_layout = 2U;
inline constexpr uint8_t flag_same_row = 4U;

inline constexpr uint32_t operation_show = 6;
inline constexpr uint32_t operation_poll = 7;
inline constexpr uint32_t operation_close = 8;
inline constexpr uint32_t operation_update = 9;
inline constexpr uint32_t operation_set_items = 10;
inline constexpr uint32_t operation_show_dialog = 11;

enum class Kind : uint8_t {
	Label = 1,
	Button = 2,
	Checkbox = 3,
	TextField = 4,
	ProgressBar = 5,
	Slider = 6,
	ListView = 7,
	ScrollList = 10,
	Dropdown = 11,
	RadioButton = 12,
};
enum class EventType : uint16_t {
	None = 0,
	Activate = 1,
	Change = 2,
	Close = 3,
	DialogResult = 4
};

struct Control {
	uint16_t id{0};
	uint8_t kind{0};
	uint8_t flags{0}; // bit 0: disabled
	int16_t x{0};
	int16_t y{0};
	int16_t width{0};
	int16_t height{0};
	uint32_t value{0}; // Checkbox state; host edits its copy
	char text[32]{}; // UTF-8, NUL-terminated
};

struct Document {
	char title[32]{};
	uint16_t count{0};
	uint16_t version{protocol_version};
	Control controls[max_controls]{};
};

struct Event {
	uint16_t type{0};
	uint16_t id{0};
	uint32_t value{0};
	char text[32]{};
};

// One ListView per window in protocol v1. List data is supplied separately,
// avoiding large string arrays inside the original Document ABI.
struct ListItems {
	uint16_t id{0};
	uint16_t count{0};
	char entries[max_list_items][32]{};
};

struct DialogDescriptor {
	char title[32]{};
	char message[96]{};
	uint16_t id{0};
	uint8_t kind{0};
	uint8_t buttons{0};
};

static_assert(sizeof(DialogDescriptor) == 132);
static_assert(sizeof(Control) == 48);
static_assert(sizeof(Document) == 420);
static_assert(sizeof(Event) == 40);
static_assert(sizeof(ListItems) == 260);

#if !defined(LEXXOS_UI_NATIVE)
// Guest compatibility wrappers. Native applications use Transport instead.
// The OS owns font, theme, focus, keyboard navigation and event dispatch.
inline int show(Document const& document) {
	return static_cast<int>(detail::ecall3(syscall_graphics, operation_show,
	    reinterpret_cast<uint32_t>(&document), 0U));
}

inline int poll(Event& event) {
	return static_cast<int>(detail::ecall3(syscall_graphics, operation_poll,
	    reinterpret_cast<uint32_t>(&event), 0U));
}

inline int close() {
	return static_cast<int>(detail::ecall3(syscall_graphics, operation_close, 0U, 0U));
}

// Change text/value/disabled status without resetting focus or geometry.
// update_control requires an existing ID and the same Kind as at show().
inline int update_control(Control const& control) {
	return static_cast<int>(detail::ecall3(syscall_graphics, operation_update,
	    reinterpret_cast<uint32_t>(&control), 0U));
}

// Supply up to 8 choices to the window's single ListView.
inline int set_items(ListItems const& items) {
	return static_cast<int>(detail::ecall3(syscall_graphics, operation_set_items,
	    reinterpret_cast<uint32_t>(&items), 0U));
}

inline int show_dialog(DialogDescriptor const& dialog) {
	return static_cast<int>(detail::ecall3(syscall_graphics, operation_show_dialog,
	    reinterpret_cast<uint32_t>(&dialog), 0U));
}
#endif

} // namespace lexx::ui
