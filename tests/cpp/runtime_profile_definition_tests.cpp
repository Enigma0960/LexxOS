// No config/ include path, even when legacy target macros are defined.
#include <lexxos/runtime/runtime_profile.h>

constexpr lexxos::runtime::RuntimeProfile portable{};
static_assert(lexxos::runtime::validateRuntimeProfile(portable));
static_assert(portable.kernel.heapBytes == 0);
static_assert(portable.ui.maxResolvedControls == 32);
static_assert(portable.taskStacks.consoleMirrorTaskBytes == 4096);

int main() {
	return 0;
}
