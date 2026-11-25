#include <string_view>
#include <type_traits>

#include <lexxos/platform/board_identity.h>

static_assert(std::is_trivially_copyable_v<lexxos::platform::BoardDescriptor>);
static_assert(sizeof(lexxos::platform::BoardDescriptor) <= sizeof(void*) * 5);
constexpr lexxos::platform::BoardDescriptor identity{.boardId = "dev_pico"};
static_assert(std::string_view{identity.boardId} == "dev_pico");

int main() {
	return 0;
}
