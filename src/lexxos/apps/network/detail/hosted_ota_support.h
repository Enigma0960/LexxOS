#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <new>
#include <string_view>

#include <lexxos/app/console_input.h>
#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/vfs/vfs.h>

#if defined(LEXXOS_PLATFORM_ESPIDF)
	#include <esp_err.h>
	#include <esp_hosted.h>
	#include <esp_system.h>
	#include <lexxos/devices/espidf/espidf_hosted.h>
#endif

namespace lexxos::apps::network_programs::detail::hosted {

using PathBuffer = std::array<char, lexxos::vfs::maxPathLength>;

using OtaBuffer = std::array<std::uint8_t, 1400>;

constexpr std::uint32_t hostedOtaWorkspaceBytes =
    static_cast<std::uint32_t>(sizeof(PathBuffer) + alignof(PathBuffer) + sizeof(OtaBuffer) + alignof(OtaBuffer));

bool allocateWorkspace(lexxos::os::AppContext& context, PathBuffer*& path, OtaBuffer*& buffer);
#if defined(LEXXOS_PLATFORM_ESPIDF)
enum class OtaRxStatus : std::uint8_t {
	Ok,
	Timeout,
	Cancelled,
	RemoteCancelled,
	ProtocolError,
	CrcError,
	ConsoleUnavailable,
	EmptyBatch,
	InvalidImageMagic,
	OtaWriteFailed,
};

struct OtaRxResult {
	OtaRxStatus status{OtaRxStatus::Ok};
	std::uint64_t bytes{0};
	std::uint64_t expectedBytes{0};
	bool hasExpectedBytes{false};
	std::uint8_t firstByte{0};
	esp_err_t otaError{ESP_OK};
};

void writeEspError(lexxos::os::AppContext& context, char const* prefix, esp_err_t error);

void writeVersion(lexxos::os::AppContext& context, lexxos::devices::espidf_hosted::Version version);

char const* toString(OtaRxStatus status);

OtaRxResult receiveYmodemHostedOta(lexxos::os::AppContext& context, bool dryRun);

bool readFirstChunk(
    lexxos::os::AppContext& context,
    lexxos::vfs::FileHandle& file,
    char const* path,
    OtaBuffer& buffer,
    std::size_t& bytesRead);

bool streamFirmware(
    lexxos::os::AppContext& context,
    lexxos::vfs::FileHandle& file,
    char const* path,
    std::uint64_t fileSize,
    OtaBuffer& buffer,
    std::size_t firstBytesRead,
    std::uint64_t& bytesWritten);
#endif

} // namespace lexxos::apps::network_programs::detail::hosted
