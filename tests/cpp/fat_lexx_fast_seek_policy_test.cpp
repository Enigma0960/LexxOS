#include <cassert>
#include <cstdint>

#include <lexxos/fs/fat/detail/lexx_fast_seek_policy.h>

int main() {
	using lexxos::fs::fat::detail::isLexxPackagePath;
	using lexxos::fs::fat::detail::lexxClusterMapWords;
	using lexxos::fs::fat::detail::lexxFastSeekMinBytes;
	using lexxos::fs::fat::detail::useLexxClusterMap;
	static_assert(lexxClusterMapWords * sizeof(std::uint32_t) == 8192U);
	static_assert(isLexxPackagePath("/lexx/demo.lexx"));
	static_assert(isLexxPackagePath("/Lexx/Demo.LEXX"));
	static_assert(isLexxPackagePath("/x/demo.LeXx"));
	static_assert(!isLexxPackagePath("/lexx/demo.lexx.tmp"));
	static_assert(!isLexxPackagePath("/lexx/demo.sav"));
	static_assert(!isLexxPackagePath(".lexx"));
	static_assert(!useLexxClusterMap(lexxFastSeekMinBytes - 1U));
	static_assert(useLexxClusterMap(lexxFastSeekMinBytes));
	assert(useLexxClusterMap(512U * 1024U * 1024U));
}
