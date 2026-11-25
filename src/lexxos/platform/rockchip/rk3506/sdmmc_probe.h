#pragma once

#include <cstdint>

namespace lexxos::platform::rk3506 {

struct SdmmcStatus {
	std::uint32_t version{};
	std::uint32_t hostConfig{};
	std::uint32_t control{};
	std::uint32_t powerEnable{};
	std::uint32_t clockDivider{};
	std::uint32_t clockEnable{};
	std::uint32_t cardType{};
	std::uint32_t cardDetect{};
	std::uint32_t status{};
	std::uint32_t interruptStatus{};
	bool ready{};
};

struct SdmmcResult {
	std::uint32_t rca{};
	std::uint32_t ocr{};
	std::uint32_t lastInterrupt{};
	std::uint32_t lastCommand{};
};

[[nodiscard]] SdmmcStatus sdmmcStatus();
[[nodiscard]] SdmmcResult sdmmcResult();
[[nodiscard]] bool sdmmcProbe();
[[nodiscard]] bool sdmmcReady();
[[nodiscard]] bool sdmmcReadSector(std::uint32_t lba, std::uint8_t* destination);

} // namespace lexxos::platform::rk3506
