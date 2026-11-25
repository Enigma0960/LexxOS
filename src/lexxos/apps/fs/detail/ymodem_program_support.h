#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>

#include <lexxos/app/console_input.h>
#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/fs/detail/raw_console_mode_guard.h>
#include <lexxos/os/app_context.h>
#include <lexxos/platform/platform_services.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::filesystem_programs::detail::ymodem {

using lexxos::program::ProgramArgumentKind;
using lexxos::program::ProgramArgumentSpec;
using lexxos::program::ProgramArgumentValueType;
using lexxos::program::ProgramDescriptor;
using lexxos::program::ProgramLaunchMode;
using lexxos::program::ProgramMemoryClass;
constexpr std::uint8_t soh = 0x01;
constexpr std::uint8_t stx = 0x02;
constexpr std::uint8_t eot = 0x04;
constexpr std::uint8_t ack = 0x06;
constexpr std::uint8_t nak = 0x15;
constexpr std::uint8_t can = 0x18;
constexpr std::uint8_t crcRequest = static_cast<std::uint8_t>('C');
constexpr std::uint8_t cpmEof = 0x1A;

constexpr std::size_t shortPacketBytes = 128;
constexpr std::size_t longPacketBytes = 1024;
constexpr std::uint8_t maxRetries = 10;
constexpr std::uint32_t startupTimeoutMs = 2000;
constexpr std::uint32_t packetTimeoutMs = 10000;
constexpr std::uint32_t byteTimeoutMs = 1000;
constexpr std::uint32_t receiverWaitMs = 60000;

using PathBuffer = std::array<char, lexxos::vfs::maxPathLength>;
using PacketData = std::array<std::uint8_t, longPacketBytes>;

enum class TransferStatus : std::uint8_t {
	Ok,
	Timeout,
	Cancelled,
	RemoteCancelled,
	ProtocolError,
	CrcError,
	ConsoleUnavailable,
	InvalidPath,
	OpenFailed,
	ReadFailed,
	WriteFailed,
};

struct TransferResult {
	TransferStatus status{TransferStatus::Ok};
	std::uint64_t bytes{0};
	lexxos::vfs::OpenStatus openStatus{lexxos::vfs::OpenStatus::Ok};
	lexxos::vfs::ReadStatus readStatus{lexxos::vfs::ReadStatus::Ok};
	lexxos::vfs::WriteStatus writeStatus{lexxos::vfs::WriteStatus::Ok};
};

struct YmodemPacket {
	bool eot{false};
	bool cancel{false};
	std::uint8_t sequence{0};
	std::size_t dataSize{0};
	PacketData data{};
};

struct YmodemMetadata {
	bool empty{true};
	bool hasSize{false};
	std::string_view fileName{};
	std::uint64_t sizeBytes{0};
};

std::string_view toString(TransferStatus status);
bool elapsed(std::uint32_t startedMs, std::uint32_t nowMs, std::uint32_t timeoutMs);
bool writeRaw(lexxos::os::AppContext& context, std::uint8_t const* data, std::size_t size);
bool writeByte(lexxos::os::AppContext& context, std::uint8_t value);
void sendCancel(lexxos::os::AppContext& context);
TransferStatus readByte(
    lexxos::os::AppContext& context,
    std::uint8_t& value,
    std::uint32_t timeoutMs);
TransferStatus readRequired(
    lexxos::os::AppContext& context,
    std::uint8_t* output,
    std::size_t size,
    std::uint32_t timeoutMs = byteTimeoutMs);
std::uint16_t crc16Xmodem(std::uint8_t const* data, std::size_t size);
TransferStatus receivePacket(
    lexxos::os::AppContext& context,
    YmodemPacket& packet,
    std::uint32_t timeoutMs);
TransferStatus sendPacketOnce(
    lexxos::os::AppContext& context,
    std::uint8_t sequence,
    std::uint8_t const* data,
    std::size_t dataSize);
TransferStatus sendPacketWithRetry(
    lexxos::os::AppContext& context,
    std::uint8_t sequence,
    std::uint8_t const* data,
    std::size_t dataSize);
TransferStatus waitForReceiverRequest(lexxos::os::AppContext& context, std::uint32_t timeoutMs);
bool appendDecimal(char* output, std::size_t capacity, std::uint64_t value, std::size_t& position);
std::string_view basenameOf(std::string_view path);
bool isSafeReceivedName(std::string_view name);
bool parseDecimalField(std::string_view field, std::uint64_t& value);
YmodemMetadata parseMetadata(YmodemPacket const& packet);
bool resolveTargetPath(
    lexxos::os::AppContext& context,
    std::string_view explicitPath,
    std::string_view receivedName,
    PathBuffer& output);
TransferResult receiveYmodem(lexxos::os::AppContext& context, std::string_view targetPath);
bool buildMetadataPacket(
    std::string_view transmittedName,
    std::uint64_t fileSize,
    std::uint8_t* output,
    std::size_t outputSize);
TransferResult sendYmodem(
    lexxos::os::AppContext& context,
    lexxos::vfs::FileHandle& file,
    std::string_view transmittedName);
void writeTransferError(
    lexxos::os::AppContext& context,
    std::string_view commandName,
    TransferResult const& result);

} // namespace lexxos::apps::filesystem_programs::detail::ymodem
