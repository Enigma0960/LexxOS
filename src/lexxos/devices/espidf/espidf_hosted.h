#pragma once

#include <cstdint>

#include <lexxos/platform/board.h>

namespace lexxos::devices::espidf_hosted {

struct Version {
	std::uint32_t major{0};
	std::uint32_t minor{0};
	std::uint32_t patch{0};
	bool valid{false};
};

struct AppDescriptor {
	bool valid{false};
	bool magicValid{false};
	std::uint32_t magicWord{0};
	std::uint32_t secureVersion{0};
	char projectName[33]{};
	char version[33]{};
	char idfVersion[33]{};
	char buildDate[17]{};
	char buildTime[17]{};
};

struct Info {
	bool supported{false};
	bool backendStartRequested{false};
	bool backendStarted{false};
	int backendStartError{0};
	char backendStartErrorName[32]{};
	Version hostVersion{};
	Version coprocessorVersion{};
	int versionError{0};
	char versionErrorName[32]{};
	AppDescriptor app{};
	int appError{0};
	char appErrorName[32]{};
	bool compatibilityKnown{false};
	bool majorMinorCompatible{false};
	int versionComparison{0};
};

[[nodiscard]] bool isHostedBoard(lexxos::platform::BoardDescriptor const& board);
[[nodiscard]] bool activateSupported(Version version);
[[nodiscard]] Info readInfo(lexxos::platform::BoardDescriptor const& board, bool startWifiBackend);
[[nodiscard]] char const* errorName(int error);

} // namespace lexxos::devices::espidf_hosted
