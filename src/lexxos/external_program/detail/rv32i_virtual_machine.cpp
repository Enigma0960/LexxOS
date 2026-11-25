#include <lexxos/external_program/detail/rv32i_virtual_machine.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <string_view>
#include <utility>

#include <lexxos/app/console_input.h>
#include <lexxos/app/console_mirror.h>
#include <lexxos/app/guest_graphics_mode.h>
#include <lexxos/app/program_input.h>
#include <lexxos/devices/audio_device.h>
#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/bus/spi_bus.h>
#include <lexxos/devices/bus/spi_transfer.h>
#include <lexxos/devices/bus/uart_bus.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/devices/input_device.h>
#include <lexxos/devices/rtc/software_rtc.h>
#include <lexxos/external_program/external_program_runner.h>
#include <lexxos/external_program/lexx_graphics_asset.h>
#include <lexxos/external_program/lexx_resources.h>
#include <lexxos/feature.h>
#include <lexxos/io/output_stream.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/kernel/system.h>
#include <lexxos/os/app_context.h>
#include <lexxos/platform/platform_services.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/ui/guest_ui.h>
#include <lexxos/vfs/vfs.h>

#if FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
	#include <lexxos/devices/picocalc/picocalc_southbridge_resource_policy.h>
	#include <lexxos/devices/picocalc/picocalc_southbridge_runtime.h>
#endif

#include <lexxos/external_program/detail/rv32i_support.h>

namespace lexxos::external_program::detail {

Rv32iVirtualMachine::Rv32iVirtualMachine(
    lexxos::os::AppContext& context,
    std::uint8_t* memory,
    std::uint32_t memorySize,
    LexxExecutableHeader const& header,
    ExternalProgramOptions const& options,
    std::string_view executablePath,
    resources::Index const& resourceIndex)
    : _context(context)
    , _memory(memory)
    , _memorySize(memorySize)
    , _header(header)
    , _options(options)
    , _executablePath(executablePath)
    , _resourceIndex(resourceIndex)
    , _inputMode((header.flags & lexxExecutableFlagInputEvents) != 0U
              ? lexxos::app::program_input::InputMode::Events
              : lexxos::app::program_input::InputMode::Terminal) {
}

Rv32iVirtualMachine::~Rv32iVirtualMachine() {
	if (_context.processId() != 0U) {
		if (_context.hasProgramFocus()) {
			_context.consoleInput().clearInjected();
		}
		_context.programInput().releaseInputMode(_context.processId());
	}
	releaseGraphics();

	for (auto& image : _graphicsImages) {
		delete[] image.pixels;
	}
}

ExternalProgramResult Rv32iVirtualMachine::run() {
	_context.programInput().reset(_guestInputCursor);
	_context.programInput().reset(_cancelInputCursor);
	if (!installArguments()) {
		_status = ExternalProgramStatus::ArgumentsTooLarge;
		return result();
	}
	if (_context.processId() != 0U) {
		if (!_context.programInput().setInputMode(_context.processId(), _inputMode)) {
			_status = ExternalProgramStatus::MemoryUnavailable;
			return result();
		}
		if (_context.hasProgramFocus()) {
			// Do not inherit Shell keystrokes or bytes from the previous program.
			_context.consoleInput().clearInjected();
		}
	}
	_registers[2] = _stackTop;
	_registers[10] = _guestArgc;
	_registers[11] = _guestArgvAddress;
	_pc = _header.entryOffset;
	_running = true;

#if defined(LEXXOS_VM_FAST_LOOP)
	auto executed = _instructionsExecuted;

	#if defined(LEXXOS_VM_LOCAL_PC)
	auto pc = _pc;
	#endif

	auto const limit = _options.maxInstructions;
	bool const trace = _options.trace;
	while (_running) {
		if (limit != 0U && executed >= limit) {
			_status = ExternalProgramStatus::InstructionLimit;
			break;
		}

		if ((executed & 0xFFU) == 0U && pollCancellation()) {
			_status = ExternalProgramStatus::Cancelled;
			break;
		}

		auto budget = 256U - (executed & 0xFFU);
		if (limit != 0U && limit - executed < budget) {
			budget = limit - executed;
		}

		for (std::uint32_t i = 0; i < budget && _running; ++i) {
	#if defined(LEXXOS_VM_LOCAL_PC)
			if (step(pc, trace)) {
	#else
			if (step(_pc, trace)) {
	#endif
				++executed;
			}
		}
	}

	_instructionsExecuted = executed;

	#if defined(LEXXOS_VM_LOCAL_PC)
	_pc = pc;
	#endif
#else
	while (_running) {
		if (_options.maxInstructions != 0U && _instructionsExecuted >= _options.maxInstructions) {
			_status = ExternalProgramStatus::InstructionLimit;
			break;
		}

		if ((_instructionsExecuted & 0xFFU) == 0U && pollCancellation()) {
			_status = ExternalProgramStatus::Cancelled;
			break;
		}

		step(_pc, _options.trace);
		_registers[0] = 0;
	}
#endif

	return result();
}

ExternalProgramResult Rv32iVirtualMachine::result() const {
	return {
	    .status = _status,
	    .exitCode = _exitCode,
	    .instructionsExecuted = _instructionsExecuted,
	    .faultAddress = _faultAddress,
	    .faultInstruction = _faultInstruction,
	    .unsupportedSyscall = _unsupportedSyscall,
	};
}

bool Rv32iVirtualMachine::rangeValid(std::uint32_t address, std::uint32_t length) const {
	return rangeFits(address, length, _memorySize);
}

bool Rv32iVirtualMachine::hasCapability(std::uint32_t capability) const {
	return (_header.requiredCapabilities & capability) != 0U;
}

bool Rv32iVirtualMachine::installArguments() {
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

bool Rv32iVirtualMachine::load8(std::uint32_t address, std::uint8_t& value) const {
	if (!rangeValid(address, 1)) {
		return false;
	}
	value = _memory[address];
	return true;
}

bool Rv32iVirtualMachine::load16(std::uint32_t address, std::uint16_t& value) const {
	if ((address & 0x01U) != 0U || !rangeValid(address, 2)) {
		return false;
	}

#if defined(LEXXOS_RK3506_VM_ALIGNED_ACCESS)
	value = readAlignedGuest<std::uint16_t>(_memory + address);
#else
	value = readLe16(_memory + address);
#endif

	return true;
}

bool Rv32iVirtualMachine::load32(std::uint32_t address, std::uint32_t& value) const {
	if ((address & 0x03U) != 0U || !rangeValid(address, 4)) {
		return false;
	}
#if defined(LEXXOS_RK3506_VM_ALIGNED_ACCESS)
	value = readAlignedGuest<std::uint32_t>(_memory + address);
#else
	value = readLe32(_memory + address);
#endif
	return true;
}

bool Rv32iVirtualMachine::store8(std::uint32_t address, std::uint8_t value) {
	if (!rangeValid(address, 1)) {
		return false;
	}
	_memory[address] = value;
	return true;
}

bool Rv32iVirtualMachine::store16(std::uint32_t address, std::uint16_t value) {
	if ((address & 0x01U) != 0U || !rangeValid(address, 2)) {
		return false;
	}
#if defined(LEXXOS_RK3506_VM_ALIGNED_ACCESS)
	writeAlignedGuest<std::uint16_t>(_memory + address, value);
#else
	writeLe16(_memory + address, value);
#endif
	return true;
}

bool Rv32iVirtualMachine::store32(std::uint32_t address, std::uint32_t value) {
	if ((address & 0x03U) != 0U || !rangeValid(address, 4)) {
		return false;
	}
#if defined(LEXXOS_RK3506_VM_ALIGNED_ACCESS)
	writeAlignedGuest<std::uint32_t>(_memory + address, value);
#else
	writeLe32(_memory + address, value);
#endif
	return true;
}

bool Rv32iVirtualMachine::loadGuestString(std::uint32_t address, std::array<char, lexxos::vfs::maxPathLength>& output) const {
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

bool Rv32iVirtualMachine::guestFileIndex(std::uint32_t fd, std::size_t& output) const {
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

void Rv32iVirtualMachine::fault(ExternalProgramStatus status, std::uint32_t address, std::uint32_t instruction) {
	_status = status;
	_faultAddress = address;
	_faultInstruction = instruction;
	_running = false;
}

bool Rv32iVirtualMachine::pollCancellation() {
	if (_context.cancellationRequested() || lexxos::kernel::isShutdownRequested()) {
		return true;
	}
	if (!_context.hasProgramFocus()) {
		return false;
	}

	lexxos::devices::InputEvent event{};
	while (_context.programInput().pollEvent(_cancelInputCursor, event)) {
		if (isCtrlCEvent(event)) {
			_context.requestCancellation();
			return true;
		}
	}
	return _context.cancellationRequested();
}

void Rv32iVirtualMachine::traceInstruction(std::uint32_t instruction) {
	if (!_options.trace) {
		return;
	}
	_context.write("rv32i pc=");
	writeHex32(_context, _pc);
	_context.write(" inst=");
	writeHex32(_context, instruction);
	_context.writeLine("");
}

std::uint32_t Rv32iVirtualMachine::readRegister(std::uint8_t index) const {
#if defined(LEXXOS_VM_FAST_LOOP)
	if (index == 0U) {
		return 0U;
	}
#endif
	return _registers[index];
}

void Rv32iVirtualMachine::writeRegister(std::uint8_t index, std::uint32_t value) {
#if defined(LEXXOS_VM_FAST_LOOP)
	if (index == 0U) {
		return;
	}
#endif
	_registers[index] = value;
}

bool Rv32iVirtualMachine::step(std::uint32_t& pc, bool trace) {
	std::uint32_t instruction = 0;

	if (!load32(pc, instruction)) {
		fault(ExternalProgramStatus::InstructionAddressFault, pc);
		return false;
	}

	if (trace) {
		_pc = pc;
		traceInstruction(instruction);
	}

#if !defined(LEXXOS_VM_FAST_LOOP)
	++_instructionsExecuted;
#endif

#if LEXXOS_VM_DECODE_CACHE_ENTRIES > 0
	auto& decoded = _decodeCache[(pc >> 2U) & (LEXXOS_VM_DECODE_CACHE_ENTRIES - 1U)];
	if (decoded.pc != pc || decoded.raw != instruction) {
		decoded = {
		    pc,
		    instruction,
		    static_cast<std::uint8_t>(instruction & 0x7FU),
		    static_cast<std::uint8_t>((instruction >> 7U) & 0x1FU),
		    static_cast<std::uint8_t>((instruction >> 12U) & 0x07U),
		    static_cast<std::uint8_t>((instruction >> 15U) & 0x1FU),
		    static_cast<std::uint8_t>((instruction >> 20U) & 0x1FU),
		    static_cast<std::uint8_t>((instruction >> 25U) & 0x7FU),
		};
	}
	auto const opcode = decoded.opcode;
	auto const rd = decoded.rd;
	auto const funct3 = decoded.funct3;
	auto const rs1 = decoded.rs1;
	auto const rs2 = decoded.rs2;
	auto const funct7 = decoded.funct7;
#else
	auto const opcode = instruction & 0x7FU;
	auto const rd = static_cast<std::uint8_t>((instruction >> 7U) & 0x1FU);
	auto const funct3 = static_cast<std::uint8_t>((instruction >> 12U) & 0x07U);
	auto const rs1 = static_cast<std::uint8_t>((instruction >> 15U) & 0x1FU);
	auto const rs2 = static_cast<std::uint8_t>((instruction >> 20U) & 0x1FU);
	auto const funct7 = static_cast<std::uint8_t>((instruction >> 25U) & 0x7FU);
#endif
	auto const nextPc = pc + 4U;

	switch (opcode) {
		case 0x37: // LUI
			writeRegister(rd, immediateU(instruction));
			pc = nextPc;
			return true;
		case 0x17: // AUIPC
			writeRegister(rd, pc + immediateU(instruction));
			pc = nextPc;
			return true;
		case 0x6F: // JAL
			writeRegister(rd, nextPc);
			pc = pc + static_cast<std::uint32_t>(immediateJ(instruction));
			return true;
		case 0x67: { // JALR
			if (funct3 != 0) {
				fault(ExternalProgramStatus::IllegalInstruction, pc, instruction);
				return true;
			}

			auto const target = (readRegister(rs1) + static_cast<std::uint32_t>(immediateI(instruction))) & ~1U;
			writeRegister(rd, nextPc);
			pc = target;
			return true;
		}
		case 0x63: // BRANCH
			executeBranch(funct3, rs1, rs2, instruction, nextPc, pc);
			return true;
		case 0x03: // LOAD
			executeLoad(funct3, rd, rs1, instruction, nextPc, pc);
			return true;
		case 0x23: // STORE
			executeStore(funct3, rs1, rs2, instruction, nextPc, pc);
			return true;
#if defined(LEXXOS_VM_SINGLE_WRITE_ALU)
		case 0x13: // OP-IMM
		case 0x33: // OP
			executeAlu(opcode == 0x33U, funct3, funct7,
			    rd, rs1, rs2, instruction, nextPc, pc);
			return true;
#else
		case 0x13: // OP-IMM
			executeOpImmediate(funct3, funct7, rd, rs1, instruction, nextPc, pc);
			return true;
		case 0x33: // OP
			executeOp(funct3, funct7, rd, rs1, rs2, instruction, nextPc, pc);
			return true;
#endif
		case 0x0F: // FENCE: all guest I/O is already mediated by host syscalls.
			pc = nextPc;
			return true;
		case 0x73: // SYSTEM
			if (instruction == 0x00000073U) {
				_pc = pc;
				handleEcall(nextPc);
				pc = _pc;
				return true;
			}

			fault(ExternalProgramStatus::IllegalInstruction, pc, instruction);
			return true;
		default:
			fault(ExternalProgramStatus::IllegalInstruction, pc, instruction);
			return true;
	}
}

void Rv32iVirtualMachine::executeBranch(std::uint8_t funct3, std::uint8_t rs1, std::uint8_t rs2, std::uint32_t instruction, std::uint32_t nextPc, std::uint32_t& pc) {
	bool take = false;
	switch (funct3) {
		case 0x0:
			take = readRegister(rs1) == readRegister(rs2);
			break;
		case 0x1:
			take = readRegister(rs1) != readRegister(rs2);
			break;
		case 0x4:
			take = static_cast<std::int32_t>(readRegister(rs1)) < static_cast<std::int32_t>(readRegister(rs2));
			break;
		case 0x5:
			take = static_cast<std::int32_t>(readRegister(rs1)) >= static_cast<std::int32_t>(readRegister(rs2));
			break;
		case 0x6:
			take = readRegister(rs1) < readRegister(rs2);
			break;
		case 0x7:
			take = readRegister(rs1) >= readRegister(rs2);
			break;
		default:
			fault(ExternalProgramStatus::IllegalInstruction, pc, instruction);
			return;
	}

	pc = take
	    ? pc + static_cast<std::uint32_t>(immediateB(instruction))
	    : nextPc;
}

void Rv32iVirtualMachine::executeLoad(std::uint8_t funct3, std::uint8_t rd, std::uint8_t rs1, std::uint32_t instruction, std::uint32_t nextPc, std::uint32_t& pc) {
	auto const address = readRegister(rs1) + static_cast<std::uint32_t>(immediateI(instruction));
	switch (funct3) {
		case 0x0: {
			std::uint8_t value = 0;
			if (!load8(address, value)) {
				fault(ExternalProgramStatus::LoadAddressFault, address, instruction);
				return;
			}

			writeRegister(rd, static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))));
			break;
		}
		case 0x1: {
			std::uint16_t value = 0;
			if (!load16(address, value)) {
				fault(ExternalProgramStatus::LoadAddressFault, address, instruction);
				return;
			}

			writeRegister(rd, static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))));
			break;
		}
		case 0x2: {
			std::uint32_t value = 0;
			if (!load32(address, value)) {
				fault(ExternalProgramStatus::LoadAddressFault, address, instruction);
				return;
			}

			writeRegister(rd, value);
			break;
		}
		case 0x4: {
			std::uint8_t value = 0;
			if (!load8(address, value)) {
				fault(ExternalProgramStatus::LoadAddressFault, address, instruction);
				return;
			}

			writeRegister(rd, value);
			break;
		}
		case 0x5: {
			std::uint16_t value = 0;
			if (!load16(address, value)) {
				fault(ExternalProgramStatus::LoadAddressFault, address, instruction);
				return;
			}

			writeRegister(rd, value);
			break;
		}
		default:
			fault(ExternalProgramStatus::IllegalInstruction, pc, instruction);
			return;
	}
	pc = nextPc;
}

void Rv32iVirtualMachine::executeStore(std::uint8_t funct3, std::uint8_t rs1, std::uint8_t rs2, std::uint32_t instruction, std::uint32_t nextPc, std::uint32_t& pc) {
	auto const address = readRegister(rs1) + static_cast<std::uint32_t>(immediateS(instruction));
	bool ok = false;
	switch (funct3) {
		case 0x0:
			ok = store8(address, static_cast<std::uint8_t>(readRegister(rs2) & 0xFFU));
			break;
		case 0x1:
			ok = store16(address, static_cast<std::uint16_t>(readRegister(rs2) & 0xFFFFU));
			break;
		case 0x2:
			ok = store32(address, readRegister(rs2));
			break;
		default:
			fault(ExternalProgramStatus::IllegalInstruction, pc, instruction);
			return;
	}

	if (!ok) {
		fault(ExternalProgramStatus::StoreAddressFault, address, instruction);
		return;
	}

	pc = nextPc;
}

#if defined(LEXXOS_VM_SINGLE_WRITE_ALU)
void Rv32iVirtualMachine::executeAlu(bool registerOperand, std::uint8_t funct3,
    std::uint8_t funct7, std::uint8_t rd, std::uint8_t rs1,
    std::uint8_t rs2, std::uint32_t instruction,
    std::uint32_t nextPc, std::uint32_t& pc) {
	auto const lhs = readRegister(rs1);
	auto const rhs = registerOperand
	    ? readRegister(rs2)
	    : static_cast<std::uint32_t>(immediateI(instruction));
	auto const shift = registerOperand
	    ? (rhs & 31U)
	    : ((instruction >> 20U) & 31U);
	std::uint32_t result = 0;

	// R-type instructions only allow funct7=0,
	// except SUB and SRA (funct7=0x20).
	if (registerOperand && funct7 != 0U && !(funct7 == 0x20U && (funct3 == 0U || funct3 == 5U))) {
		fault(ExternalProgramStatus::IllegalInstruction, pc, instruction);
		return;
	}

	switch (funct3) {
		case 0: // ADDI / ADD / SUB
			result = registerOperand && funct7 == 0x20U
			    ? lhs - rhs
			    : lhs + rhs;
			break;
		case 1: // SLLI / SLL
			if (!registerOperand && funct7 != 0U) {
				fault(ExternalProgramStatus::IllegalInstruction, pc, instruction);
				return;
			}
			result = lhs << shift;
			break;
		case 2: // SLTI / SLT
			result = static_cast<std::int32_t>(lhs)
			    < static_cast<std::int32_t>(rhs);
			break;
		case 3: // SLTIU / SLTU
			result = lhs < rhs;
			break;
		case 4: // XORI / XOR
			result = lhs ^ rhs;
			break;
		case 5: // SRLI / SRL / SRAI / SRA
			if (funct7 != 0U && funct7 != 0x20U) {
				fault(ExternalProgramStatus::IllegalInstruction, pc, instruction);
				return;
			}
			result = funct7 == 0x20U
			    ? static_cast<std::uint32_t>(
			          static_cast<std::int32_t>(lhs) >> shift)
			    : lhs >> shift;
			break;
		case 6: // ORI / OR
			result = lhs | rhs;
			break;
		case 7: // ANDI / AND
			result = lhs & rhs;
			break;
		default:
			fault(ExternalProgramStatus::IllegalInstruction, pc, instruction);
			return;
	}

	writeRegister(rd, result);
	pc = nextPc;
}
#endif

#if defined(LEXXOS_VM_SINGLE_WRITE_ALU)
#else
void Rv32iVirtualMachine::executeOpImmediate(
    std::uint8_t funct3,
    std::uint8_t funct7,
    std::uint8_t rd,
    std::uint8_t rs1,
    std::uint32_t instruction,
    std::uint32_t nextPc,
    std::uint32_t& pc) {
	auto const imm = immediateI(instruction);
	auto const shift = static_cast<std::uint8_t>((instruction >> 20U) & 0x1FU);
	switch (funct3) {
		case 0x0:
			writeRegister(rd, readRegister(rs1) + static_cast<std::uint32_t>(imm));
			break;
		case 0x2:
			writeRegister(rd, static_cast<std::int32_t>(readRegister(rs1)) < imm ? 1U : 0U);
			break;
		case 0x3:
			writeRegister(rd, readRegister(rs1) < static_cast<std::uint32_t>(imm) ? 1U : 0U);
			break;
		case 0x4:
			writeRegister(rd, readRegister(rs1) ^ static_cast<std::uint32_t>(imm));
			break;
		case 0x6:
			writeRegister(rd, readRegister(rs1) | static_cast<std::uint32_t>(imm));
			break;
		case 0x7:
			writeRegister(rd, readRegister(rs1) & static_cast<std::uint32_t>(imm));
			break;
		case 0x1:
			if (funct7 != 0x00U) {
				fault(ExternalProgramStatus::IllegalInstruction, pc, instruction);
				return;
			}

			writeRegister(rd, readRegister(rs1) << shift);
			break;
		case 0x5:
			if (funct7 == 0x00U) {
				writeRegister(rd, readRegister(rs1) >> shift);
			} else if (funct7 == 0x20U) {
				writeRegister(rd, static_cast<std::uint32_t>(static_cast<std::int32_t>(readRegister(rs1)) >> shift));
			} else {
				fault(ExternalProgramStatus::IllegalInstruction, pc, instruction);
				return;
			}

			break;
		default:
			fault(ExternalProgramStatus::IllegalInstruction, pc, instruction);
			return;
	}

	pc = nextPc;
}
#endif

#if defined(LEXXOS_VM_SINGLE_WRITE_ALU)
#else
void Rv32iVirtualMachine::executeOp(
    std::uint8_t funct3,
    std::uint8_t funct7,
    std::uint8_t rd,
    std::uint8_t rs1,
    std::uint8_t rs2,
    std::uint32_t instruction,
    std::uint32_t nextPc,
    std::uint32_t& pc) {
	switch (funct3) {
		case 0x0:
			if (funct7 == 0x00U) {
				writeRegister(rd, readRegister(rs1) + readRegister(rs2));
			} else if (funct7 == 0x20U) {
				writeRegister(rd, readRegister(rs1) - readRegister(rs2));
			} else {
				fault(ExternalProgramStatus::IllegalInstruction, pc, instruction);
				return;
			}
			break;
		case 0x1:
			if (funct7 != 0x00U) {
				fault(ExternalProgramStatus::IllegalInstruction, pc, instruction);
				return;
			}

			writeRegister(rd, readRegister(rs1) << (readRegister(rs2) & 0x1FU));
			break;
		case 0x2:
			writeRegister(rd, static_cast<std::int32_t>(readRegister(rs1)) < static_cast<std::int32_t>(readRegister(rs2)) ? 1U : 0U);
			break;
		case 0x3:
			writeRegister(rd, readRegister(rs1) < readRegister(rs2) ? 1U : 0U);
			break;
		case 0x4:
			writeRegister(rd, readRegister(rs1) ^ readRegister(rs2));
			break;
		case 0x5:
			if (funct7 == 0x00U) {
				writeRegister(rd, readRegister(rs1) >> (readRegister(rs2) & 0x1FU));
			} else if (funct7 == 0x20U) {
				writeRegister(rd, static_cast<std::uint32_t>(static_cast<std::int32_t>(readRegister(rs1)) >> (readRegister(rs2) & 0x1FU)));
			} else {
				fault(ExternalProgramStatus::IllegalInstruction, pc, instruction);
				return;
			}
			break;
		case 0x6:
			writeRegister(rd, readRegister(rs1) | readRegister(rs2));
			break;
		case 0x7:
			writeRegister(rd, readRegister(rs1) & readRegister(rs2));
			break;
		default:
			fault(ExternalProgramStatus::IllegalInstruction, pc, instruction);
			return;
	}

	pc = nextPc;
}
#endif

} // namespace lexxos::external_program::detail
