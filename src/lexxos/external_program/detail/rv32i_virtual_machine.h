#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

#include <lexxos/app/program_input.h>
#include <lexxos/devices/device_resource_lock.h>
#include <lexxos/external_program/detail/rv32i_support.h>
#include <lexxos/external_program/external_program_runner.h>
#include <lexxos/external_program/lexx_resources.h>
#include <lexxos/platform/board.h>
#include <lexxos/runtime/selected_runtime_profile.h>
#include <lexxos/ui/ui_service.h>
#include <lexxos/vfs/file_handle.h>

namespace lexxos::devices {

class DisplayDevice;

} // namespace lexxos::devices

namespace lexxos::os {

class AppContext;

} // namespace lexxos::os

namespace lexxos::external_program::detail {

class Rv32iVirtualMachine final {
public:
	Rv32iVirtualMachine(
	    lexxos::os::AppContext& context,
	    std::uint8_t* memory,
	    std::uint32_t memorySize,
	    LexxExecutableHeader const& header,
	    ExternalProgramOptions const& options,
	    std::string_view executablePath,
	    resources::Index const& resourceIndex);

	~Rv32iVirtualMachine();

	ExternalProgramResult run();

private:
	struct GuestDeviceSlot {
		std::uint32_t kind{0};
		lexxos::devices::DeviceResourceLock lock{};
		void* instance{nullptr};
		lexxos::platform::GpioPin pin{};
		bool inputAllowed{false};
		bool outputAllowed{false};
	};

	std::uint32_t handleGpioOperation(
	    GuestDeviceSlot& slot, std::uint32_t operation,
	    std::uint32_t flags, std::uint32_t arg0,
	    std::uint32_t& result);

	std::uint32_t handleI2cOperation(
	    GuestDeviceSlot& slot, std::uint32_t operation,
	    std::uint32_t address, std::uint32_t buffer,
	    std::uint32_t size, std::uint32_t& result);

	std::uint32_t handleSpiOperation(
	    GuestDeviceSlot& slot, std::uint32_t operation,
	    std::uint32_t mode, std::uint32_t tx,
	    std::uint32_t rx, std::uint32_t size,
	    std::uint32_t baud);

	std::uint32_t handleUartOperation(
	    GuestDeviceSlot& slot, std::uint32_t operation,
	    std::uint32_t arg0, std::uint32_t arg1,
	    std::uint32_t& result);

	void handleDeviceControlSyscall();

	ExternalProgramResult result() const;

	bool rangeValid(std::uint32_t address, std::uint32_t length) const;

	bool hasCapability(std::uint32_t capability) const;

	bool installArguments();

	bool load8(std::uint32_t address, std::uint8_t& value) const;

	bool load16(std::uint32_t address, std::uint16_t& value) const;

	bool load32(std::uint32_t address, std::uint32_t& value) const;

	bool store8(std::uint32_t address, std::uint8_t value);

	bool store16(std::uint32_t address, std::uint16_t value);

	bool store32(std::uint32_t address, std::uint32_t value);

	bool loadGuestString(std::uint32_t address, std::array<char, lexxos::vfs::maxPathLength>& output) const;

	bool guestFileIndex(std::uint32_t fd, std::size_t& output) const;

	void fault(ExternalProgramStatus status, std::uint32_t address, std::uint32_t instruction = 0);

	bool pollCancellation();

	void traceInstruction(std::uint32_t instruction);

	std::uint32_t readRegister(std::uint8_t index) const;

	void writeRegister(std::uint8_t index, std::uint32_t value);

	bool step(std::uint32_t& pc, bool trace);

	void executeBranch(std::uint8_t funct3, std::uint8_t rs1, std::uint8_t rs2, std::uint32_t instruction, std::uint32_t nextPc, std::uint32_t& pc);

	void executeLoad(std::uint8_t funct3, std::uint8_t rd, std::uint8_t rs1, std::uint32_t instruction, std::uint32_t nextPc, std::uint32_t& pc);

	void executeStore(std::uint8_t funct3, std::uint8_t rs1, std::uint8_t rs2, std::uint32_t instruction, std::uint32_t nextPc, std::uint32_t& pc);

#if defined(LEXXOS_VM_SINGLE_WRITE_ALU)
	void executeAlu(bool registerOperand, std::uint8_t funct3,
	    std::uint8_t funct7, std::uint8_t rd, std::uint8_t rs1,
	    std::uint8_t rs2, std::uint32_t instruction,
	    std::uint32_t nextPc, std::uint32_t& pc);
#else
	void executeOpImmediate(
	    std::uint8_t funct3,
	    std::uint8_t funct7,
	    std::uint8_t rd,
	    std::uint8_t rs1,
	    std::uint32_t instruction,
	    std::uint32_t nextPc,
	    std::uint32_t& pc);

	void executeOp(
	    std::uint8_t funct3,
	    std::uint8_t funct7,
	    std::uint8_t rd,
	    std::uint8_t rs1,
	    std::uint8_t rs2,
	    std::uint32_t instruction,
	    std::uint32_t nextPc,
	    std::uint32_t& pc);
#endif

	void handleEcall(std::uint32_t nextPc);

	void releaseGraphics();

	void handleUnixTimeSyscall();

	void handleMemoryInfoSyscall();

	// Graphics HUD uses 5x7 glyphs and a 6-pixel advance.
	bool graphicsTextValid(std::uint32_t address, std::uint32_t& length) const;

	bool drawGraphicsText(lexxos::devices::DisplayDevice& display, int x, int y,
	    std::uint32_t address, int scale, std::uint32_t colors, bool transparent);

	bool graphicsRectValid(lexxos::devices::DisplayDevice const& display,
	    int x, int y, int width, int height) const;

	// Cached images keep RGB565/alpha in host RAM; streamed images keep offsets.
	void loadGraphicsImage(std::uint32_t guestPathAddress, bool streamed);
	bool drawStreamedGraphicsImage(lexxos::devices::DisplayDevice& display,
	    GuestGraphicsImage const& image, int x, int y,
	    int sx, int sy, int width, int height);

	void unloadGraphicsImage(std::uint32_t handle);

	// The system owns rendering, theme, font, focus and keyboard state.
	void handleSystemUi(std::uint32_t operation, std::uint32_t address, std::uint32_t bytes);

	void handleGraphicsSyscall();

	GuestDeviceSlot* guestDevice(std::uint32_t handle);

	bool validDeviceBuffer(std::uint32_t address,
	    std::uint32_t size) const;

	void handleDeviceInfoSyscall();

	void handleDeviceOpenSyscall();

	void handleDeviceCloseSyscall();

	void handlePlayToneSyscall();

	void handleWriteSyscall();

	void handleReadCharSyscall();

	void handleReadSyscall();

	void handlePollKeyEventSyscall();

	// Read-only, paged directory enumeration. The guest never receives a host pointer.
	void handleListDirectorySyscall();

	void handleStatSyscall();

	void handleOpenReadSyscall();

	void handleCloseSyscall();

	void handleSeekSyscall();

	bool guestPath(std::uint32_t address, std::array<char, lexxos::vfs::maxPathLength>& out);

	void handleOpenWriteNewSyscall();

	void handleWriteFileSyscall();

	void handleRemoveFileSyscall();

	void handleRenamePathSyscall();

	void handleMakeDirectorySyscall();

	void handleRemoveDirectorySyscall();

	struct ResourceStream {
		bool active{false};
		std::uint32_t offset{0};
		std::uint32_t size{0};
		std::uint32_t position{0};
	};

	lexxos::os::AppContext& _context;
	std::uint8_t* _memory{nullptr};
	std::uint32_t _memorySize{0};
	LexxExecutableHeader _header{};
	ExternalProgramOptions _options{};
	std::string_view _executablePath{};
	resources::Index _resourceIndex{};
	lexxos::app::program_input::InputMode _inputMode{lexxos::app::program_input::InputMode::Terminal};
	lexxos::ui::UiSession _uiSession{};
	std::array<ResourceStream, lexxMaximumGuestOpenFiles> _resourceStreams{};
	lexxos::app::program_input::EventCursor _guestInputCursor{};
	lexxos::app::program_input::EventCursor _cancelInputCursor{};
	std::array<lexxos::vfs::FileHandle, lexxMaximumGuestOpenFiles> _guestFiles{};
	std::array<GuestDeviceSlot, lexxGuestDeviceSlots> _guestDevices{};
#if LEXXOS_VM_DECODE_CACHE_ENTRIES > 0
	struct DecodeEntry {
		std::uint32_t pc{0xFFFFFFFFU};
		std::uint32_t raw{0};
		std::uint8_t opcode{0};
		std::uint8_t rd{0};
		std::uint8_t funct3{0};
		std::uint8_t rs1{0};
		std::uint8_t rs2{0};
		std::uint8_t funct7{0};
	};

	static_assert(sizeof(DecodeEntry) == 16U);
	std::array<DecodeEntry, LEXXOS_VM_DECODE_CACHE_ENTRIES> _decodeCache{};
#endif
	std::uint32_t _registers[32]{};
	std::uint32_t _pc{0};
	ExternalProgramStatus _status{ExternalProgramStatus::Ok};
	int _exitCode{0};
	std::uint32_t _instructionsExecuted{0};
	std::uint32_t _faultAddress{0};
	std::uint32_t _faultInstruction{0};
	std::uint32_t _unsupportedSyscall{0};
	std::uint32_t _guestArgc{0};
	std::uint32_t _guestArgvAddress{0};
	std::uint32_t _stackTop{0};
	bool _running{false};
	bool _graphicsEnabled{false};
	bool _graphicsInvalidReported{false};
	std::array<GuestGraphicsImage,
	    lexxos::runtime::standardRuntimeProfile.graphics.maxImageHandles>
	    _graphicsImages{};
	std::size_t _graphicsCacheBytes{0};
	std::unique_ptr<std::uint8_t[]> _graphicsStreamScratch{};
	// Per-process rate limiting for slow graphics diagnostic messages.
	std::array<std::uint32_t, 8U> _graphicsSlowLast{};
	std::array<bool, 8U> _graphicsSlowReported{};
};

} // namespace lexxos::external_program::detail
