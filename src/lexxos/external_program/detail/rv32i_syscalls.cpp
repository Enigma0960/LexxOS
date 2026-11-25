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
#include <lexxos/external_program/detail/rv32i_virtual_machine.h>

namespace lexxos::external_program::detail {

void Rv32iVirtualMachine::handleEcall(std::uint32_t nextPc) {
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
		case VmSyscall::DeviceInfo:
			handleDeviceInfoSyscall();
			_pc = nextPc;
			return;
		case VmSyscall::DeviceOpen:
			handleDeviceOpenSyscall();
			_pc = nextPc;
			return;
		case VmSyscall::DeviceClose:
			handleDeviceCloseSyscall();
			_pc = nextPc;
			return;
		case VmSyscall::DeviceControl:
			handleDeviceControlSyscall();
			_pc = nextPc;
			return;
		case VmSyscall::Graphics:
			handleGraphicsSyscall();
			_pc = nextPc;
			return;
		case VmSyscall::UnixTime:
			handleUnixTimeSyscall();
			_pc = nextPc;
			return;
		case VmSyscall::MemoryInfo:
			handleMemoryInfoSyscall();
			_pc = nextPc;
			return;
		case VmSyscall::SleepMs:
			lexxos::kernel::sleepMs(std::min(_registers[10], static_cast<std::uint32_t>(100U)));
			_registers[10] = 0U;
			_pc = nextPc;
			return;
		case VmSyscall::InputMode: {
			auto const request = _registers[10];
			_registers[10] = lexxSyscallError;
			if (hasCapability(LexxCapabilityKeyboard)) {
				if (request == 2U) {
					_registers[10] = static_cast<std::uint32_t>(_inputMode);
				} else if (request <= 1U) {
					auto const nextMode = static_cast<lexxos::app::program_input::InputMode>(request);
					if (_context.processId() == 0U
					    || _context.programInput().setInputMode(_context.processId(), nextMode)) {
						_inputMode = nextMode;
						if (_context.hasProgramFocus()) {
							_context.consoleInput().clearInjected();
						}

						_registers[10] = 0U;
					}
				}
			}

			_pc = nextPc;
			return;
		}
		default: return; // Add error
	}

	_unsupportedSyscall = syscall;
	fault(ExternalProgramStatus::UnsupportedSyscall, _pc, 0x00000073U);
}

void Rv32iVirtualMachine::handleUnixTimeSyscall() {
	auto const address = _registers[10];
	_registers[10] = deviceDenied;
	if (!hasCapability(LexxCapabilityClock)) {
		return;
	}

	_registers[10] = deviceInvalid;
	if (!rangeValid(address, 4U)) {
		return;
	}

	// Prefer configured software time; on PicoCalc use the JCS RTC,
	// then fall back to a valid non-preferred software clock.
	auto const& platform = _context.platform();

	std::uint32_t now = 0;
	bool ready = false;

	if (lexxos::devices::rtc::software::preferredAsDefault(platform)) {
		ready = lexxos::devices::rtc::software::readUnixSeconds(platform, now);
	}

#if FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
	if (!ready) {
		auto& devices = _context.devices();
		if (auto* service = lexxos::devices::PicoCalcSouthbridgeResourcePolicy::readyService(devices)) {
			auto lock = lexxos::devices::PicoCalcSouthbridgeResourcePolicy::lock(
			    devices, "lexx-clock", lexxos::devices::DeviceResourcePriority::Interactive);
			if (lock) {
				lexxos::devices::rtc::DateTime date{};
				ready = service->readJcsRtcDateTime(date)
				    && lexxos::devices::rtc::dateTimeToUnixSeconds(date, now);
			}
		}
	}
#endif

	if (!ready) {
		ready = lexxos::devices::rtc::software::readUnixSeconds(platform, now);
	}

	_registers[10] = deviceNotReady;
	if (ready) {
		writeLe32(_memory + address, now);
		_registers[10] = 0U;
	}
}

void Rv32iVirtualMachine::handleMemoryInfoSyscall() {
	auto const address = _registers[10];
	_registers[10] = deviceInvalid;
	if (!rangeValid(address, 8U)) {
		return;
	}

	writeLe32(_memory + address, _memorySize);
	writeLe32(_memory + address + 4U, _header.stackSize);
	_registers[10] = 0U;
}

} // namespace lexxos::external_program::detail
