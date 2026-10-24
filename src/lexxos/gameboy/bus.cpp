#include <lexxos/gameboy/bus.h>

namespace lexxos::gameboy {

namespace {

	constexpr std::uint16_t joypadAddress = 0xFF00;
	constexpr std::uint16_t serialDataAddress = 0xFF01;
	constexpr std::uint16_t serialControlAddress = 0xFF02;
	constexpr std::uint16_t dividerAddress = 0xFF04;
	constexpr std::uint16_t timerCounterAddress = 0xFF05;
	constexpr std::uint16_t timerModuloAddress = 0xFF06;
	constexpr std::uint16_t timerControlAddress = 0xFF07;
	constexpr std::uint16_t interruptFlagAddress = 0xFF0F;
	constexpr std::uint16_t dmaAddress = 0xFF46;
	constexpr std::uint16_t lcdControlAddress = 0xFF40;
	constexpr std::uint16_t lcdStatusAddress = 0xFF41;
	constexpr std::uint16_t lyAddress = 0xFF44;
	constexpr std::uint16_t lyCompareAddress = 0xFF45;
	constexpr std::uint16_t bgPaletteAddress = 0xFF47;
	constexpr std::uint16_t objPalette0Address = 0xFF48;
	constexpr std::uint16_t objPalette1Address = 0xFF49;
	constexpr std::uint16_t windowYAddress = 0xFF4A;
	constexpr std::uint16_t windowXAddress = 0xFF4B;

	constexpr std::uint8_t serialTransferStart = 0x80;
	constexpr std::uint8_t serialInternalClock = 0x01;
	constexpr std::uint16_t serialInternalTransferCycles = 4096;

	constexpr std::uint8_t lcdControlLcdEnable = 0x80;
	constexpr std::uint8_t statModeMask = 0x03;
	constexpr std::uint8_t statCoincidenceFlag = 0x04;
	constexpr std::uint8_t statHBlankInterruptEnable = 0x08;
	constexpr std::uint8_t statVBlankInterruptEnable = 0x10;
	constexpr std::uint8_t statOamInterruptEnable = 0x20;
	constexpr std::uint8_t statLycInterruptEnable = 0x40;
	constexpr std::uint8_t statWritableMask = 0x78;

	constexpr std::uint8_t ppuModeHBlank = 0;
	constexpr std::uint8_t ppuModeVBlank = 1;
	constexpr std::uint8_t ppuModeOam = 2;
	constexpr std::uint8_t ppuModeTransfer = 3;

	constexpr std::uint8_t joypadSelectDirections = 0x10;
	constexpr std::uint8_t joypadSelectButtons = 0x20;

	std::uint16_t timerPeriod(std::uint8_t tac) {
		switch (tac & 0x03U) {
			case 0:
				return 1024;
			case 1:
				return 16;
			case 2:
				return 64;
			case 3:
				return 256;
		}
		return 1024;
	}

} // namespace

void Bus::reset() {
	_cartridge = nullptr;
	_vram = {};
	_wram = {};
	_oam = {};
	_io = {};
	_hram = {};
	_interruptEnable = 0;
	_joypad = {};
	_joypadSelect = 0x30;
	_dividerCounter = 0;
	_timerCounter = 0;
	_serialTransferCycles = 0;
	_serialTransferByte = 0;
	_scanlineDots = 0;
	_vramWriteCount = 0;
	_oamWriteCount = 0;
	_serialOutput = {};
	_serialOutputSize = 0;
	_serialOutputOverflowed = false;

	_apu.reset();

	_io[static_cast<std::size_t>(joypadAddress - ioStart)] = 0xCF;
	_io[static_cast<std::size_t>(serialControlAddress - ioStart)] = 0x7E;
	_io[static_cast<std::size_t>(timerControlAddress - ioStart)] = 0xF8;
	_io[static_cast<std::size_t>(interruptFlagAddress - ioStart)] = 0xE0;
	_io[static_cast<std::size_t>(lcdControlAddress - ioStart)] = 0x91;
	_io[static_cast<std::size_t>(lcdStatusAddress - ioStart)] = 0x85;
	_io[static_cast<std::size_t>(lyAddress - ioStart)] = 0x00;
	_io[static_cast<std::size_t>(lyCompareAddress - ioStart)] = 0x00;
	_io[static_cast<std::size_t>(bgPaletteAddress - ioStart)] = 0xFC;
	_io[static_cast<std::size_t>(objPalette0Address - ioStart)] = 0xFF;
	_io[static_cast<std::size_t>(objPalette1Address - ioStart)] = 0xFF;
	_io[static_cast<std::size_t>(windowYAddress - ioStart)] = 0x00;
	_io[static_cast<std::size_t>(windowXAddress - ioStart)] = 0x00;
}

void Bus::attachCartridge(Cartridge* cartridge) {
	_cartridge = cartridge;
}

Cartridge* Bus::cartridge() {
	return _cartridge;
}

Cartridge const* Bus::cartridge() const {
	return _cartridge;
}

lexxos::vfs::ReadStatus Bus::read8(std::uint16_t address, std::uint8_t& value) {
	value = 0xFF;

	if (address <= romEnd) {
		if (_cartridge == nullptr) {
			return lexxos::vfs::ReadStatus::NotReadable;
		}
		return _cartridge->read8(address, value);
	}

	if (address >= vramStart && address <= vramEnd) {
		value = _vram[static_cast<std::size_t>(address - vramStart)];
		return lexxos::vfs::ReadStatus::Ok;
	}

	if (address >= externalRamStart && address <= externalRamEnd) {
		if (_cartridge == nullptr) {
			value = 0xFF;
			return lexxos::vfs::ReadStatus::Ok;
		}
		return _cartridge->readExternalRam(address, value);
	}

	if (address >= wramStart && address <= wramEnd) {
		value = _wram[static_cast<std::size_t>(address - wramStart)];
		return lexxos::vfs::ReadStatus::Ok;
	}

	if (isEchoRamAddress(address)) {
		value = _wram[echoRamIndex(address)];
		return lexxos::vfs::ReadStatus::Ok;
	}

	if (address >= oamStart && address <= oamEnd) {
		value = _oam[static_cast<std::size_t>(address - oamStart)];
		return lexxos::vfs::ReadStatus::Ok;
	}

	if (address >= unusableStart && address <= unusableEnd) {
		value = 0xFF;
		return lexxos::vfs::ReadStatus::Ok;
	}

	if (address >= ioStart && address <= ioEnd) {
		value = readIo(address);
		return lexxos::vfs::ReadStatus::Ok;
	}

	if (address >= hramStart && address <= hramEnd) {
		value = _hram[static_cast<std::size_t>(address - hramStart)];
		return lexxos::vfs::ReadStatus::Ok;
	}

	value = _interruptEnable;
	return lexxos::vfs::ReadStatus::Ok;
}

std::uint8_t Bus::read8Fast(std::uint16_t address) {
	switch (address >> 12U) {
		case 0x0:
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
			return _cartridge != nullptr ? _cartridge->read8Fast(address) : 0xFF;
		case 0x8:
		case 0x9:
			return _vram[static_cast<std::size_t>(address - vramStart)];
		case 0xA:
		case 0xB:
			return _cartridge != nullptr ? _cartridge->readExternalRamFast(address) : 0xFF;
		case 0xC:
		case 0xD:
			return _wram[static_cast<std::size_t>(address - wramStart)];
		case 0xE:
			return _wram[static_cast<std::size_t>(address - echoRamStart)];
		case 0xF:
			if (address < oamStart) {
				return _wram[static_cast<std::size_t>(address - echoRamStart)];
			}
			if (address < unusableStart) {
				return _oam[static_cast<std::size_t>(address - oamStart)];
			}
			if (address < ioStart) {
				return 0xFF;
			}
			if (address < hramStart) {
				return readIo(address);
			}
			if (address < interruptEnableAddress) {
				return _hram[static_cast<std::size_t>(address - hramStart)];
			}
			return _interruptEnable;
	}
	return 0xFF;
}

std::uint8_t Bus::readProgram8(std::uint16_t address) {
	if (_cartridge != nullptr) {
		if (address < 0x4000U) {
			auto const* fixed = _cartridge->fixedRomData();
			if (fixed != nullptr) {
				return fixed[address];
			}
		} else if (address < 0x8000U) {
			auto const* selected = _cartridge->selectedRomData();
			if (selected != nullptr) {
				return selected[static_cast<std::size_t>(address - 0x4000U)];
			}
		}
	}
	return read8Fast(address);
}

std::uint16_t Bus::readProgram16(std::uint16_t address) {
	if (_cartridge != nullptr) {
		if (address < 0x3FFFU) {
			auto const* fixed = _cartridge->fixedRomData();
			if (fixed != nullptr) {
				return static_cast<std::uint16_t>(fixed[address] | (static_cast<std::uint16_t>(fixed[address + 1U]) << 8U));
			}
		} else if (address >= 0x4000U && address < 0x7FFFU) {
			auto const* selected = _cartridge->selectedRomData();
			if (selected != nullptr) {
				auto const offset = static_cast<std::size_t>(address - 0x4000U);
				return static_cast<std::uint16_t>(selected[offset] | (static_cast<std::uint16_t>(selected[offset + 1U]) << 8U));
			}
		}
	}
	auto const low = static_cast<std::uint16_t>(readProgram8(address));
	auto const high = static_cast<std::uint16_t>(readProgram8(static_cast<std::uint16_t>(address + 1U)));
	return static_cast<std::uint16_t>(low | (high << 8U));
}

std::uint8_t Bus::read8OrFF(std::uint16_t address) {
	return read8Fast(address);
}

void Bus::write8(std::uint16_t address, std::uint8_t value) {
	write8Fast(address, value);
}

void Bus::write8Fast(std::uint16_t address, std::uint8_t value) {
	switch (address >> 12U) {
		case 0x0:
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
			if (_cartridge != nullptr) {
				_cartridge->write8(address, value);
			}
			return;
		case 0x8:
		case 0x9:
			_vram[static_cast<std::size_t>(address - vramStart)] = value;
			++_vramWriteCount;
			return;
		case 0xA:
		case 0xB:
			if (_cartridge != nullptr) {
				_cartridge->writeExternalRam(address, value);
			}
			return;
		case 0xC:
		case 0xD:
			_wram[static_cast<std::size_t>(address - wramStart)] = value;
			return;
		case 0xE:
			_wram[static_cast<std::size_t>(address - echoRamStart)] = value;
			return;
		case 0xF:
			if (address < oamStart) {
				_wram[static_cast<std::size_t>(address - echoRamStart)] = value;
				return;
			}
			if (address < unusableStart) {
				_oam[static_cast<std::size_t>(address - oamStart)] = value;
				++_oamWriteCount;
				return;
			}
			if (address < ioStart) {
				return;
			}
			if (address < hramStart) {
				writeIo(address, value);
				return;
			}
			if (address < interruptEnableAddress) {
				_hram[static_cast<std::size_t>(address - hramStart)] = value;
				return;
			}
			_interruptEnable = value;
			return;
	}
}

std::uint16_t Bus::read16OrFFFF(std::uint16_t address) {
	auto const low = static_cast<std::uint16_t>(read8OrFF(address));
	auto const high = static_cast<std::uint16_t>(read8OrFF(static_cast<std::uint16_t>(address + 1U)));
	return static_cast<std::uint16_t>(low | (high << 8U));
}

void Bus::write16(std::uint16_t address, std::uint16_t value) {
	write8(address, static_cast<std::uint8_t>(value & 0xFFU));
	write8(static_cast<std::uint16_t>(address + 1U), static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void Bus::setJoypadState(JoypadState state) {
	auto const before = readJoypad();
	_joypad = state;
	auto const after = readJoypad();
	if ((before & 0x0FU) != (after & 0x0FU) && (before & ~after & 0x0FU) != 0) {
		requestInterrupt(interruptJoypad);
	}
}

JoypadState Bus::joypadState() const {
	return _joypad;
}

void Bus::advanceCycles(std::uint32_t cycles) {
	_apu.advanceCycles(cycles);
	updateTimer(cycles);
	updateSerialTransfer(cycles);
	updatePpuTiming(cycles);
}

void Bus::advanceCyclesFast(std::uint32_t cycles) {
	_apu.advanceCycles(cycles);
	updateTimer(cycles);
	updateSerialTransfer(cycles);

	auto const lcdc = _io[static_cast<std::size_t>(lcdControlAddress - ioStart)];
	if ((lcdc & lcdControlLcdEnable) == 0) {
		// Keep the common LCD-off state cheap. The full path also resets STAT mode,
		// but doing it on every CPU instruction is expensive and irrelevant for
		// the gbemu fast playback mode.
		_scanlineDots = 0;
		_io[static_cast<std::size_t>(lyAddress - ioStart)] = 0;
		return;
	}

	auto& ly = _io[static_cast<std::size_t>(lyAddress - ioStart)];
	bool lyChanged = false;
	_scanlineDots = static_cast<std::uint16_t>(_scanlineDots + cycles);
	while (_scanlineDots >= 456U) {
		_scanlineDots = static_cast<std::uint16_t>(_scanlineDots - 456U);
		ly = static_cast<std::uint8_t>(ly + 1U);
		if (ly == 144U) {
			requestInterrupt(interruptVBlank);
		} else if (ly >= 154U) {
			ly = 0;
		}
		lyChanged = true;
	}

	if (lyChanged) {
		updatePpuCoincidence(true);
	}
}

std::uint8_t Bus::interruptEnable() const {
	return static_cast<std::uint8_t>(_interruptEnable & 0x1FU);
}

std::uint8_t Bus::interruptFlags() const {
	return static_cast<std::uint8_t>(_io[static_cast<std::size_t>(interruptFlagAddress - ioStart)] & 0x1FU);
}

std::uint8_t Bus::pendingInterrupts() const {
	return static_cast<std::uint8_t>(_interruptEnable & _io[static_cast<std::size_t>(interruptFlagAddress - ioStart)] & 0x1FU);
}

void Bus::clearInterrupt(std::uint8_t mask) {
	auto& flag = _io[static_cast<std::size_t>(interruptFlagAddress - ioStart)];
	flag = static_cast<std::uint8_t>(0xE0U | ((flag & ~mask) & 0x1FU));
}

bool Bus::isEchoRamAddress(std::uint16_t address) {
	return address >= echoRamStart && address <= echoRamEnd;
}

std::size_t Bus::echoRamIndex(std::uint16_t address) {
	return static_cast<std::size_t>(address - echoRamStart);
}

std::uint8_t Bus::readIo(std::uint16_t address) const {
	switch (address) {
		case joypadAddress:
			return readJoypad();
		case dividerAddress:
			return static_cast<std::uint8_t>((_dividerCounter >> 8U) & 0xFFU);
		case serialControlAddress:
			return static_cast<std::uint8_t>(0x7EU | (_io[static_cast<std::size_t>(serialControlAddress - ioStart)] & 0x81U));
		case timerControlAddress:
			return static_cast<std::uint8_t>(0xF8U | (_io[static_cast<std::size_t>(timerControlAddress - ioStart)] & 0x07U));
		case interruptFlagAddress:
			return static_cast<std::uint8_t>(0xE0U | (_io[static_cast<std::size_t>(interruptFlagAddress - ioStart)] & 0x1FU));
		case lcdStatusAddress: {
			break;
		}
		default:
			break;
	}
	if (Apu::isAudioRegister(address)) {
		return _apu.readRegister(address);
	}
	switch (address) {
		case lcdStatusAddress: {
			auto const stat = _io[static_cast<std::size_t>(lcdStatusAddress - ioStart)];
			return static_cast<std::uint8_t>(0x80U | (stat & static_cast<std::uint8_t>(~statModeMask)) | currentPpuMode());
		}
		default:
			return _io[static_cast<std::size_t>(address - ioStart)];
	}
}

void Bus::writeIo(std::uint16_t address, std::uint8_t value) {
	auto& slot = _io[static_cast<std::size_t>(address - ioStart)];

	switch (address) {
		default: break;
	}

	if (Apu::isAudioRegister(address)) {
		_apu.writeRegister(address, value);
		slot = _apu.readRegister(address);
		return;
	}

	switch (address) {
		case joypadAddress:
			_joypadSelect = static_cast<std::uint8_t>(value & 0x30U);
			slot = readJoypad();
			return;
		case serialDataAddress:
			slot = value;
			return;
		case serialControlAddress:
			slot = static_cast<std::uint8_t>(0x7EU | (value & 0x81U));
			_serialTransferCycles = 0;
			if ((value & serialTransferStart) != 0 && (value & serialInternalClock) != 0) {
				_serialTransferByte = _io[static_cast<std::size_t>(serialDataAddress - ioStart)];
				_serialTransferCycles = serialInternalTransferCycles;
			}
			return;
		case dividerAddress:
			_dividerCounter = 0;
			slot = 0;
			return;
		case timerControlAddress:
			slot = static_cast<std::uint8_t>(0xF8U | (value & 0x07U));
			_timerCounter = 0;
			return;
		case interruptFlagAddress:
			slot = static_cast<std::uint8_t>(0xE0U | (value & 0x1FU));
			return;
		case lcdControlAddress: {
			auto const wasEnabled = (slot & lcdControlLcdEnable) != 0;
			slot = value;
			auto const isEnabled = (slot & lcdControlLcdEnable) != 0;
			if (!isEnabled) {
				resetPpuTiming();
			} else if (!wasEnabled) {
				_scanlineDots = 0;
				_io[static_cast<std::size_t>(lyAddress - ioStart)] = 0;
				updatePpuCoincidence(true);
				setPpuMode(ppuModeOam, true);
			}
			return;
		}
		case lcdStatusAddress:
			slot = static_cast<std::uint8_t>(0x80U | (value & statWritableMask) | (slot & (statModeMask | statCoincidenceFlag)));
			updatePpuCoincidence(true);
			return;
		case lyAddress:
			// LY is read-only from the CPU point of view in this simplified model.
			return;
		case lyCompareAddress:
			slot = value;
			updatePpuCoincidence(true);
			return;
		case dmaAddress: {
			slot = value;
			auto const sourceBase = static_cast<std::uint16_t>(static_cast<std::uint16_t>(value) << 8U);
			for (std::uint16_t offset = 0; offset < oamSize; ++offset) {
				_oam[static_cast<std::size_t>(offset)] = read8OrFF(static_cast<std::uint16_t>(sourceBase + offset));
			}
			_oamWriteCount += static_cast<std::uint32_t>(oamSize);
			return;
		}
		default:
			slot = value;
			return;
	}
}

std::uint8_t Bus::readJoypad() const {
	std::uint8_t low = 0x0F;

	if ((_joypadSelect & joypadSelectDirections) == 0) {
		if (_joypad.right) {
			low &= static_cast<std::uint8_t>(~0x01U);
		}
		if (_joypad.left) {
			low &= static_cast<std::uint8_t>(~0x02U);
		}
		if (_joypad.up) {
			low &= static_cast<std::uint8_t>(~0x04U);
		}
		if (_joypad.down) {
			low &= static_cast<std::uint8_t>(~0x08U);
		}
	}

	if ((_joypadSelect & joypadSelectButtons) == 0) {
		if (_joypad.a) {
			low &= static_cast<std::uint8_t>(~0x01U);
		}
		if (_joypad.b) {
			low &= static_cast<std::uint8_t>(~0x02U);
		}
		if (_joypad.select) {
			low &= static_cast<std::uint8_t>(~0x04U);
		}
		if (_joypad.start) {
			low &= static_cast<std::uint8_t>(~0x08U);
		}
	}

	return static_cast<std::uint8_t>(0xC0U | _joypadSelect | low);
}

void Bus::updateTimer(std::uint32_t cycles) {
	_dividerCounter = static_cast<std::uint16_t>(_dividerCounter + cycles);

	auto& tima = _io[static_cast<std::size_t>(timerCounterAddress - ioStart)];
	auto const tma = _io[static_cast<std::size_t>(timerModuloAddress - ioStart)];
	auto const tac = _io[static_cast<std::size_t>(timerControlAddress - ioStart)];
	if ((tac & 0x04U) == 0) {
		return;
	}

	_timerCounter = static_cast<std::uint16_t>(_timerCounter + cycles);
	auto const period = timerPeriod(tac);
	while (_timerCounter >= period) {
		_timerCounter = static_cast<std::uint16_t>(_timerCounter - period);
		if (tima == 0xFFU) {
			tima = tma;
			requestInterrupt(interruptTimer);
		} else {
			++tima;
		}
	}
}

void Bus::updateSerialTransfer(std::uint32_t cycles) {
	if (_serialTransferCycles == 0) {
		return;
	}

	if (cycles < _serialTransferCycles) {
		_serialTransferCycles = static_cast<std::uint16_t>(_serialTransferCycles - cycles);
		return;
	}

	_serialTransferCycles = 0;
	auto& control = _io[static_cast<std::size_t>(serialControlAddress - ioStart)];
	if ((control & serialTransferStart) == 0) {
		return;
	}

	appendSerialByte(_serialTransferByte);
	_io[static_cast<std::size_t>(serialDataAddress - ioStart)] = 0xFF;
	control = static_cast<std::uint8_t>(0x7EU | (control & serialInternalClock));
	requestInterrupt(interruptSerial);
}

void Bus::updatePpuCoincidence(bool allowStatInterrupt) {
	auto& stat = _io[static_cast<std::size_t>(lcdStatusAddress - ioStart)];
	auto const ly = _io[static_cast<std::size_t>(lyAddress - ioStart)];
	auto const lyc = _io[static_cast<std::size_t>(lyCompareAddress - ioStart)];
	auto const wasCoincident = (stat & statCoincidenceFlag) != 0;
	auto const isCoincident = ly == lyc;

	if (isCoincident) {
		stat = static_cast<std::uint8_t>(stat | statCoincidenceFlag);
	} else {
		stat = static_cast<std::uint8_t>(stat & static_cast<std::uint8_t>(~statCoincidenceFlag));
	}
	if (allowStatInterrupt && isCoincident && !wasCoincident && (stat & statLycInterruptEnable) != 0) {
		requestInterrupt(interruptLcdStat);
	}
}

std::uint8_t Bus::currentPpuMode() const {
	auto const lcdc = _io[static_cast<std::size_t>(lcdControlAddress - ioStart)];
	if ((lcdc & lcdControlLcdEnable) == 0) {
		return ppuModeHBlank;
	}

	auto const ly = _io[static_cast<std::size_t>(lyAddress - ioStart)];
	if (ly >= 144U) {
		return ppuModeVBlank;
	}
	if (_scanlineDots < 80U) {
		return ppuModeOam;
	}
	if (_scanlineDots < 252U) {
		return ppuModeTransfer;
	}
	return ppuModeHBlank;
}

void Bus::setPpuMode(std::uint8_t mode, bool allowStatInterrupt) {
	mode = static_cast<std::uint8_t>(mode & statModeMask);
	auto& stat = _io[static_cast<std::size_t>(lcdStatusAddress - ioStart)];
	auto const previousMode = static_cast<std::uint8_t>(stat & statModeMask);
	if (previousMode == mode) {
		return;
	}
	stat = static_cast<std::uint8_t>((stat & static_cast<std::uint8_t>(~statModeMask)) | mode);
	if (!allowStatInterrupt) {
		return;
	}
	std::uint8_t interruptEnable = 0;
	switch (mode) {
		case ppuModeHBlank:
			interruptEnable = statHBlankInterruptEnable;
			break;
		case ppuModeVBlank:
			interruptEnable = statVBlankInterruptEnable;
			break;
		case ppuModeOam:
			interruptEnable = statOamInterruptEnable;
			break;
		default:
			break;
	}
	if (interruptEnable != 0 && (stat & interruptEnable) != 0) {
		requestInterrupt(interruptLcdStat);
	}
}

void Bus::resetPpuTiming() {
	_scanlineDots = 0;
	_io[static_cast<std::size_t>(lyAddress - ioStart)] = 0;
	setPpuMode(ppuModeHBlank, false);
	updatePpuCoincidence(false);
}

void Bus::updatePpuTiming(std::uint32_t cycles) {
	auto const lcdc = _io[static_cast<std::size_t>(lcdControlAddress - ioStart)];
	if ((lcdc & lcdControlLcdEnable) == 0) {
		resetPpuTiming();
		return;
	}
	auto& ly = _io[static_cast<std::size_t>(lyAddress - ioStart)];

	bool lyChanged = false;
	_scanlineDots = static_cast<std::uint16_t>(_scanlineDots + cycles);
	while (_scanlineDots >= 456U) {
		_scanlineDots = static_cast<std::uint16_t>(_scanlineDots - 456U);
		ly = static_cast<std::uint8_t>(ly + 1U);
		if (ly == 144U) {
			requestInterrupt(interruptVBlank);
		} else if (ly >= 154U) {
			ly = 0;
		}
		lyChanged = true;
	}
	if (lyChanged) {
		updatePpuCoincidence(true);
	}

	setPpuMode(currentPpuMode(), true);
}

std::uint8_t Bus::vramByte(std::size_t offset) const {
	return offset < _vram.size() ? _vram[offset] : 0xFF;
}

std::uint8_t Bus::oamByte(std::size_t offset) const {
	return offset < _oam.size() ? _oam[offset] : 0xFF;
}

std::uint32_t Bus::vramWriteCount() const {
	return _vramWriteCount;
}

std::uint32_t Bus::oamWriteCount() const {
	return _oamWriteCount;
}

std::uint32_t Bus::nonZeroVramBytes() const {
	std::uint32_t count = 0;
	for (auto const byte : _vram) {
		if (byte != 0) {
			++count;
		}
	}
	return count;
}

std::uint32_t Bus::nonZeroOamBytes() const {
	std::uint32_t count = 0;
	for (auto const byte : _oam) {
		if (byte != 0) {
			++count;
		}
	}
	return count;
}

void Bus::clearSerialOutput() {
	_serialOutput = {};
	_serialOutputSize = 0;
	_serialOutputOverflowed = false;
}

std::size_t Bus::serialOutputSize() const {
	return _serialOutputSize;
}

std::uint8_t Bus::serialOutputByte(std::size_t index) const {
	return index < _serialOutputSize ? _serialOutput[index] : 0;
}

bool Bus::serialOutputOverflowed() const {
	return _serialOutputOverflowed;
}

void Bus::appendSerialByte(std::uint8_t value) {
	if (_serialOutputSize < _serialOutput.size()) {
		_serialOutput[_serialOutputSize++] = value;
		return;
	}
	_serialOutputOverflowed = true;
}

void Bus::requestInterrupt(std::uint8_t mask) {
	auto& flag = _io[static_cast<std::size_t>(interruptFlagAddress - ioStart)];
	flag = static_cast<std::uint8_t>(0xE0U | ((flag | mask) & 0x1FU));
}

void Bus::setAudioOutputEnabled(bool enabled) {
	_apu.setPcmOutputEnabled(enabled);
}

void Bus::clearAudioFrames() {
	_apu.clearPcmFrames();
}

std::size_t Bus::audioFrameCount() const {
	return _apu.audioFrameCount();
}

std::uint32_t Bus::droppedAudioFrames() const {
	return _apu.droppedAudioFrames();
}

std::size_t Bus::readAudioFrames(lexxos::devices::AudioPcmFrame* frames, std::size_t maxFrames) {
	return _apu.readAudioFrames(frames, maxFrames);
}

} // namespace lexxos::gameboy
