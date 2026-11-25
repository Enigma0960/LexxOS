#pragma once

#include <cstdint>

namespace lexxos::platform::rk3506 {

class GicRegisterAccess {
public:
	virtual ~GicRegisterAccess() = default;
	[[nodiscard]] virtual std::uint32_t read(std::uintptr_t address) const noexcept = 0;
	virtual void write(std::uintptr_t address, std::uint32_t value) noexcept = 0;
	[[nodiscard]] virtual std::uintptr_t saveAndDisableIrq() noexcept = 0;
	virtual void restoreIrq(std::uintptr_t state) noexcept = 0;
};

} // namespace lexxos::platform::rk3506
