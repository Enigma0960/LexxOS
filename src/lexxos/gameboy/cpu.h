#pragma once

#include <cstdint>

#include <lexxos/gameboy/bus.h>
#include <lexxos/gameboy/cpu_types.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::gameboy {

class Cpu final {
public:
	Cpu() = default;

	void resetToPostBootState();

	[[nodiscard]] CpuStepResult step(Bus& bus);

	[[nodiscard]] std::uint16_t af() const;
	[[nodiscard]] std::uint16_t bc() const;
	[[nodiscard]] std::uint16_t de() const;
	[[nodiscard]] std::uint16_t hl() const;
	[[nodiscard]] std::uint16_t sp() const;
	[[nodiscard]] std::uint16_t pc() const;
	[[nodiscard]] bool ime() const;
	[[nodiscard]] bool halted() const;
	[[nodiscard]] bool stopped() const;

	void setPc(std::uint16_t value);
	void setIme(bool enabled);

private:
	static constexpr std::uint8_t flagZ = 0x80;
	static constexpr std::uint8_t flagN = 0x40;
	static constexpr std::uint8_t flagH = 0x20;
	static constexpr std::uint8_t flagC = 0x10;

	[[nodiscard]] bool flag(std::uint8_t mask) const;
	void setFlag(std::uint8_t mask, bool enabled);
	void setAf(std::uint16_t value);
	void setBc(std::uint16_t value);
	void setDe(std::uint16_t value);
	void setHl(std::uint16_t value);
	void setSp(std::uint16_t value);

	[[nodiscard]] lexxos::vfs::ReadStatus fetch8(Bus& bus, std::uint8_t& value);
	[[nodiscard]] lexxos::vfs::ReadStatus fetch16(Bus& bus, std::uint16_t& value);

	[[nodiscard]] std::uint8_t readRegister(Bus& bus, std::uint8_t index, lexxos::vfs::ReadStatus& status);
	void writeRegister(Bus& bus, std::uint8_t index, std::uint8_t value);

	[[nodiscard]] std::uint8_t inc8(std::uint8_t value);
	[[nodiscard]] std::uint8_t dec8(std::uint8_t value);
	void addA(std::uint8_t value, bool withCarry);
	void subA(std::uint8_t value, bool withCarry);
	void andA(std::uint8_t value);
	void xorA(std::uint8_t value);
	void orA(std::uint8_t value);
	void cpA(std::uint8_t value);
	void addHl(std::uint16_t value);
	void addSpSigned(std::int8_t value);
	void daa();

	[[nodiscard]] bool condition(std::uint8_t conditionCode) const;
	void push16(Bus& bus, std::uint16_t value);
	[[nodiscard]] std::uint16_t pop16(Bus& bus);
	[[nodiscard]] CpuStepResult executeCb(Bus& bus, CpuStepResult result);
	[[nodiscard]] CpuStepResult serviceInterrupt(Bus& bus, CpuStepResult result, std::uint8_t mask, std::uint16_t vector);

	std::uint8_t _a{0};
	std::uint8_t _f{0};
	std::uint8_t _b{0};
	std::uint8_t _c{0};
	std::uint8_t _d{0};
	std::uint8_t _e{0};
	std::uint8_t _h{0};
	std::uint8_t _l{0};
	std::uint16_t _sp{0};
	std::uint16_t _pc{0};
	bool _ime{false};
	bool _imeEnablePending{false};
	bool _halted{false};
	bool _stopped{false};
};

} // namespace lexxos::gameboy
