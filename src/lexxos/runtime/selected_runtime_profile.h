#pragma once

#include <lexxos/runtime/runtime_profile.h>

// Сборка выбирает полную композицию для каждой цели. Отдельные host-инструменты
// и контрактные тесты используют базовые лимиты без SDK/config-заголовков.
#if defined(LEXXOS_RUNTIME_PROFILE_HEADER)
	#include LEXXOS_RUNTIME_PROFILE_HEADER
#else
namespace lexxos::runtime {

inline constexpr RuntimeProfile standardRuntimeProfile = [] {
	RuntimeProfile profile{};
	#if defined(LEXXOS_BOARD_NAME)
	profile.origin.board = LEXXOS_BOARD_NAME;
	#endif
	return profile;
}();

} // namespace lexxos::runtime
#endif

namespace lexxos::runtime {

static_assert(validateRuntimeProfile(standardRuntimeProfile), "Invalid LexxOS runtime memory profile");

[[nodiscard]] constexpr RuntimeProfile const& runtimeProfile() {
	return standardRuntimeProfile;
}

} // namespace lexxos::runtime
