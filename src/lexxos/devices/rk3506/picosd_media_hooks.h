#pragma once

namespace lexxos::platform::rk3506 {

// Composition задаёт hooks до первого probe; context живёт дольше транспорта.
struct PicoSdMediaHooks {
	void* context{nullptr};
	bool (*isMounted)(void*){nullptr};
	void (*suppressAutoProbe)(void*){nullptr};

	// Неполная привязка запрещает probe и переключение транспорта.
	[[nodiscard]] bool mounted() const;
	void suppress() const;
};

} // namespace lexxos::platform::rk3506
