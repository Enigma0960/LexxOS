#include <lexxos/gameboy/cpu.h>

namespace lexxos::gameboy {

namespace {

	std::uint8_t rotateLeft(std::uint8_t value) {
		return static_cast<std::uint8_t>((value << 1U) | (value >> 7U));
	}

	std::uint8_t rotateRight(std::uint8_t value) {
		return static_cast<std::uint8_t>((value >> 1U) | (value << 7U));
	}

	std::uint8_t highestPriorityInterrupt(std::uint8_t pending) {
		for (auto const mask : {interruptVBlank, interruptLcdStat, interruptTimer, interruptSerial, interruptJoypad}) {
			if ((pending & mask) != 0) {
				return mask;
			}
		}
		return 0;
	}

	std::uint16_t interruptVector(std::uint8_t mask) {
		switch (mask) {
			case interruptVBlank:
				return 0x0040;
			case interruptLcdStat:
				return 0x0048;
			case interruptTimer:
				return 0x0050;
			case interruptSerial:
				return 0x0058;
			case interruptJoypad:
				return 0x0060;
			default:
				return 0;
		}
	}

	struct ImeDelay {
		bool& ime;
		bool& enableAfterInstruction;

		~ImeDelay() {
			if (enableAfterInstruction) {
				ime = true;
			}
		}
	};

} // namespace

std::string_view toString(CpuStepStatus status) {
	switch (status) {
		case CpuStepStatus::Ok:
			return "ok";
		case CpuStepStatus::Halted:
			return "halted";
		case CpuStepStatus::Stopped:
			return "stopped";
		case CpuStepStatus::UnsupportedOpcode:
			return "unsupported-opcode";
		case CpuStepStatus::BusReadError:
			return "bus-read-error";
	}
	return "unknown";
}

void Cpu::resetToPostBootState() {
	_a = 0x01;
	_f = 0xB0;
	_b = 0x00;
	_c = 0x13;
	_d = 0x00;
	_e = 0xD8;
	_h = 0x01;
	_l = 0x4D;
	_sp = 0xFFFE;
	_pc = 0x0100;
	_ime = false;
	_imeEnablePending = false;
	_halted = false;
	_stopped = false;
}

CpuStepResult Cpu::step(Bus& bus) {
	CpuStepResult result;
	result.pcBefore = _pc;

	auto const pending = bus.pendingInterrupts();
	if (pending != 0) {
		_halted = false;
	}

	if (_ime && pending != 0) {
		auto const mask = highestPriorityInterrupt(pending);
		return serviceInterrupt(bus, result, mask, interruptVector(mask));
	}

	if (_stopped) {
		result.status = CpuStepStatus::Stopped;
		return result;
	}

	if (_halted) {
		result.status = CpuStepStatus::Halted;
		result.cycles = 4;
		return result;
	}

	bool enableImeAfterInstruction = _imeEnablePending;
	_imeEnablePending = false;
	ImeDelay imeDelay{_ime, enableImeAfterInstruction};

	std::uint8_t opcode = 0;
	if (fetch8(bus, opcode) != lexxos::vfs::ReadStatus::Ok) {
		result.status = CpuStepStatus::BusReadError;
		return result;
	}
	result.opcode = opcode;
	result.cycles = 4;

	if ((opcode & 0xC0U) == 0x40U && opcode != 0x76U) {
		auto const destination = static_cast<std::uint8_t>((opcode >> 3U) & 0x07U);
		auto const source = static_cast<std::uint8_t>(opcode & 0x07U);
		lexxos::vfs::ReadStatus status = lexxos::vfs::ReadStatus::Ok;
		auto const value = readRegister(bus, source, status);
		if (status != lexxos::vfs::ReadStatus::Ok) {
			result.status = CpuStepStatus::BusReadError;
			return result;
		}
		writeRegister(bus, destination, value);
		result.cycles = destination == 6 || source == 6 ? 8 : 4;
		return result;
	}

	if ((opcode & 0xC0U) == 0x80U) {
		auto const operation = static_cast<std::uint8_t>((opcode >> 3U) & 0x07U);
		auto const source = static_cast<std::uint8_t>(opcode & 0x07U);
		lexxos::vfs::ReadStatus status = lexxos::vfs::ReadStatus::Ok;
		auto const value = readRegister(bus, source, status);
		if (status != lexxos::vfs::ReadStatus::Ok) {
			result.status = CpuStepStatus::BusReadError;
			return result;
		}

		switch (operation) {
			case 0:
				addA(value, false);
				break;
			case 1:
				addA(value, true);
				break;
			case 2:
				subA(value, false);
				break;
			case 3:
				subA(value, true);
				break;
			case 4:
				andA(value);
				break;
			case 5:
				xorA(value);
				break;
			case 6:
				orA(value);
				break;
			case 7:
				cpA(value);
				break;
		}
		result.cycles = source == 6 ? 8 : 4;
		return result;
	}

	switch (opcode) {
		case 0x00:
			result.cycles = 4;
			break;
		case 0x01: {
			std::uint16_t value = 0;
			if (fetch16(bus, value) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			setBc(value);
			result.cycles = 12;
			break;
		}
		case 0x02:
			bus.write8Fast(bc(), _a);
			result.cycles = 8;
			break;
		case 0x03:
			setBc(static_cast<std::uint16_t>(bc() + 1U));
			result.cycles = 8;
			break;
		case 0x04:
			_b = inc8(_b);
			break;
		case 0x05:
			_b = dec8(_b);
			break;
		case 0x06:
			if (fetch8(bus, _b) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			result.cycles = 8;
			break;
		case 0x07: {
			auto const carry = (_a & 0x80U) != 0;
			_a = rotateLeft(_a);
			_f = 0;
			setFlag(flagC, carry);
			break;
		}
		case 0x08: {
			std::uint16_t address = 0;
			if (fetch16(bus, address) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			bus.write16(address, _sp);
			result.cycles = 20;
			break;
		}
		case 0x09:
			addHl(bc());
			result.cycles = 8;
			break;
		case 0x0A:
			_a = bus.read8Fast(bc());
			result.cycles = 8;
			break;

		case 0x0B:
			setBc(static_cast<std::uint16_t>(bc() - 1U));
			result.cycles = 8;
			break;
		case 0x0C:
			_c = inc8(_c);
			break;
		case 0x0D:
			_c = dec8(_c);
			break;
		case 0x0E:
			if (fetch8(bus, _c) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			result.cycles = 8;
			break;
		case 0x0F: {
			auto const carry = (_a & 0x01U) != 0;
			_a = rotateRight(_a);
			_f = 0;
			setFlag(flagC, carry);
			break;
		}
		case 0x10:
			(void) fetch8(bus, opcode);
			_stopped = true;
			result.status = CpuStepStatus::Stopped;
			result.cycles = 4;
			break;
		case 0x11: {
			std::uint16_t value = 0;
			if (fetch16(bus, value) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			setDe(value);
			result.cycles = 12;
			break;
		}
		case 0x12:
			bus.write8Fast(de(), _a);
			result.cycles = 8;
			break;
		case 0x13:
			setDe(static_cast<std::uint16_t>(de() + 1U));
			result.cycles = 8;
			break;
		case 0x14:
			_d = inc8(_d);
			break;
		case 0x15:
			_d = dec8(_d);
			break;
		case 0x16:
			if (fetch8(bus, _d) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			result.cycles = 8;
			break;
		case 0x17: {
			auto const previousCarry = flag(flagC);
			auto const carry = (_a & 0x80U) != 0;
			_a = static_cast<std::uint8_t>((_a << 1U) | (previousCarry ? 1U : 0U));
			_f = 0;
			setFlag(flagC, carry);
			break;
		}
		case 0x18: {
			std::uint8_t value = 0;
			if (fetch8(bus, value) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			_pc = static_cast<std::uint16_t>(_pc + static_cast<std::int8_t>(value));
			result.cycles = 12;
			break;
		}
		case 0x19:
			addHl(de());
			result.cycles = 8;
			break;
		case 0x1A:
			_a = bus.read8Fast(de());
			result.cycles = 8;
			break;
		case 0x1B:
			setDe(static_cast<std::uint16_t>(de() - 1U));
			result.cycles = 8;
			break;
		case 0x1C:
			_e = inc8(_e);
			break;
		case 0x1D:
			_e = dec8(_e);
			break;
		case 0x1E:
			if (fetch8(bus, _e) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			result.cycles = 8;
			break;
		case 0x1F: {
			auto const previousCarry = flag(flagC);
			auto const carry = (_a & 0x01U) != 0;
			_a = static_cast<std::uint8_t>((_a >> 1U) | (previousCarry ? 0x80U : 0U));
			_f = 0;
			setFlag(flagC, carry);
			break;
		}
		case 0x20:
		case 0x28:
		case 0x30:
		case 0x38: {
			std::uint8_t offset = 0;
			if (fetch8(bus, offset) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			auto const conditionCode = static_cast<std::uint8_t>((opcode >> 3U) & 0x03U);
			if (condition(conditionCode)) {
				_pc = static_cast<std::uint16_t>(_pc + static_cast<std::int8_t>(offset));
				result.cycles = 12;
			} else {
				result.cycles = 8;
			}
			break;
		}
		case 0x21: {
			std::uint16_t value = 0;
			if (fetch16(bus, value) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			setHl(value);
			result.cycles = 12;
			break;
		}
		case 0x22:
			bus.write8Fast(hl(), _a);
			setHl(static_cast<std::uint16_t>(hl() + 1U));
			result.cycles = 8;
			break;
		case 0x23:
			setHl(static_cast<std::uint16_t>(hl() + 1U));
			result.cycles = 8;
			break;
		case 0x24:
			_h = inc8(_h);
			break;
		case 0x25:
			_h = dec8(_h);
			break;
		case 0x26:
			if (fetch8(bus, _h) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			result.cycles = 8;
			break;
		case 0x27:
			daa();
			break;
		case 0x29:
			addHl(hl());
			result.cycles = 8;
			break;
		case 0x2A:
			_a = bus.read8Fast(hl());
			setHl(static_cast<std::uint16_t>(hl() + 1U));
			result.cycles = 8;
			break;
		case 0x2B:
			setHl(static_cast<std::uint16_t>(hl() - 1U));
			result.cycles = 8;
			break;
		case 0x2C:
			_l = inc8(_l);
			break;
		case 0x2D:
			_l = dec8(_l);
			break;
		case 0x2E:
			if (fetch8(bus, _l) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			result.cycles = 8;
			break;
		case 0x2F:
			_a = static_cast<std::uint8_t>(~_a);
			setFlag(flagN, true);
			setFlag(flagH, true);
			break;
		case 0x31: {
			std::uint16_t value = 0;
			if (fetch16(bus, value) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			_sp = value;
			result.cycles = 12;
			break;
		}
		case 0x32:
			bus.write8Fast(hl(), _a);
			setHl(static_cast<std::uint16_t>(hl() - 1U));
			result.cycles = 8;
			break;
		case 0x33:
			++_sp;
			result.cycles = 8;
			break;
		case 0x34: {
			auto const address = hl();
			auto const value = bus.read8Fast(address);
			bus.write8Fast(address, inc8(value));
			result.cycles = 12;
			break;
		}
		case 0x35: {
			auto const address = hl();
			auto const value = bus.read8Fast(address);
			bus.write8Fast(address, dec8(value));
			result.cycles = 12;
			break;
		}
		case 0x36: {
			std::uint8_t value = 0;
			if (fetch8(bus, value) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			bus.write8Fast(hl(), value);
			result.cycles = 12;
			break;
		}
		case 0x37:
			setFlag(flagN, false);
			setFlag(flagH, false);
			setFlag(flagC, true);
			break;
		case 0x39:
			addHl(_sp);
			result.cycles = 8;
			break;
		case 0x3A:
			_a = bus.read8Fast(hl());
			setHl(static_cast<std::uint16_t>(hl() - 1U));
			result.cycles = 8;
			break;
		case 0x3B:
			--_sp;
			result.cycles = 8;
			break;
		case 0x3C:
			_a = inc8(_a);
			break;
		case 0x3D:
			_a = dec8(_a);
			break;
		case 0x3E:
			if (fetch8(bus, _a) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			result.cycles = 8;
			break;
		case 0x3F: {
			auto const carry = flag(flagC);
			setFlag(flagN, false);
			setFlag(flagH, false);
			setFlag(flagC, !carry);
			break;
		}
		case 0x76:
			_halted = true;
			result.status = CpuStepStatus::Halted;
			result.cycles = 4;
			break;
		case 0xC0:
		case 0xC8:
		case 0xD0:
		case 0xD8: {
			auto const conditionCode = static_cast<std::uint8_t>((opcode >> 3U) & 0x03U);
			if (condition(conditionCode)) {
				_pc = pop16(bus);
				result.cycles = 20;
			} else {
				result.cycles = 8;
			}
			break;
		}
		case 0xC1:
			setBc(pop16(bus));
			result.cycles = 12;
			break;
		case 0xC2:
		case 0xCA:
		case 0xD2:
		case 0xDA: {
			std::uint16_t address = 0;
			if (fetch16(bus, address) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			auto const conditionCode = static_cast<std::uint8_t>((opcode >> 3U) & 0x03U);
			if (condition(conditionCode)) {
				_pc = address;
				result.cycles = 16;
			} else {
				result.cycles = 12;
			}
			break;
		}
		case 0xC3: {
			std::uint16_t address = 0;
			if (fetch16(bus, address) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			_pc = address;
			result.cycles = 16;
			break;
		}
		case 0xC4:
		case 0xCC:
		case 0xD4:
		case 0xDC: {
			std::uint16_t address = 0;
			if (fetch16(bus, address) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			auto const conditionCode = static_cast<std::uint8_t>((opcode >> 3U) & 0x03U);
			if (condition(conditionCode)) {
				push16(bus, _pc);
				_pc = address;
				result.cycles = 24;
			} else {
				result.cycles = 12;
			}
			break;
		}
		case 0xC5:
			push16(bus, bc());
			result.cycles = 16;
			break;
		case 0xC6: {
			std::uint8_t value = 0;
			if (fetch8(bus, value) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			addA(value, false);
			result.cycles = 8;
			break;
		}
		case 0xC7:
		case 0xCF:
		case 0xD7:
		case 0xDF:
		case 0xE7:
		case 0xEF:
		case 0xF7:
		case 0xFF:
			push16(bus, _pc);
			_pc = static_cast<std::uint16_t>(opcode & 0x38U);
			result.cycles = 16;
			break;
		case 0xC9:
			_pc = pop16(bus);
			result.cycles = 16;
			break;
		case 0xCB:
			return executeCb(bus, result);
		case 0xCD: {
			std::uint16_t address = 0;
			if (fetch16(bus, address) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			push16(bus, _pc);
			_pc = address;
			result.cycles = 24;
			break;
		}
		case 0xCE: {
			std::uint8_t value = 0;
			if (fetch8(bus, value) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			addA(value, true);
			result.cycles = 8;
			break;
		}
		case 0xD1:
			setDe(pop16(bus));
			result.cycles = 12;
			break;
		case 0xD5:
			push16(bus, de());
			result.cycles = 16;
			break;
		case 0xD6: {
			std::uint8_t value = 0;
			if (fetch8(bus, value) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			subA(value, false);
			result.cycles = 8;
			break;
		}
		case 0xD9:
			_pc = pop16(bus);
			_ime = true;
			_imeEnablePending = false;
			enableImeAfterInstruction = false;
			result.cycles = 16;
			break;
		case 0xDE: {
			std::uint8_t value = 0;
			if (fetch8(bus, value) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			subA(value, true);
			result.cycles = 8;
			break;
		}
		case 0xE0: {
			std::uint8_t offset = 0;
			if (fetch8(bus, offset) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			bus.write8Fast(static_cast<std::uint16_t>(0xFF00U + offset), _a);
			result.cycles = 12;
			break;
		}
		case 0xE1:
			setHl(pop16(bus));
			result.cycles = 12;
			break;
		case 0xE2:
			bus.write8Fast(static_cast<std::uint16_t>(0xFF00U + _c), _a);
			result.cycles = 8;
			break;
		case 0xE5:
			push16(bus, hl());
			result.cycles = 16;
			break;
		case 0xE6: {
			std::uint8_t value = 0;
			if (fetch8(bus, value) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			andA(value);
			result.cycles = 8;
			break;
		}
		case 0xE8: {
			std::uint8_t value = 0;
			if (fetch8(bus, value) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			addSpSigned(static_cast<std::int8_t>(value));
			result.cycles = 16;
			break;
		}
		case 0xE9:
			_pc = hl();
			result.cycles = 4;
			break;
		case 0xEA: {
			std::uint16_t address = 0;
			if (fetch16(bus, address) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			bus.write8Fast(address, _a);
			result.cycles = 16;
			break;
		}
		case 0xEE: {
			std::uint8_t value = 0;
			if (fetch8(bus, value) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			xorA(value);
			result.cycles = 8;
			break;
		}
		case 0xF0: {
			std::uint8_t offset = 0;
			if (fetch8(bus, offset) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			_a = bus.read8Fast(static_cast<std::uint16_t>(0xFF00U + offset));
			result.cycles = 12;
			break;
		}
		case 0xF1:
			setAf(pop16(bus));
			result.cycles = 12;
			break;
		case 0xF2:
			_a = bus.read8Fast(static_cast<std::uint16_t>(0xFF00U + _c));
			result.cycles = 8;
			break;
		case 0xF3:
			_ime = false;
			_imeEnablePending = false;
			enableImeAfterInstruction = false;
			break;
		case 0xF5:
			push16(bus, af());
			result.cycles = 16;
			break;
		case 0xF6: {
			std::uint8_t value = 0;
			if (fetch8(bus, value) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			orA(value);
			result.cycles = 8;
			break;
		}
		case 0xF8: {
			std::uint8_t value = 0;
			if (fetch8(bus, value) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			auto const signedValue = static_cast<std::int8_t>(value);
			auto const unsignedOffset = static_cast<std::uint16_t>(static_cast<std::uint8_t>(value));
			auto const spValue = _sp;
			setHl(static_cast<std::uint16_t>(_sp + signedValue));
			_f = 0;
			setFlag(flagH, ((spValue & 0x0FU) + (unsignedOffset & 0x0FU)) > 0x0FU);
			setFlag(flagC, ((spValue & 0xFFU) + (unsignedOffset & 0xFFU)) > 0xFFU);
			result.cycles = 12;
			break;
		}
		case 0xF9:
			_sp = hl();
			result.cycles = 8;
			break;
		case 0xFA: {
			std::uint16_t address = 0;
			if (fetch16(bus, address) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			_a = bus.read8Fast(address);
			result.cycles = 16;
			break;
		}
		case 0xFB:
			_imeEnablePending = true;
			break;
		case 0xFE: {
			std::uint8_t value = 0;
			if (fetch8(bus, value) != lexxos::vfs::ReadStatus::Ok) {
				result.status = CpuStepStatus::BusReadError;
				return result;
			}
			cpA(value);
			result.cycles = 8;
			break;
		}
		default:
			result.status = CpuStepStatus::UnsupportedOpcode;
			break;
	}

	return result;
}

CpuStepResult Cpu::serviceInterrupt(Bus& bus, CpuStepResult result, std::uint8_t mask, std::uint16_t vector) {
	bus.clearInterrupt(mask);
	_ime = false;
	_halted = false;
	push16(bus, _pc);
	_pc = vector;
	result.status = CpuStepStatus::Ok;
	result.servicedInterrupt = true;
	result.interruptMask = mask;
	result.interruptVector = vector;
	result.cycles = 20;
	return result;
}

std::uint16_t Cpu::af() const {
	return static_cast<std::uint16_t>((static_cast<std::uint16_t>(_a) << 8U) | (_f & 0xF0U));
}

std::uint16_t Cpu::bc() const {
	return static_cast<std::uint16_t>((static_cast<std::uint16_t>(_b) << 8U) | _c);
}

std::uint16_t Cpu::de() const {
	return static_cast<std::uint16_t>((static_cast<std::uint16_t>(_d) << 8U) | _e);
}

std::uint16_t Cpu::hl() const {
	return static_cast<std::uint16_t>((static_cast<std::uint16_t>(_h) << 8U) | _l);
}

std::uint16_t Cpu::sp() const {
	return _sp;
}

std::uint16_t Cpu::pc() const {
	return _pc;
}

bool Cpu::ime() const {
	return _ime;
}

bool Cpu::halted() const {
	return _halted;
}

bool Cpu::stopped() const {
	return _stopped;
}

void Cpu::setPc(std::uint16_t value) {
	_pc = value;
}

void Cpu::setIme(bool enabled) {
	_ime = enabled;
	_imeEnablePending = false;
}

bool Cpu::flag(std::uint8_t mask) const {
	return (_f & mask) != 0;
}

void Cpu::setFlag(std::uint8_t mask, bool enabled) {
	if (enabled) {
		_f = static_cast<std::uint8_t>((_f | mask) & 0xF0U);
	} else {
		_f = static_cast<std::uint8_t>((_f & ~mask) & 0xF0U);
	}
}

void Cpu::setAf(std::uint16_t value) {
	_a = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
	_f = static_cast<std::uint8_t>(value & 0xF0U);
}

void Cpu::setBc(std::uint16_t value) {
	_b = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
	_c = static_cast<std::uint8_t>(value & 0xFFU);
}

void Cpu::setDe(std::uint16_t value) {
	_d = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
	_e = static_cast<std::uint8_t>(value & 0xFFU);
}

void Cpu::setHl(std::uint16_t value) {
	_h = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
	_l = static_cast<std::uint8_t>(value & 0xFFU);
}

void Cpu::setSp(std::uint16_t value) {
	_sp = value;
}

lexxos::vfs::ReadStatus Cpu::fetch8(Bus& bus, std::uint8_t& value) {
	value = bus.readProgram8(_pc);
	++_pc;
	return lexxos::vfs::ReadStatus::Ok;
}

lexxos::vfs::ReadStatus Cpu::fetch16(Bus& bus, std::uint16_t& value) {
	value = bus.readProgram16(_pc);
	_pc = static_cast<std::uint16_t>(_pc + 2U);
	return lexxos::vfs::ReadStatus::Ok;
}

std::uint8_t Cpu::readRegister(Bus& bus, std::uint8_t index, lexxos::vfs::ReadStatus& status) {
	status = lexxos::vfs::ReadStatus::Ok;
	switch (index & 0x07U) {
		case 0:
			return _b;
		case 1:
			return _c;
		case 2:
			return _d;
		case 3:
			return _e;
		case 4:
			return _h;
		case 5:
			return _l;
		case 6:
			return bus.read8Fast(hl());
		case 7:
			return _a;
	}
	return 0xFF;
}

void Cpu::writeRegister(Bus& bus, std::uint8_t index, std::uint8_t value) {
	switch (index & 0x07U) {
		case 0:
			_b = value;
			break;
		case 1:
			_c = value;
			break;
		case 2:
			_d = value;
			break;
		case 3:
			_e = value;
			break;
		case 4:
			_h = value;
			break;
		case 5:
			_l = value;
			break;
		case 6:
			bus.write8Fast(hl(), value);
			break;
		case 7:
			_a = value;
			break;
	}
}

std::uint8_t Cpu::inc8(std::uint8_t value) {
	auto const result = static_cast<std::uint8_t>(value + 1U);
	setFlag(flagZ, result == 0);
	setFlag(flagN, false);
	setFlag(flagH, (value & 0x0FU) == 0x0FU);
	return result;
}

std::uint8_t Cpu::dec8(std::uint8_t value) {
	auto const result = static_cast<std::uint8_t>(value - 1U);
	setFlag(flagZ, result == 0);
	setFlag(flagN, true);
	setFlag(flagH, (value & 0x0FU) == 0x00U);
	return result;
}

void Cpu::addA(std::uint8_t value, bool withCarry) {
	auto const carry = withCarry && flag(flagC) ? 1U : 0U;
	auto const lhs = _a;
	auto const sum = static_cast<std::uint16_t>(lhs + value + carry);
	_a = static_cast<std::uint8_t>(sum & 0xFFU);
	_f = 0;
	setFlag(flagZ, _a == 0);
	setFlag(flagH, ((lhs & 0x0FU) + (value & 0x0FU) + carry) > 0x0FU);
	setFlag(flagC, sum > 0xFFU);
}

void Cpu::subA(std::uint8_t value, bool withCarry) {
	auto const carry = withCarry && flag(flagC) ? 1U : 0U;
	auto const lhs = _a;
	auto const diff = static_cast<std::uint16_t>(lhs - value - carry);
	_a = static_cast<std::uint8_t>(diff & 0xFFU);
	_f = flagN;
	setFlag(flagZ, _a == 0);
	setFlag(flagH, (lhs & 0x0FU) < ((value & 0x0FU) + carry));
	setFlag(flagC, lhs < static_cast<std::uint16_t>(value + carry));
}

void Cpu::andA(std::uint8_t value) {
	_a = static_cast<std::uint8_t>(_a & value);
	_f = flagH;
	setFlag(flagZ, _a == 0);
}

void Cpu::xorA(std::uint8_t value) {
	_a = static_cast<std::uint8_t>(_a ^ value);
	_f = 0;
	setFlag(flagZ, _a == 0);
}

void Cpu::orA(std::uint8_t value) {
	_a = static_cast<std::uint8_t>(_a | value);
	_f = 0;
	setFlag(flagZ, _a == 0);
}

void Cpu::cpA(std::uint8_t value) {
	auto const lhs = _a;
	auto const diff = static_cast<std::uint16_t>(lhs - value);
	_f = flagN;
	setFlag(flagZ, static_cast<std::uint8_t>(diff & 0xFFU) == 0);
	setFlag(flagH, (lhs & 0x0FU) < (value & 0x0FU));
	setFlag(flagC, lhs < value);
}

void Cpu::addHl(std::uint16_t value) {
	auto const lhs = hl();
	auto const sum = static_cast<std::uint32_t>(lhs + value);
	setHl(static_cast<std::uint16_t>(sum & 0xFFFFU));
	setFlag(flagN, false);
	setFlag(flagH, ((lhs & 0x0FFFU) + (value & 0x0FFFU)) > 0x0FFFU);
	setFlag(flagC, sum > 0xFFFFU);
}

void Cpu::addSpSigned(std::int8_t value) {
	auto const unsignedOffset = static_cast<std::uint16_t>(static_cast<std::uint8_t>(value));
	auto const oldSp = _sp;
	_sp = static_cast<std::uint16_t>(_sp + value);
	_f = 0;
	setFlag(flagH, ((oldSp & 0x0FU) + (unsignedOffset & 0x0FU)) > 0x0FU);
	setFlag(flagC, ((oldSp & 0xFFU) + (unsignedOffset & 0xFFU)) > 0xFFU);
}

void Cpu::daa() {
	std::uint8_t correction = 0;
	bool carry = flag(flagC);

	if (!flag(flagN)) {
		if (flag(flagH) || (_a & 0x0FU) > 0x09U) {
			correction |= 0x06U;
		}
		if (carry || _a > 0x99U) {
			correction |= 0x60U;
			carry = true;
		}
		_a = static_cast<std::uint8_t>(_a + correction);
	} else {
		if (flag(flagH)) {
			correction |= 0x06U;
		}
		if (carry) {
			correction |= 0x60U;
		}
		_a = static_cast<std::uint8_t>(_a - correction);
	}

	setFlag(flagZ, _a == 0);
	setFlag(flagH, false);
	setFlag(flagC, carry);
}

bool Cpu::condition(std::uint8_t conditionCode) const {
	switch (conditionCode & 0x03U) {
		case 0:
			return !flag(flagZ);
		case 1:
			return flag(flagZ);
		case 2:
			return !flag(flagC);
		case 3:
			return flag(flagC);
	}
	return false;
}

void Cpu::push16(Bus& bus, std::uint16_t value) {
	--_sp;
	bus.write8Fast(_sp, static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
	--_sp;
	bus.write8Fast(_sp, static_cast<std::uint8_t>(value & 0xFFU));
}

std::uint16_t Cpu::pop16(Bus& bus) {
	auto const low = static_cast<std::uint16_t>(bus.read8Fast(_sp));
	++_sp;
	auto const high = static_cast<std::uint16_t>(bus.read8Fast(_sp));
	++_sp;
	return static_cast<std::uint16_t>(low | (high << 8U));
}

CpuStepResult Cpu::executeCb(Bus& bus, CpuStepResult result) {
	std::uint8_t opcode = 0;
	if (fetch8(bus, opcode) != lexxos::vfs::ReadStatus::Ok) {
		result.status = CpuStepStatus::BusReadError;
		return result;
	}
	result.prefixedOpcode = opcode;

	auto const operationGroup = static_cast<std::uint8_t>((opcode >> 6U) & 0x03U);
	auto const bitOrOperation = static_cast<std::uint8_t>((opcode >> 3U) & 0x07U);
	auto const registerIndex = static_cast<std::uint8_t>(opcode & 0x07U);
	lexxos::vfs::ReadStatus status = lexxos::vfs::ReadStatus::Ok;
	auto value = readRegister(bus, registerIndex, status);
	if (status != lexxos::vfs::ReadStatus::Ok) {
		result.status = CpuStepStatus::BusReadError;
		return result;
	}

	if (operationGroup == 0) {
		bool carry = false;
		switch (bitOrOperation) {
			case 0:
				carry = (value & 0x80U) != 0;
				value = rotateLeft(value);
				break;
			case 1:
				carry = (value & 0x01U) != 0;
				value = rotateRight(value);
				break;
			case 2: {
				auto const previousCarry = flag(flagC);
				carry = (value & 0x80U) != 0;
				value = static_cast<std::uint8_t>((value << 1U) | (previousCarry ? 1U : 0U));
				break;
			}
			case 3: {
				auto const previousCarry = flag(flagC);
				carry = (value & 0x01U) != 0;
				value = static_cast<std::uint8_t>((value >> 1U) | (previousCarry ? 0x80U : 0U));
				break;
			}
			case 4:
				carry = (value & 0x80U) != 0;
				value = static_cast<std::uint8_t>(value << 1U);
				break;
			case 5:
				carry = (value & 0x01U) != 0;
				value = static_cast<std::uint8_t>((value >> 1U) | (value & 0x80U));
				break;
			case 6:
				value = static_cast<std::uint8_t>((value >> 4U) | (value << 4U));
				carry = false;
				break;
			case 7:
				carry = (value & 0x01U) != 0;
				value = static_cast<std::uint8_t>(value >> 1U);
				break;
		}
		writeRegister(bus, registerIndex, value);
		_f = 0;
		setFlag(flagZ, value == 0);
		setFlag(flagC, carry);
		result.cycles = registerIndex == 6 ? 16 : 8;
		return result;
	}

	if (operationGroup == 1) {
		setFlag(flagZ, (value & (1U << bitOrOperation)) == 0);
		setFlag(flagN, false);
		setFlag(flagH, true);
		result.cycles = registerIndex == 6 ? 12 : 8;
		return result;
	}

	if (operationGroup == 2) {
		value = static_cast<std::uint8_t>(value & ~(1U << bitOrOperation));
		writeRegister(bus, registerIndex, value);
		result.cycles = registerIndex == 6 ? 16 : 8;
		return result;
	}

	value = static_cast<std::uint8_t>(value | (1U << bitOrOperation));
	writeRegister(bus, registerIndex, value);
	result.cycles = registerIndex == 6 ? 16 : 8;
	return result;
}

} // namespace lexxos::gameboy
