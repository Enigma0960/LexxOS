#include <lexxos/external_program/external_program_runner.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

#include <lexxos/app/console_input.h>
#include <lexxos/app/program_input.h>
#include <lexxos/devices/audio_device.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/input_device.h>
#include <lexxos/io/output_stream.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/app_context.h>
#include <lexxos/platform/platform_services.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::external_program {
namespace {

	constexpr std::uint32_t lexxSupportedCapabilities =
	    LexxCapabilityConsoleText
	    | LexxCapabilityKeyboard
	    | LexxCapabilityStorageRead
	    | LexxCapabilityStorageWrite
	    | LexxCapabilityAudio;
	constexpr std::uint32_t lexxGuestEventSize = 12;
	constexpr std::uint32_t lexxHeaderReservedOffset = 44;
	constexpr std::uint32_t lexxGuestDirEntrySize = 76;
	constexpr std::uint32_t lexxGuestDirPageSize = 8;
	constexpr std::uint32_t lexxGuestStatSize = 12;
	constexpr std::uint32_t lexxGuestDirectoryNameSize = 64;
	constexpr std::uint32_t lexxSyscallError = 0xFFFFFFFFU;
	constexpr std::uint32_t lexxSyscallCancelled = 0xFFFFFFFEU;
	static_assert(lexxHeaderReservedOffset < lexxExecutableHeaderSize);

	enum class VmSyscall : std::uint32_t {
		Exit = 0,
		Write = 1,
		ReadChar = 2,
		Read = 3,
		PollKeyEvent = 4,
		Yield = 5,
		Argc = 6,
		Argv = 7,
		OpenRead = 8,
		Close = 9,
		ListDirectory = 10,
		Stat = 11,
		Seek = 12,
		OpenWriteNew = 13,
		WriteFile = 14,
		RemoveFile = 15,
		RenamePath = 16,
		MakeDirectory = 17,
		RemoveDirectory = 18,
		Milliseconds = 19,
		PlayTone = 20,
	};

	std::uint16_t readLe16(std::uint8_t const* data) {
		return static_cast<std::uint16_t>(data[0])
		    | static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) << 8U);
	}

	std::uint32_t readLe32(std::uint8_t const* data) {
		return static_cast<std::uint32_t>(data[0])
		    | (static_cast<std::uint32_t>(data[1]) << 8U)
		    | (static_cast<std::uint32_t>(data[2]) << 16U)
		    | (static_cast<std::uint32_t>(data[3]) << 24U);
	}

	void writeLe16(std::uint8_t* data, std::uint16_t value) {
		data[0] = static_cast<std::uint8_t>(value & 0xFFU);
		data[1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
	}

	void writeLe32(std::uint8_t* data, std::uint32_t value) {
		data[0] = static_cast<std::uint8_t>(value & 0xFFU);
		data[1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
		data[2] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
		data[3] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
	}

	std::uint32_t alignDown4(std::uint32_t value) {
		return value & ~0x03U;
	}

	std::uint32_t alignUp4(std::uint32_t value) {
		return (value + 3U) & ~0x03U;
	}

	bool rangeFits(std::uint32_t offset, std::uint32_t length, std::uint32_t limit) {
		return offset <= limit && length <= limit - offset;
	}

	bool fileRangeFits(std::uint32_t offset, std::uint32_t length, std::uint64_t fileSize) {
		auto const wideOffset = static_cast<std::uint64_t>(offset);
		return wideOffset <= fileSize && static_cast<std::uint64_t>(length) <= fileSize - wideOffset;
	}

	bool bytesAreZero(std::uint8_t const* data, std::size_t size) {
		for (std::size_t index = 0; index < size; ++index) {
			if (data[index] != 0) {
				return false;
			}
		}
		return true;
	}

	std::size_t cstringLength(char const* text) {
		if (text == nullptr) {
			return 0;
		}
		std::size_t length = 0;
		while (text[length] != '\0') {
			++length;
		}
		return length;
	}

	std::int32_t signExtend(std::uint32_t value, std::uint8_t bits) {
		auto const signBit = 1U << (bits - 1U);
		return static_cast<std::int32_t>((value ^ signBit) - signBit);
	}

	std::int32_t immediateI(std::uint32_t instruction) {
		return signExtend(instruction >> 20U, 12);
	}

	std::int32_t immediateS(std::uint32_t instruction) {
		auto const value = ((instruction >> 7U) & 0x1FU) | (((instruction >> 25U) & 0x7FU) << 5U);
		return signExtend(value, 12);
	}

	std::int32_t immediateB(std::uint32_t instruction) {
		auto const value = (((instruction >> 31U) & 0x01U) << 12U)
		    | (((instruction >> 7U) & 0x01U) << 11U)
		    | (((instruction >> 25U) & 0x3FU) << 5U)
		    | (((instruction >> 8U) & 0x0FU) << 1U);
		return signExtend(value, 13);
	}

	std::int32_t immediateJ(std::uint32_t instruction) {
		auto const value = (((instruction >> 31U) & 0x01U) << 20U)
		    | (((instruction >> 12U) & 0xFFU) << 12U)
		    | (((instruction >> 20U) & 0x01U) << 11U)
		    | (((instruction >> 21U) & 0x3FFU) << 1U);
		return signExtend(value, 21);
	}

	std::uint32_t immediateU(std::uint32_t instruction) {
		return instruction & 0xFFFFF000U;
	}

	bool isCtrlCEvent(lexxos::devices::InputEvent const& event) {
		if (event.type != lexxos::devices::InputEventType::KeyDown) {
			return false;
		}
		if (event.character == 0x03U) {
			return true;
		}
		return event.key == lexxos::devices::KeyCode::C
		    && (event.modifiers & lexxos::devices::InputModifierCtrl) != 0;
	}

	void writeHexNibble(lexxos::os::AppContext& context, std::uint8_t value) {
		char const text[] = {static_cast<char>(value < 10 ? ('0' + value) : ('a' + value - 10)), '\0'};
		context.write(text);
	}

	void writeHex32(lexxos::os::AppContext& context, std::uint32_t value) {
		context.write("0x");
		for (int shift = 28; shift >= 0; shift -= 4) {
			writeHexNibble(context, static_cast<std::uint8_t>((value >> static_cast<unsigned>(shift)) & 0x0FU));
		}
	}

	bool parseHeader(std::uint8_t const* data, std::size_t size, LexxExecutableHeader& output) {
		if (size < lexxExecutableHeaderSize) {
			return false;
		}
		output = {
		    .formatVersion = readLe16(data + 4),
		    .headerSize = readLe16(data + 6),
		    .machine = readLe16(data + 8),
		    .abiVersion = readLe16(data + 10),
		    .flags = readLe32(data + 12),
		    .requiredCapabilities = readLe32(data + 16),
		    .entryOffset = readLe32(data + 20),
		    .imageOffset = readLe32(data + 24),
		    .imageSize = readLe32(data + 28),
		    .memorySize = readLe32(data + 32),
		    .stackSize = readLe32(data + 36),
		    .bssSize = readLe32(data + 40),
		};
		return readLe32(data) == lexxExecutableMagic;
	}

	ExternalProgramStatus validateHeader(
	    LexxExecutableHeader const& header,
	    std::uint64_t fileSize,
	    bool reservedBytesAreZero) {
		if (header.formatVersion != lexxExecutableFormatVersion) {
			return ExternalProgramStatus::UnsupportedVersion;
		}
		if (header.machine != lexxMachineRv32i) {
			return ExternalProgramStatus::UnsupportedMachine;
		}
		if (header.abiVersion != lexxExecutableAbiVersion) {
			return ExternalProgramStatus::UnsupportedAbi;
		}
		if ((header.requiredCapabilities & ~lexxSupportedCapabilities) != 0U) {
			return ExternalProgramStatus::UnsupportedCapabilities;
		}
		if (header.flags != 0U || !reservedBytesAreZero) {
			return ExternalProgramStatus::InvalidHeader;
		}
		if (header.headerSize != lexxExecutableHeaderSize || header.imageOffset < header.headerSize) {
			return ExternalProgramStatus::InvalidHeader;
		}
		if ((header.memorySize & 0x03U) != 0U) {
			return ExternalProgramStatus::InvalidHeader;
		}
		if (header.imageSize == 0 || header.memorySize == 0 || header.imageSize > header.memorySize) {
			return ExternalProgramStatus::InvalidImage;
		}
		if (header.memorySize > lexxMaximumGuestMemoryBytes) {
			return ExternalProgramStatus::ImageTooLarge;
		}
		if (!fileRangeFits(header.imageOffset, header.imageSize, fileSize)) {
			return ExternalProgramStatus::InvalidImage;
		}
		if ((header.entryOffset & 0x03U) != 0U || !rangeFits(header.entryOffset, 4U, header.memorySize)) {
			return ExternalProgramStatus::InvalidImage;
		}
		if (header.stackSize > header.memorySize) {
			return ExternalProgramStatus::InvalidImage;
		}
		return ExternalProgramStatus::Ok;
	}

	class Rv32iVirtualMachine final {
	public:
		Rv32iVirtualMachine(
		    lexxos::os::AppContext& context,
		    std::uint8_t* memory,
		    std::uint32_t memorySize,
		    LexxExecutableHeader const& header,
		    ExternalProgramOptions const& options)
		    : _context(context)
		    , _memory(memory)
		    , _memorySize(memorySize)
		    , _header(header)
		    , _options(options) {
		}

		ExternalProgramResult run() {
			lexxos::app::program_input::reset(_guestInputCursor);
			lexxos::app::program_input::reset(_cancelInputCursor);
			if (!installArguments()) {
				_status = ExternalProgramStatus::ArgumentsTooLarge;
				return result();
			}
			_registers[2] = _stackTop;
			_registers[10] = _guestArgc;
			_registers[11] = _guestArgvAddress;
			_pc = _header.entryOffset;
			_running = true;

			while (_running) {
				if (_options.maxInstructions != 0U && _instructionsExecuted >= _options.maxInstructions) {
					_status = ExternalProgramStatus::InstructionLimit;
					break;
				}
				if ((_instructionsExecuted & 0xFFU) == 0U && pollCancellation()) {
					_status = ExternalProgramStatus::Cancelled;
					break;
				}
				step();
				_registers[0] = 0;
			}

			return result();
		}

	private:
		ExternalProgramResult result() const {
			return {
			    .status = _status,
			    .exitCode = _exitCode,
			    .instructionsExecuted = _instructionsExecuted,
			    .faultAddress = _faultAddress,
			    .faultInstruction = _faultInstruction,
			    .unsupportedSyscall = _unsupportedSyscall,
			};
		}

		bool rangeValid(std::uint32_t address, std::uint32_t length) const {
			return rangeFits(address, length, _memorySize);
		}

		bool hasCapability(std::uint32_t capability) const {
			return (_header.requiredCapabilities & capability) != 0U;
		}

		bool installArguments() {
			_stackTop = alignDown4(_memorySize);
			_guestArgc = 0;
			_guestArgvAddress = 0;

			if (_options.guestArgc <= 0 || _options.guestArgv == nullptr) {
				return true;
			}

			auto const count = static_cast<std::uint32_t>(_options.guestArgc);
			if (count > lexxMaximumGuestArguments) {
				return false;
			}

			std::uint32_t stringsBytes = 0;
			for (std::uint32_t index = 0; index < count; ++index) {
				auto const length = static_cast<std::uint32_t>(cstringLength(_options.guestArgv[index]));
				if (length >= _memorySize || stringsBytes > _memorySize - (length + 1U)) {
					return false;
				}
				stringsBytes += length + 1U;
			}

			auto const tableBytes = (count + 1U) * 4U;
			if (tableBytes > _memorySize || stringsBytes > _memorySize - tableBytes) {
				return false;
			}

			auto const totalBytes = alignUp4(tableBytes + stringsBytes);
			if (totalBytes > _memorySize) {
				return false;
			}

			auto const argvAddress = _memorySize - totalBytes;
			if (argvAddress < _header.imageSize) {
				return false;
			}

			auto stringCursor = argvAddress + tableBytes;
			for (std::uint32_t index = 0; index < count; ++index) {
				auto const* argument = _options.guestArgv[index] != nullptr ? _options.guestArgv[index] : "";
				auto const length = static_cast<std::uint32_t>(cstringLength(argument));
				writeLe32(_memory + argvAddress + index * 4U, stringCursor);
				std::memcpy(_memory + stringCursor, argument, length);
				_memory[stringCursor + length] = 0;
				stringCursor += length + 1U;
			}
			writeLe32(_memory + argvAddress + count * 4U, 0);

			_guestArgc = count;
			_guestArgvAddress = argvAddress;
			_stackTop = alignDown4(argvAddress);
			return true;
		}

		bool load8(std::uint32_t address, std::uint8_t& value) const {
			if (!rangeValid(address, 1)) {
				return false;
			}
			value = _memory[address];
			return true;
		}

		bool load16(std::uint32_t address, std::uint16_t& value) const {
			if ((address & 0x01U) != 0U || !rangeValid(address, 2)) {
				return false;
			}
			value = readLe16(_memory + address);
			return true;
		}

		bool load32(std::uint32_t address, std::uint32_t& value) const {
			if ((address & 0x03U) != 0U || !rangeValid(address, 4)) {
				return false;
			}
			value = readLe32(_memory + address);
			return true;
		}

		bool store8(std::uint32_t address, std::uint8_t value) {
			if (!rangeValid(address, 1)) {
				return false;
			}
			_memory[address] = value;
			return true;
		}

		bool store16(std::uint32_t address, std::uint16_t value) {
			if ((address & 0x01U) != 0U || !rangeValid(address, 2)) {
				return false;
			}
			writeLe16(_memory + address, value);
			return true;
		}

		bool store32(std::uint32_t address, std::uint32_t value) {
			if ((address & 0x03U) != 0U || !rangeValid(address, 4)) {
				return false;
			}
			writeLe32(_memory + address, value);
			return true;
		}

		bool loadGuestString(std::uint32_t address, std::array<char, lexxos::vfs::maxPathLength>& output) const {
			for (std::size_t index = 0; index < output.size(); ++index) {
				std::uint8_t value = 0;
				if (!load8(address + static_cast<std::uint32_t>(index), value)) {
					return false;
				}
				output[index] = static_cast<char>(value);
				if (value == 0) {
					return true;
				}
			}
			output[output.size() - 1U] = '\0';
			return false;
		}

		bool guestFileIndex(std::uint32_t fd, std::size_t& output) const {
			if (fd < lexxGuestFileDescriptorBase) {
				return false;
			}
			auto const index = fd - lexxGuestFileDescriptorBase;
			if (index >= _guestFiles.size()) {
				return false;
			}
			output = static_cast<std::size_t>(index);
			return true;
		}

		void fault(ExternalProgramStatus status, std::uint32_t address, std::uint32_t instruction = 0) {
			_status = status;
			_faultAddress = address;
			_faultInstruction = instruction;
			_running = false;
		}

		bool pollCancellation() {
			if (_context.cancellationRequested()) {
				return true;
			}
			if (!_context.hasProgramFocus()) {
				return false;
			}

			lexxos::devices::InputEvent event{};
			while (lexxos::app::program_input::pollEvent(_cancelInputCursor, event)) {
				if (isCtrlCEvent(event)) {
					_context.requestCancellation();
					return true;
				}
			}
			return _context.cancellationRequested();
		}

		void traceInstruction(std::uint32_t instruction) {
			if (!_options.trace) {
				return;
			}
			_context.write("rv32i pc=");
			writeHex32(_context, _pc);
			_context.write(" inst=");
			writeHex32(_context, instruction);
			_context.writeLine("");
		}

		void step() {
			std::uint32_t instruction = 0;
			if (!load32(_pc, instruction)) {
				fault(ExternalProgramStatus::InstructionAddressFault, _pc);
				return;
			}
			traceInstruction(instruction);
			++_instructionsExecuted;

			auto const opcode = instruction & 0x7FU;
			auto const rd = static_cast<std::uint8_t>((instruction >> 7U) & 0x1FU);
			auto const funct3 = static_cast<std::uint8_t>((instruction >> 12U) & 0x07U);
			auto const rs1 = static_cast<std::uint8_t>((instruction >> 15U) & 0x1FU);
			auto const rs2 = static_cast<std::uint8_t>((instruction >> 20U) & 0x1FU);
			auto const funct7 = static_cast<std::uint8_t>((instruction >> 25U) & 0x7FU);
			auto const nextPc = _pc + 4U;

			switch (opcode) {
				case 0x37: // LUI
					_registers[rd] = immediateU(instruction);
					_pc = nextPc;
					return;
				case 0x17: // AUIPC
					_registers[rd] = _pc + immediateU(instruction);
					_pc = nextPc;
					return;
				case 0x6F: // JAL
					_registers[rd] = nextPc;
					_pc = _pc + static_cast<std::uint32_t>(immediateJ(instruction));
					return;
				case 0x67: { // JALR
					if (funct3 != 0) {
						fault(ExternalProgramStatus::IllegalInstruction, _pc, instruction);
						return;
					}
					auto const target = (_registers[rs1] + static_cast<std::uint32_t>(immediateI(instruction))) & ~1U;
					_registers[rd] = nextPc;
					_pc = target;
					return;
				}
				case 0x63: // BRANCH
					executeBranch(funct3, rs1, rs2, instruction, nextPc);
					return;
				case 0x03: // LOAD
					executeLoad(funct3, rd, rs1, instruction, nextPc);
					return;
				case 0x23: // STORE
					executeStore(funct3, rs1, rs2, instruction, nextPc);
					return;
				case 0x13: // OP-IMM
					executeOpImmediate(funct3, funct7, rd, rs1, instruction, nextPc);
					return;
				case 0x33: // OP
					executeOp(funct3, funct7, rd, rs1, rs2, instruction, nextPc);
					return;
				case 0x0F: // FENCE: all guest I/O is already mediated by host syscalls.
					_pc = nextPc;
					return;
				case 0x73: // SYSTEM
					if (instruction == 0x00000073U) {
						handleEcall(nextPc);
						return;
					}
					fault(ExternalProgramStatus::IllegalInstruction, _pc, instruction);
					return;
				default:
					fault(ExternalProgramStatus::IllegalInstruction, _pc, instruction);
					return;
			}
		}

		void executeBranch(std::uint8_t funct3, std::uint8_t rs1, std::uint8_t rs2, std::uint32_t instruction, std::uint32_t nextPc) {
			bool take = false;
			switch (funct3) {
				case 0x0:
					take = _registers[rs1] == _registers[rs2];
					break;
				case 0x1:
					take = _registers[rs1] != _registers[rs2];
					break;
				case 0x4:
					take = static_cast<std::int32_t>(_registers[rs1]) < static_cast<std::int32_t>(_registers[rs2]);
					break;
				case 0x5:
					take = static_cast<std::int32_t>(_registers[rs1]) >= static_cast<std::int32_t>(_registers[rs2]);
					break;
				case 0x6:
					take = _registers[rs1] < _registers[rs2];
					break;
				case 0x7:
					take = _registers[rs1] >= _registers[rs2];
					break;
				default:
					fault(ExternalProgramStatus::IllegalInstruction, _pc, instruction);
					return;
			}
			_pc = take ? _pc + static_cast<std::uint32_t>(immediateB(instruction)) : nextPc;
		}

		void executeLoad(std::uint8_t funct3, std::uint8_t rd, std::uint8_t rs1, std::uint32_t instruction, std::uint32_t nextPc) {
			auto const address = _registers[rs1] + static_cast<std::uint32_t>(immediateI(instruction));
			switch (funct3) {
				case 0x0: {
					std::uint8_t value = 0;
					if (!load8(address, value)) {
						fault(ExternalProgramStatus::LoadAddressFault, address, instruction);
						return;
					}
					_registers[rd] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value)));
					break;
				}
				case 0x1: {
					std::uint16_t value = 0;
					if (!load16(address, value)) {
						fault(ExternalProgramStatus::LoadAddressFault, address, instruction);
						return;
					}
					_registers[rd] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value)));
					break;
				}
				case 0x2: {
					std::uint32_t value = 0;
					if (!load32(address, value)) {
						fault(ExternalProgramStatus::LoadAddressFault, address, instruction);
						return;
					}
					_registers[rd] = value;
					break;
				}
				case 0x4: {
					std::uint8_t value = 0;
					if (!load8(address, value)) {
						fault(ExternalProgramStatus::LoadAddressFault, address, instruction);
						return;
					}
					_registers[rd] = value;
					break;
				}
				case 0x5: {
					std::uint16_t value = 0;
					if (!load16(address, value)) {
						fault(ExternalProgramStatus::LoadAddressFault, address, instruction);
						return;
					}
					_registers[rd] = value;
					break;
				}
				default:
					fault(ExternalProgramStatus::IllegalInstruction, _pc, instruction);
					return;
			}
			_pc = nextPc;
		}

		void executeStore(std::uint8_t funct3, std::uint8_t rs1, std::uint8_t rs2, std::uint32_t instruction, std::uint32_t nextPc) {
			auto const address = _registers[rs1] + static_cast<std::uint32_t>(immediateS(instruction));
			bool ok = false;
			switch (funct3) {
				case 0x0:
					ok = store8(address, static_cast<std::uint8_t>(_registers[rs2] & 0xFFU));
					break;
				case 0x1:
					ok = store16(address, static_cast<std::uint16_t>(_registers[rs2] & 0xFFFFU));
					break;
				case 0x2:
					ok = store32(address, _registers[rs2]);
					break;
				default:
					fault(ExternalProgramStatus::IllegalInstruction, _pc, instruction);
					return;
			}
			if (!ok) {
				fault(ExternalProgramStatus::StoreAddressFault, address, instruction);
				return;
			}
			_pc = nextPc;
		}

		void executeOpImmediate(
		    std::uint8_t funct3,
		    std::uint8_t funct7,
		    std::uint8_t rd,
		    std::uint8_t rs1,
		    std::uint32_t instruction,
		    std::uint32_t nextPc) {
			auto const imm = immediateI(instruction);
			auto const shift = static_cast<std::uint8_t>((instruction >> 20U) & 0x1FU);
			switch (funct3) {
				case 0x0:
					_registers[rd] = _registers[rs1] + static_cast<std::uint32_t>(imm);
					break;
				case 0x2:
					_registers[rd] = static_cast<std::int32_t>(_registers[rs1]) < imm ? 1U : 0U;
					break;
				case 0x3:
					_registers[rd] = _registers[rs1] < static_cast<std::uint32_t>(imm) ? 1U : 0U;
					break;
				case 0x4:
					_registers[rd] = _registers[rs1] ^ static_cast<std::uint32_t>(imm);
					break;
				case 0x6:
					_registers[rd] = _registers[rs1] | static_cast<std::uint32_t>(imm);
					break;
				case 0x7:
					_registers[rd] = _registers[rs1] & static_cast<std::uint32_t>(imm);
					break;
				case 0x1:
					if (funct7 != 0x00U) {
						fault(ExternalProgramStatus::IllegalInstruction, _pc, instruction);
						return;
					}
					_registers[rd] = _registers[rs1] << shift;
					break;
				case 0x5:
					if (funct7 == 0x00U) {
						_registers[rd] = _registers[rs1] >> shift;
					} else if (funct7 == 0x20U) {
						_registers[rd] = static_cast<std::uint32_t>(static_cast<std::int32_t>(_registers[rs1]) >> shift);
					} else {
						fault(ExternalProgramStatus::IllegalInstruction, _pc, instruction);
						return;
					}
					break;
				default:
					fault(ExternalProgramStatus::IllegalInstruction, _pc, instruction);
					return;
			}
			_pc = nextPc;
		}

		void executeOp(
		    std::uint8_t funct3,
		    std::uint8_t funct7,
		    std::uint8_t rd,
		    std::uint8_t rs1,
		    std::uint8_t rs2,
		    std::uint32_t instruction,
		    std::uint32_t nextPc) {
			switch (funct3) {
				case 0x0:
					if (funct7 == 0x00U) {
						_registers[rd] = _registers[rs1] + _registers[rs2];
					} else if (funct7 == 0x20U) {
						_registers[rd] = _registers[rs1] - _registers[rs2];
					} else {
						fault(ExternalProgramStatus::IllegalInstruction, _pc, instruction);
						return;
					}
					break;
				case 0x1:
					if (funct7 != 0x00U) {
						fault(ExternalProgramStatus::IllegalInstruction, _pc, instruction);
						return;
					}
					_registers[rd] = _registers[rs1] << (_registers[rs2] & 0x1FU);
					break;
				case 0x2:
					_registers[rd] = static_cast<std::int32_t>(_registers[rs1]) < static_cast<std::int32_t>(_registers[rs2]) ? 1U : 0U;
					break;
				case 0x3:
					_registers[rd] = _registers[rs1] < _registers[rs2] ? 1U : 0U;
					break;
				case 0x4:
					_registers[rd] = _registers[rs1] ^ _registers[rs2];
					break;
				case 0x5:
					if (funct7 == 0x00U) {
						_registers[rd] = _registers[rs1] >> (_registers[rs2] & 0x1FU);
					} else if (funct7 == 0x20U) {
						_registers[rd] = static_cast<std::uint32_t>(static_cast<std::int32_t>(_registers[rs1]) >> (_registers[rs2] & 0x1FU));
					} else {
						fault(ExternalProgramStatus::IllegalInstruction, _pc, instruction);
						return;
					}
					break;
				case 0x6:
					_registers[rd] = _registers[rs1] | _registers[rs2];
					break;
				case 0x7:
					_registers[rd] = _registers[rs1] & _registers[rs2];
					break;
				default:
					fault(ExternalProgramStatus::IllegalInstruction, _pc, instruction);
					return;
			}
			_pc = nextPc;
		}

		void handleEcall(std::uint32_t nextPc) {
			auto const syscall = _registers[17]; // a7
			switch (static_cast<VmSyscall>(syscall)) {
				case VmSyscall::Exit:
					_exitCode = static_cast<int>(static_cast<std::int32_t>(_registers[10]));
					_pc = nextPc;
					_running = false;
					return;
				case VmSyscall::Write:
					handleWriteSyscall();
					_pc = nextPc;
					return;
				case VmSyscall::ReadChar:
					handleReadCharSyscall();
					_pc = nextPc;
					return;
				case VmSyscall::Read:
					handleReadSyscall();
					_pc = nextPc;
					return;
				case VmSyscall::PollKeyEvent:
					handlePollKeyEventSyscall();
					_pc = nextPc;
					return;
				case VmSyscall::Yield:
					lexxos::kernel::yield();
					_registers[10] = 0;
					_pc = nextPc;
					return;
				case VmSyscall::Argc:
					_registers[10] = _guestArgc;
					_pc = nextPc;
					return;
				case VmSyscall::Argv:
					_registers[10] = _guestArgvAddress;
					_pc = nextPc;
					return;
				case VmSyscall::OpenRead:
					handleOpenReadSyscall();
					_pc = nextPc;
					return;
				case VmSyscall::Close:
					handleCloseSyscall();
					_pc = nextPc;
					return;
				case VmSyscall::ListDirectory:
					handleListDirectorySyscall();
					_pc = nextPc;
					return;
				case VmSyscall::Stat:
					handleStatSyscall();
					_pc = nextPc;
					return;
				case VmSyscall::Seek:
					handleSeekSyscall();
					_pc = nextPc;
					return;
				case VmSyscall::OpenWriteNew:
					handleOpenWriteNewSyscall();
					_pc = nextPc;
					return;
				case VmSyscall::WriteFile:
					handleWriteFileSyscall();
					_pc = nextPc;
					return;
				case VmSyscall::RemoveFile:
					handleRemoveFileSyscall();
					_pc = nextPc;
					return;
				case VmSyscall::RenamePath:
					handleRenamePathSyscall();
					_pc = nextPc;
					return;
				case VmSyscall::MakeDirectory:
					handleMakeDirectorySyscall();
					_pc = nextPc;
					return;
				case VmSyscall::RemoveDirectory:
					handleRemoveDirectorySyscall();
					_pc = nextPc;
					return;
				case VmSyscall::Milliseconds: {
					auto const clock = _context.platform().clock.uptimeMs;
					_registers[10] = clock != nullptr ? clock() : lexxSyscallError;
					_pc = nextPc;
					return;
				}
				case VmSyscall::PlayTone:
					handlePlayToneSyscall();
					_pc = nextPc;
					return;
			}
			_unsupportedSyscall = syscall;
			fault(ExternalProgramStatus::UnsupportedSyscall, _pc, 0x00000073U);
		}

		void handlePlayToneSyscall() {
			auto const frequency = _registers[10];
			auto const duration = _registers[11];
			auto const volume = _registers[12];
			_registers[10] = lexxSyscallError;
			if ((_header.requiredCapabilities & LexxCapabilityAudio) == 0U) {
				return;
			}
			auto* device = _context.devices().primaryAudioDevice(_context.platform());
			if (device == nullptr || !device->supportsTone() || !device->supportsChannel(lexxos::devices::AudioChannel::Stereo) || frequency < device->minToneFrequencyHz() || frequency > device->maxToneFrequencyHz() || duration == 0 || duration > device->maxToneDurationMs() || volume > 255U) {
				return;
			}
			lexxos::devices::AudioToneRequest request{
			    .channel = lexxos::devices::AudioChannel::Stereo,
			    .frequencyHz = frequency,
			    .durationMs = duration,
			    .volume = static_cast<std::uint8_t>(volume),
			};
			_registers[10] = device->playTone(request) ? 0U : lexxSyscallError;
		}

		void handleWriteSyscall() {
			auto const fd = _registers[10];
			auto const address = _registers[11];
			auto const length = _registers[12];
			if ((fd != 1U && fd != 2U) || !rangeValid(address, length)) {
				_registers[10] = lexxSyscallError;
				return;
			}
			auto* output = fd == 2U ? &_context.standardError() : &_context.standardOutput();
			std::uint32_t written = 0;
			while (written < length) {
				auto const chunk = std::min<std::uint32_t>(length - written, 128U);
				output->write(std::string_view{reinterpret_cast<char const*>(_memory + address + written), chunk});
				written += chunk;
			}
			_registers[10] = written;
		}

		void handleReadCharSyscall() {
			if (!_context.hasProgramFocus()) {
				_registers[10] = lexxSyscallError;
				return;
			}
			char value = 0;
			if (!lexxos::app::console_input::readChar(_context.platform(), value)) {
				_registers[10] = lexxSyscallError;
				return;
			}
			if (static_cast<std::uint8_t>(value) == 0x03U) {
				_context.requestCancellation();
				_status = ExternalProgramStatus::Cancelled;
				_running = false;
				_registers[10] = lexxSyscallCancelled;
				return;
			}
			_registers[10] = static_cast<std::uint8_t>(value);
		}

		void handleReadSyscall() {
			auto const fd = _registers[10];
			auto const address = _registers[11];
			auto const length = _registers[12];
			if (length == 0) {
				_registers[10] = 0;
				return;
			}
			if (!rangeValid(address, length)) {
				_registers[10] = lexxSyscallError;
				return;
			}
			if (fd == 0U) {
				std::size_t bytesRead = 0;
				auto const status = _context.standardInput().read(_memory + address, length, bytesRead);
				_registers[10] = status == lexxos::vfs::ReadStatus::Ok ? static_cast<std::uint32_t>(bytesRead) : lexxSyscallError;
				return;
			}

			std::size_t index = 0;
			if (!guestFileIndex(fd, index) || !_guestFiles[index].isOpen() || !_guestFiles[index].canRead()) {
				_registers[10] = lexxSyscallError;
				return;
			}

			std::size_t bytesRead = 0;
			auto const status = _guestFiles[index].read(_memory + address, length, bytesRead);
			_registers[10] = status == lexxos::vfs::ReadStatus::Ok ? static_cast<std::uint32_t>(bytesRead) : lexxSyscallError;
		}

		void handlePollKeyEventSyscall() {
			auto const address = _registers[10];
			if (!rangeValid(address, lexxGuestEventSize)) {
				_registers[10] = lexxSyscallError;
				return;
			}
			if (!_context.hasProgramFocus()) {
				_registers[10] = 0;
				return;
			}

			lexxos::devices::InputEvent event{};
			while (lexxos::app::program_input::pollEvent(_guestInputCursor, event)) {
				if (isCtrlCEvent(event)) {
					_context.requestCancellation();
					_status = ExternalProgramStatus::Cancelled;
					_running = false;
					_registers[10] = lexxSyscallCancelled;
					return;
				}
				_memory[address] = event.type == lexxos::devices::InputEventType::KeyDown ? 1U : 2U;
				_memory[address + 1U] = event.modifiers;
				_memory[address + 2U] = event.character;
				_memory[address + 3U] = event.scanCode;
				writeLe16(_memory + address + 4U, static_cast<std::uint16_t>(event.key));
				writeLe16(_memory + address + 6U, 0);
				writeLe32(_memory + address + 8U, event.timestampMs);
				_registers[10] = 1;
				return;
			}
			_registers[10] = 0;
		}

		// Read-only, paged directory enumeration. The guest never receives a host pointer.
		void handleListDirectorySyscall() {
			auto const guestAddress = _registers[10];
			auto const offset = _registers[11];
			auto const bufferAddress = _registers[12];
			if (!hasCapability(LexxCapabilityStorageRead)
			    || !rangeValid(bufferAddress, lexxGuestDirEntrySize * lexxGuestDirPageSize)) {
				_registers[10] = lexxSyscallError;
				return;
			}

			std::array<char, lexxos::vfs::maxPathLength> guestPath{};
			std::array<char, lexxos::vfs::maxPathLength> path{};
			if (!loadGuestString(guestAddress, guestPath)
			    || !_context.vfs().normalizePath(_context.workingDirectory(), guestPath.data(), path.data(), path.size())) {
				_registers[10] = lexxSyscallError;
				return;
			}

			lexxos::vfs::Stat stat{};
			if (!_context.vfs().stat(path.data(), stat) || stat.type != lexxos::vfs::NodeType::Directory) {
				_registers[10] = lexxSyscallError;
				return;
			}

			struct Page {
				std::uint8_t* bytes;
				std::uint32_t skip;
				std::uint32_t seen{0};
				std::uint32_t count{0};
			} page{_memory + bufferAddress, offset};

			std::memset(page.bytes, 0, lexxGuestDirEntrySize * lexxGuestDirPageSize);

			auto const visitor = [](lexxos::vfs::DirectoryEntry const& entry, void* opaque) -> bool {
				auto& out = *static_cast<Page*>(opaque);
				if (out.seen < out.skip) {
					++out.seen;
					return true;
				}
				if (out.count >= lexxGuestDirPageSize) {
					return true;
				}
				auto* record = out.bytes + out.count * lexxGuestDirEntrySize;
				auto const length = std::min<std::size_t>(entry.name.size(), lexxGuestDirectoryNameSize - 1U);
				std::memcpy(record, entry.name.data(), length);
				record[length] = 0;
				writeLe32(record + 64, static_cast<std::uint32_t>(entry.sizeBytes));
				writeLe32(record + 68, static_cast<std::uint32_t>(entry.sizeBytes >> 32U));
				record[72] = static_cast<std::uint8_t>(entry.type);
				++out.count;
				return true;
			};
			_registers[10] = _context.vfs().list(path.data(), visitor, &page)
			    ? page.count
			    : lexxSyscallError;
		}

		void handleStatSyscall() {
			auto const guestAddress = _registers[10];
			auto const bufferAddress = _registers[11];
			if (!hasCapability(LexxCapabilityStorageRead) || !rangeValid(bufferAddress, lexxGuestStatSize)) {
				_registers[10] = lexxSyscallError;
				return;
			}
			std::array<char, lexxos::vfs::maxPathLength> guestPath{};
			std::array<char, lexxos::vfs::maxPathLength> path{};
			if (!loadGuestString(guestAddress, guestPath)
			    || !_context.vfs().normalizePath(_context.workingDirectory(), guestPath.data(), path.data(), path.size())) {
				_registers[10] = lexxSyscallError;
				return;
			}
			lexxos::vfs::Stat stat{};
			if (!_context.vfs().stat(path.data(), stat)) {
				_registers[10] = lexxSyscallError;
				return;
			}
			auto* record = _memory + bufferAddress;
			writeLe32(record, static_cast<std::uint32_t>(stat.type));
			writeLe32(record + 4, static_cast<std::uint32_t>(stat.sizeBytes));
			writeLe32(record + 8, static_cast<std::uint32_t>(stat.sizeBytes >> 32U));
			_registers[10] = 0;
		}

		void handleOpenReadSyscall() {
			if (!hasCapability(LexxCapabilityStorageRead)) {
				_registers[10] = lexxSyscallError;
				return;
			}

			std::array<char, lexxos::vfs::maxPathLength> guestPath{};
			if (!loadGuestString(_registers[10], guestPath)) {
				_registers[10] = lexxSyscallError;
				return;
			}

			std::array<char, lexxos::vfs::maxPathLength> normalizedPath{};
			if (!_context.vfs().normalizePath(_context.workingDirectory(), guestPath.data(), normalizedPath.data(), normalizedPath.size())) {
				_registers[10] = lexxSyscallError;
				return;
			}

			std::size_t slot = _guestFiles.size();
			for (std::size_t index = 0; index < _guestFiles.size(); ++index) {
				if (!_guestFiles[index].isOpen()) {
					slot = index;
					break;
				}
			}
			if (slot >= _guestFiles.size()) {
				_registers[10] = lexxSyscallError;
				return;
			}

			auto const openStatus = _context.vfs().open(normalizedPath.data(), lexxos::vfs::OpenMode::Read, _guestFiles[slot]);
			if (openStatus != lexxos::vfs::OpenStatus::Ok || !_guestFiles[slot].isOpen() || !_guestFiles[slot].canRead()) {
				_guestFiles[slot].close();
				_registers[10] = lexxSyscallError;
				return;
			}

			_registers[10] = lexxGuestFileDescriptorBase + static_cast<std::uint32_t>(slot);
		}

		void handleCloseSyscall() {
			std::size_t index = 0;
			if (!guestFileIndex(_registers[10], index) || !_guestFiles[index].isOpen()) {
				_registers[10] = lexxSyscallError;
				return;
			}
			_guestFiles[index].close();
			_registers[10] = 0;
		}

		void handleSeekSyscall() {
			if (!hasCapability(LexxCapabilityStorageRead)) {
				_registers[10] = lexxSyscallError;
				return;
			}
			std::size_t index = 0;
			auto const fd = _registers[10];
			if (!guestFileIndex(fd, index) || !_guestFiles[index].isOpen() || !_guestFiles[index].canRead()) {
				_registers[10] = lexxSyscallError;
				return;
			}
			auto const position = static_cast<std::uint64_t>(_registers[11])
			    | (static_cast<std::uint64_t>(_registers[12]) << 32U);
			_registers[10] = (position <= _guestFiles[index].size() && _guestFiles[index].seek(position))
			    ? 0U
			    : lexxSyscallError;
		}

		bool guestPath(std::uint32_t address, std::array<char, lexxos::vfs::maxPathLength>& out) {
			std::array<char, lexxos::vfs::maxPathLength> raw{};
			return loadGuestString(address, raw)
			    && _context.vfs().normalizePath(_context.workingDirectory(), raw.data(), out.data(), out.size());
		}

		void handleOpenWriteNewSyscall() {
			auto const guestAddress = _registers[10];
			_registers[10] = lexxSyscallError;
			if (!hasCapability(LexxCapabilityStorageWrite)) {
				return;
			}
			std::array<char, lexxos::vfs::maxPathLength> path{};
			if (!guestPath(guestAddress, path)) {
				return;
			}
			lexxos::vfs::Stat existing{};
			if (_context.vfs().stat(path.data(), existing)) {
				return;
			}
			std::size_t slot = _guestFiles.size();
			for (std::size_t i = 0; i < _guestFiles.size(); ++i) {
				if (!_guestFiles[i].isOpen()) {
					slot = i;
					break;
				}
			}
			if (slot == _guestFiles.size()) {
				return;
			}
			if (_context.vfs().open(path.data(), lexxos::vfs::OpenMode::CreateTruncateWrite,
			        _guestFiles[slot])
			        != lexxos::vfs::OpenStatus::Ok
			    || !_guestFiles[slot].canWrite()) {
				_guestFiles[slot].close();
				return;
			}
			_registers[10] = lexxGuestFileDescriptorBase + static_cast<std::uint32_t>(slot);
		}

		void handleWriteFileSyscall() {
			auto const fd = _registers[10];
			auto const address = _registers[11];
			auto const length = _registers[12];
			_registers[10] = lexxSyscallError;
			std::size_t index = 0;
			if (!hasCapability(LexxCapabilityStorageWrite) || length > 4096U || !rangeValid(address, length) || !guestFileIndex(fd, index) || !_guestFiles[index].canWrite()) {
				return;
			}
			auto const data = std::string_view{reinterpret_cast<char const*>(_memory + address), length};
			if (_guestFiles[index].write(data) == lexxos::vfs::WriteStatus::Ok) {
				_registers[10] = length;
			}
		}

		void handleRemoveFileSyscall() {
			auto const guestAddress = _registers[10];
			_registers[10] = lexxSyscallError;
			if (!hasCapability(LexxCapabilityStorageWrite)) {
				return;
			}
			std::array<char, lexxos::vfs::maxPathLength> path{};
			if (!guestPath(guestAddress, path)) {
				return;
			}
			lexxos::vfs::Stat stat{};
			if (!_context.vfs().stat(path.data(), stat) || stat.type != lexxos::vfs::NodeType::RegularFile) {
				return;
			}
			if (_context.vfs().remove(path.data()) == lexxos::vfs::RemoveStatus::Ok) {
				_registers[10] = 0;
			}
		}

		void handleRenamePathSyscall() {
			auto const first = _registers[10];
			auto const second = _registers[11];
			_registers[10] = lexxSyscallError;
			if (!hasCapability(LexxCapabilityStorageWrite)) {
				return;
			}
			std::array<char, lexxos::vfs::maxPathLength> source{};
			std::array<char, lexxos::vfs::maxPathLength> destination{};
			if (!guestPath(first, source) || !guestPath(second, destination)) {
				return;
			}
			lexxos::vfs::Stat existing{};
			if (_context.vfs().stat(destination.data(), existing)) {
				return;
			}
			auto const status = _context.vfs().rename(source.data(), destination.data());
			if (status == lexxos::vfs::RenameStatus::Ok) {
				_registers[10] = 0;
			} else if (status == lexxos::vfs::RenameStatus::CrossFilesystem
			    || status == lexxos::vfs::RenameStatus::CrossDirectory) {
				_registers[10] = 0xFFFFFFFEU;
			}
		}

		void handleMakeDirectorySyscall() {
			auto const guestAddress = _registers[10];
			_registers[10] = lexxSyscallError;
			if (!hasCapability(LexxCapabilityStorageWrite)) {
				return;
			}
			std::array<char, lexxos::vfs::maxPathLength> path{};
			if (!guestPath(guestAddress, path)) {
				return;
			}
			lexxos::vfs::Stat existing{};
			if (_context.vfs().stat(path.data(), existing)) {
				return;
			}
			if (_context.vfs().createDirectory(path.data()) == lexxos::vfs::WriteStatus::Ok) {
				_registers[10] = 0;
			}
		}

		void handleRemoveDirectorySyscall() {
			auto const guestAddress = _registers[10];
			_registers[10] = lexxSyscallError;
			if (!hasCapability(LexxCapabilityStorageWrite)) {
				return;
			}
			std::array<char, lexxos::vfs::maxPathLength> path{};
			if (!guestPath(guestAddress, path)) {
				return;
			}
			lexxos::vfs::Stat stat{};
			if (!_context.vfs().stat(path.data(), stat) || stat.type != lexxos::vfs::NodeType::Directory) {
				return;
			}
			if (_context.vfs().removeDirectory(path.data()) == lexxos::vfs::RemoveStatus::Ok) {
				_registers[10] = 0;
			}
		}

		lexxos::os::AppContext& _context;
		std::uint8_t* _memory{nullptr};
		std::uint32_t _memorySize{0};
		LexxExecutableHeader _header{};
		ExternalProgramOptions _options{};
		lexxos::app::program_input::EventCursor _guestInputCursor{};
		lexxos::app::program_input::EventCursor _cancelInputCursor{};
		std::array<lexxos::vfs::FileHandle, lexxMaximumGuestOpenFiles> _guestFiles{};
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
	};

	ExternalProgramStatus readExact(lexxos::vfs::FileHandle& file, void* buffer, std::size_t length) {
		auto* output = static_cast<std::uint8_t*>(buffer);
		std::size_t total = 0;
		while (total < length) {
			std::size_t bytesRead = 0;
			auto const status = file.read(output + total, length - total, bytesRead);
			if (status != lexxos::vfs::ReadStatus::Ok) {
				return ExternalProgramStatus::ReadFailed;
			}
			if (bytesRead == 0) {
				return ExternalProgramStatus::ReadFailed;
			}
			total += bytesRead;
		}
		return ExternalProgramStatus::Ok;
	}

} // namespace

std::string_view toString(ExternalProgramStatus status) {
	switch (status) {
		case ExternalProgramStatus::Ok:
			return "ok";
		case ExternalProgramStatus::Cancelled:
			return "cancelled";
		case ExternalProgramStatus::OpenFailed:
			return "open-failed";
		case ExternalProgramStatus::ReadFailed:
			return "read-failed";
		case ExternalProgramStatus::ShortHeader:
			return "short-header";
		case ExternalProgramStatus::BadMagic:
			return "bad-magic";
		case ExternalProgramStatus::UnsupportedVersion:
			return "unsupported-version";
		case ExternalProgramStatus::UnsupportedMachine:
			return "unsupported-machine";
		case ExternalProgramStatus::UnsupportedAbi:
			return "unsupported-abi";
		case ExternalProgramStatus::UnsupportedCapabilities:
			return "unsupported-capabilities";
		case ExternalProgramStatus::InvalidHeader:
			return "invalid-header";
		case ExternalProgramStatus::InvalidImage:
			return "invalid-image";
		case ExternalProgramStatus::ImageTooLarge:
			return "image-too-large";
		case ExternalProgramStatus::ArgumentsTooLarge:
			return "arguments-too-large";
		case ExternalProgramStatus::MemoryUnavailable:
			return "memory-unavailable";
		case ExternalProgramStatus::InstructionLimit:
			return "instruction-limit";
		case ExternalProgramStatus::InstructionAddressFault:
			return "instruction-address-fault";
		case ExternalProgramStatus::LoadAddressFault:
			return "load-address-fault";
		case ExternalProgramStatus::StoreAddressFault:
			return "store-address-fault";
		case ExternalProgramStatus::IllegalInstruction:
			return "illegal-instruction";
		case ExternalProgramStatus::UnsupportedSyscall:
			return "unsupported-syscall";
	}
	return "unknown";
}

ExternalProgramResult runLexxExecutable(
    lexxos::os::AppContext& context,
    std::string_view path,
    ExternalProgramOptions const& options) {
	lexxos::vfs::FileHandle file{};
	auto const openStatus = context.vfs().open(path, lexxos::vfs::OpenMode::Read, file);
	if (openStatus != lexxos::vfs::OpenStatus::Ok || !file.isOpen()) {
		return {.status = ExternalProgramStatus::OpenFailed, .exitCode = 126};
	}
	if (file.size() < lexxExecutableHeaderSize) {
		return {.status = ExternalProgramStatus::ShortHeader, .exitCode = 126};
	}

	std::uint8_t rawHeader[lexxExecutableHeaderSize]{};
	if (auto const status = readExact(file, rawHeader, sizeof(rawHeader)); status != ExternalProgramStatus::Ok) {
		return {.status = status, .exitCode = 126};
	}

	LexxExecutableHeader header{};
	if (!parseHeader(rawHeader, sizeof(rawHeader), header)) {
		return {.status = ExternalProgramStatus::BadMagic, .exitCode = 126};
	}
	auto const reservedBytesAreZero = bytesAreZero(
	    rawHeader + lexxHeaderReservedOffset,
	    lexxExecutableHeaderSize - lexxHeaderReservedOffset);
	if (auto const status = validateHeader(header, file.size(), reservedBytesAreZero); status != ExternalProgramStatus::Ok) {
		return {.status = status, .exitCode = 126};
	}

	auto* const arena = context.memoryArena();
	if (arena == nullptr) {
		return {.status = ExternalProgramStatus::MemoryUnavailable, .exitCode = 126};
	}

	auto* memory = static_cast<std::uint8_t*>(arena->allocate(header.memorySize, 4));
	if (memory == nullptr) {
		return {.status = ExternalProgramStatus::MemoryUnavailable, .exitCode = 126};
	}
	std::memset(memory, 0, header.memorySize);

	if (!file.seek(header.imageOffset)) {
		return {.status = ExternalProgramStatus::ReadFailed, .exitCode = 126};
	}
	if (auto const status = readExact(file, memory, header.imageSize); status != ExternalProgramStatus::Ok) {
		return {.status = status, .exitCode = 126};
	}

	Rv32iVirtualMachine vm{context, memory, header.memorySize, header, options};
	return vm.run();
}

} // namespace lexxos::external_program
