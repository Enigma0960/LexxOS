#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <lexxos/devices/audio_device.h>
#include <lexxos/gameboy/apu.h>
#include <lexxos/gameboy/bus_types.h>
#include <lexxos/gameboy/cartridge.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::gameboy {

class Bus final {
public:
	static constexpr std::uint16_t romStart = 0x0000;
	static constexpr std::uint16_t romEnd = 0x7FFF;
	static constexpr std::uint16_t vramStart = 0x8000;
	static constexpr std::uint16_t vramEnd = 0x9FFF;
	static constexpr std::uint16_t externalRamStart = 0xA000;
	static constexpr std::uint16_t externalRamEnd = 0xBFFF;
	static constexpr std::uint16_t wramStart = 0xC000;
	static constexpr std::uint16_t wramEnd = 0xDFFF;
	static constexpr std::uint16_t echoRamStart = 0xE000;
	static constexpr std::uint16_t echoRamEnd = 0xFDFF;
	static constexpr std::uint16_t oamStart = 0xFE00;
	static constexpr std::uint16_t oamEnd = 0xFE9F;
	static constexpr std::uint16_t unusableStart = 0xFEA0;
	static constexpr std::uint16_t unusableEnd = 0xFEFF;
	static constexpr std::uint16_t ioStart = 0xFF00;
	static constexpr std::uint16_t ioEnd = 0xFF7F;
	static constexpr std::uint16_t hramStart = 0xFF80;
	static constexpr std::uint16_t hramEnd = 0xFFFE;
	static constexpr std::uint16_t interruptEnableAddress = 0xFFFF;

	static constexpr std::size_t vramSize = 8U * 1024U;
	static constexpr std::size_t wramSize = 8U * 1024U;
	static constexpr std::size_t echoRamSize = 0x1E00U;
	static constexpr std::size_t oamSize = 0xA0U;
	static constexpr std::size_t ioSize = 0x80U;
	static constexpr std::size_t hramSize = 0x7FU;
	static constexpr std::size_t serialOutputCapacity = 256U;

	Bus() = default;
	Bus(Bus const&) = delete;
	Bus& operator=(Bus const&) = delete;

	void reset();
	void attachCartridge(Cartridge* cartridge);
	[[nodiscard]] Cartridge* cartridge();
	[[nodiscard]] Cartridge const* cartridge() const;

	[[nodiscard]] lexxos::vfs::ReadStatus read8(std::uint16_t address, std::uint8_t& value);
	[[nodiscard]] std::uint8_t read8Fast(std::uint16_t address);
	[[nodiscard]] std::uint8_t read8OrFF(std::uint16_t address);
	[[nodiscard]] std::uint8_t readProgram8(std::uint16_t address);
	[[nodiscard]] std::uint16_t readProgram16(std::uint16_t address);
	void write8(std::uint16_t address, std::uint8_t value);
	void write8Fast(std::uint16_t address, std::uint8_t value);

	[[nodiscard]] std::uint16_t read16OrFFFF(std::uint16_t address);
	void write16(std::uint16_t address, std::uint16_t value);

	void setJoypadState(JoypadState state);
	[[nodiscard]] JoypadState joypadState() const;
	void advanceCycles(std::uint32_t cycles);
	void advanceCyclesFast(std::uint32_t cycles);

	[[nodiscard]] std::uint8_t interruptEnable() const;
	[[nodiscard]] std::uint8_t interruptFlags() const;
	[[nodiscard]] std::uint8_t pendingInterrupts() const;
	void requestInterrupt(std::uint8_t mask);
	void clearInterrupt(std::uint8_t mask);

	[[nodiscard]] std::uint8_t vramByte(std::size_t offset) const;
	[[nodiscard]] std::uint8_t oamByte(std::size_t offset) const;
	[[nodiscard]] std::uint32_t vramWriteCount() const;
	[[nodiscard]] std::uint32_t oamWriteCount() const;
	[[nodiscard]] std::uint32_t nonZeroVramBytes() const;
	[[nodiscard]] std::uint32_t nonZeroOamBytes() const;

	void clearSerialOutput();
	[[nodiscard]] std::size_t serialOutputSize() const;
	[[nodiscard]] std::uint8_t serialOutputByte(std::size_t index) const;
	[[nodiscard]] bool serialOutputOverflowed() const;

	void setAudioOutputEnabled(bool enabled);
	void clearAudioFrames();
	[[nodiscard]] std::size_t audioFrameCount() const;
	[[nodiscard]] std::uint32_t droppedAudioFrames() const;
	[[nodiscard]] std::size_t readAudioFrames(lexxos::devices::AudioPcmFrame* frames, std::size_t maxFrames);

private:
	[[nodiscard]] static bool isEchoRamAddress(std::uint16_t address);
	[[nodiscard]] static std::size_t echoRamIndex(std::uint16_t address);
	[[nodiscard]] std::uint8_t readIo(std::uint16_t address) const;
	void writeIo(std::uint16_t address, std::uint8_t value);
	[[nodiscard]] std::uint8_t readJoypad() const;
	void updateTimer(std::uint32_t cycles);
	void updateSerialTransfer(std::uint32_t cycles);
	void updatePpuTiming(std::uint32_t cycles);
	[[nodiscard]] std::uint8_t currentPpuMode() const;
	void updatePpuCoincidence(bool allowStatInterrupt);
	void setPpuMode(std::uint8_t mode, bool allowStatInterrupt);
	void resetPpuTiming();
	void appendSerialByte(std::uint8_t value);

	Cartridge* _cartridge{nullptr};
	JoypadState _joypad{};
	Apu _apu{};

	std::array<std::uint8_t, vramSize> _vram{};
	std::array<std::uint8_t, wramSize> _wram{};
	std::array<std::uint8_t, oamSize> _oam{};
	std::array<std::uint8_t, ioSize> _io{};
	std::array<std::uint8_t, hramSize> _hram{};
	std::uint8_t _interruptEnable{0};
	std::uint8_t _joypadSelect{0x30};
	std::uint16_t _dividerCounter{0};
	std::uint16_t _timerCounter{0};
	std::uint16_t _serialTransferCycles{0};
	std::uint8_t _serialTransferByte{0};
	std::uint16_t _scanlineDots{0};
	std::uint32_t _vramWriteCount{0};
	std::uint32_t _oamWriteCount{0};
	std::array<std::uint8_t, serialOutputCapacity> _serialOutput{};
	std::size_t _serialOutputSize{0};
	bool _serialOutputOverflowed{false};
};

} // namespace lexxos::gameboy
