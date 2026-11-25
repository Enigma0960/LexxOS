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

std::uint32_t Rv32iVirtualMachine::handleGpioOperation(
    GuestDeviceSlot& slot, std::uint32_t operation,
    std::uint32_t flags, std::uint32_t arg0,
    std::uint32_t& result) {
	auto const& gpio = _context.platform().gpio;

	switch (operation) {
		case 1: // configure input
			if (!slot.inputAllowed) {
				return deviceDenied;
			}

			if (flags > 2U) {
				return deviceInvalid;
			}

			if (!gpio.configureInput) {
				return deviceUnsupported;
			}

			return gpio.configureInput(slot.pin,
			           static_cast<lexxos::platform::GpioPull>(flags))
			    ? 0U
			    : deviceIoError;

		case 2: // configure output
			if (!slot.outputAllowed) {
				return deviceDenied;
			}

			if (arg0 > 1U) {
				return deviceInvalid;
			}

			if (!gpio.configureOutput) {
				return deviceUnsupported;
			}

			return gpio.configureOutput(slot.pin, arg0 != 0)
			    ? 0U
			    : deviceIoError;

		case 3: { // read
			if (!slot.inputAllowed) {
				return deviceDenied;
			}

			if (!gpio.read) {
				return deviceUnsupported;
			}

			bool value = false;
			if (!gpio.read(slot.pin, value)) {
				return deviceIoError;
			}

			result = value ? 1U : 0U;
			return 0;
		}

		case 4: // write
			if (!slot.outputAllowed) {
				return deviceDenied;
			}

			if (arg0 > 1U) {
				return deviceInvalid;
			}

			if (!gpio.write) {
				return deviceUnsupported;
			}

			return gpio.write(slot.pin, arg0 != 0)
			    ? 0U
			    : deviceIoError;
		default:
			return deviceUnsupported;
	}
}

std::uint32_t Rv32iVirtualMachine::handleI2cOperation(
    GuestDeviceSlot& slot, std::uint32_t operation,
    std::uint32_t address, std::uint32_t buffer,
    std::uint32_t size, std::uint32_t& result) {
	if (address < 0x08U || address > 0x77U) {
		return deviceInvalid;
	}

	auto& bus = *static_cast<lexxos::devices::I2cBus*>(slot.instance);
	if (_context.devices().isI2cAddressReserved(bus,
	        static_cast<std::uint8_t>(address))) {
		return deviceDenied;
	}

	if (operation != 10 && !validDeviceBuffer(buffer, size)) {
		return deviceInvalid;
	}

	// The bus handle is shared. Only this bounded operation is queued.
	lexxos::devices::I2cBusTransaction transaction{
	    bus, lexxos::devices::DeviceResourcePriority::Normal, 50};
	if (!transaction) {
		return deviceBusy;
	}

	auto const target = static_cast<std::uint8_t>(address);
	switch (operation) {
		case 10: // probe
			result = bus.probe(target) ? 1U : 0U;
			return 0;
		case 11: // write
			return bus.write(target, _memory + buffer, size)
			    ? 0U
			    : deviceIoError;
		case 12: // read
			return bus.read(target, _memory + buffer, size)
			    ? 0U
			    : deviceIoError;
		default:
			return deviceUnsupported;
	}
}

std::uint32_t Rv32iVirtualMachine::handleSpiOperation(
    GuestDeviceSlot& slot, std::uint32_t operation,
    std::uint32_t mode, std::uint32_t tx,
    std::uint32_t rx, std::uint32_t size,
    std::uint32_t baud) {
	if (operation != 20) {
		return deviceUnsupported;
	}

	if (mode > 3U || size == 0 || size > lexxGuestDeviceMaxTransfer || (tx == 0 && rx == 0) || (tx != 0 && !rangeValid(tx, size)) || (rx != 0 && !rangeValid(rx, size))) {
		return deviceInvalid;
	}

	auto& bus = *static_cast<lexxos::devices::SpiBus*>(
	    slot.instance);
	auto const status = lexxos::devices::queuedSpiTransfer(bus,
	    static_cast<lexxos::devices::SpiClockMode>(mode), baud,
	    tx != 0 ? _memory + tx : nullptr, rx != 0 ? _memory + rx : nullptr, size);
	switch (status) {
		case lexxos::devices::SpiTransferStatus::Ok  : return 0;
		case lexxos::devices::SpiTransferStatus::Busy: return deviceBusy;
		default                                      : return deviceIoError;
	}
}

std::uint32_t Rv32iVirtualMachine::handleUartOperation(
    GuestDeviceSlot& slot, std::uint32_t operation,
    std::uint32_t arg0, std::uint32_t arg1,
    std::uint32_t& result) {
	auto& bus = *static_cast<lexxos::devices::UartBus*>(
	    slot.instance);
	if (operation == 30) {
		if (arg0 == 0) {
			return deviceInvalid;
		}

		return bus.setBaudRate(arg0)
		    ? 0U
		    : deviceIoError;
	}

	if (operation != 31 && operation != 32) {
		return deviceUnsupported;
	}

	if (!validDeviceBuffer(arg0, arg1)) {
		return deviceInvalid;
	}

	std::size_t transferred = 0;
	bool const success = operation == 31
	    ? bus.write(_memory + arg0, arg1, transferred)
	    : bus.read(_memory + arg0, arg1, transferred);

	if (!success || transferred > arg1) {
		return deviceIoError;
	}

	result = static_cast<std::uint32_t>(transferred);
	return 0;
}

void Rv32iVirtualMachine::handleDeviceControlSyscall() {
	auto const handle = _registers[10];
	auto const address = _registers[11];
	_registers[10] = deviceInvalid;

	if (!hasCapability(LexxCapabilityDevices)) {
		_registers[10] = deviceDenied;
		return;
	}

	auto* slot = guestDevice(handle);
	if (!slot || !rangeValid(address, lexxGuestDeviceRequestSize)) {
		return;
	}

	auto const* request = _memory + address;
	auto const operation = readLe32(request);
	auto const flags = readLe32(request + 4U);
	auto const arg0 = readLe32(request + 8U);
	auto const arg1 = readLe32(request + 12U);
	auto const arg2 = readLe32(request + 16U);
	auto const arg3 = readLe32(request + 20U);

	std::uint32_t result = 0;
	std::uint32_t status = deviceUnsupported;

	switch (slot->kind) {
		case 1:
			status = handleGpioOperation(
			    *slot, operation, flags, arg0, result);
			break;
		case 2:
			status = handleI2cOperation(
			    *slot, operation, arg0, arg1, arg2, result);
			break;
		case 3:
			status = handleSpiOperation(
			    *slot, operation, flags, arg0, arg1,
			    arg2, arg3);
			break;
		case 4:
			status = handleUartOperation(
			    *slot, operation, arg0, arg1, result);
			break;
	}

	writeLe32(_memory + address + 24U, result);
	_registers[10] = status;
}

Rv32iVirtualMachine::GuestDeviceSlot* Rv32iVirtualMachine::guestDevice(std::uint32_t handle) {
	if (handle < lexxGuestDeviceHandleBase) {
		return nullptr;
	}

	auto const index = handle - lexxGuestDeviceHandleBase;
	if (index >= _guestDevices.size()) {
		return nullptr;
	}

	auto& slot = _guestDevices[index];
	return slot.kind != 0 ? &slot : nullptr;
}

bool Rv32iVirtualMachine::validDeviceBuffer(std::uint32_t address,
    std::uint32_t size) const {
	return address != 0 && size > 0 && size <= lexxGuestDeviceMaxTransfer && rangeValid(address, size);
}

void Rv32iVirtualMachine::handleDeviceInfoSyscall() {
	auto const kind = _registers[10];
	auto const index = _registers[11];
	auto const address = _registers[12];

	_registers[10] = deviceInvalid;
	if (!hasCapability(LexxCapabilityDevices)) {
		_registers[10] = deviceDenied;
		return;
	}

	if (!rangeValid(address, lexxGuestDeviceInfoSize)) {
		return;
	}

	auto& devices = _context.devices();
	std::string_view name{};
	std::uint32_t flags = 0;
	bool busy = false;
	bool ready = false;

	switch (kind) {
		case 1: {
			if (index >= devices.gpioPinCount()) {
				_registers[10] = deviceNotFound;
				return;
			}

			auto const& entry = devices.gpioPinAt(index);
			name = entry.name;
			busy = devices.gpioPinStatusAt(index).locked;
			auto const& gpio = _context.platform().gpio;
			ready = gpio.configureInput != nullptr
			    || gpio.configureOutput != nullptr;
			if (entry.inputAllowed) {
				flags |= 4U;
			}

			if (entry.outputAllowed) {
				flags |= 8U;
			}

			break;
		}
		case 2: {
			if (index >= devices.i2cBusCount()) {
				_registers[10] = deviceNotFound;
				return;
			}

			auto const& entry = devices.i2cBusAt(index);
			name = entry.name;
			ready = entry.bus && entry.bus->isReady();
			busy = devices.i2cBusStatusAt(index).locked
			    || (entry.bus && entry.bus->transactionBusy());
			break;
		}
		case 3: {
			if (index >= devices.spiBusCount()) {
				_registers[10] = deviceNotFound;
				return;
			}

			auto const& entry = devices.spiBusAt(index);
			name = entry.name;
			ready = entry.bus && entry.bus->isReady();
			busy = devices.spiBusStatusAt(index).locked
			    || (entry.bus && entry.bus->transactionBusy());
			break;
		}
		case 4: {
			if (index >= devices.uartBusCount()) {
				_registers[10] = deviceNotFound;
				return;
			}

			auto const& entry = devices.uartBusAt(index);
			name = entry.name;
			ready = entry.bus && entry.bus->isReady();
			busy = devices.uartBusStatusAt(index).locked;
			break;
		}
		default:
			return;
	}

	if (ready) {
		flags |= 1U;
	}

	if (busy) {
		flags |= 2U;
	}

	auto* output = _memory + address;
	std::memset(output, 0, lexxGuestDeviceInfoSize);
	writeLe32(output, kind);
	writeLe32(output + 4U, flags);

	auto const length = std::min<std::size_t>(name.size(), 31);
	std::memcpy(output + 8U, name.data(), length);
	_registers[10] = 0;
}

void Rv32iVirtualMachine::handleDeviceOpenSyscall() {
	auto const kind = _registers[10];
	auto const guestName = _registers[11];
	_registers[10] = deviceInvalid;

	if (!hasCapability(LexxCapabilityDevices)) {
		_registers[10] = deviceDenied;
		return;
	}

	std::array<char, lexxos::vfs::maxPathLength> name{};
	if (!loadGuestString(guestName, name)) {
		return;
	}

	std::size_t freeSlot = _guestDevices.size();
	for (std::size_t i = 0; i < _guestDevices.size(); ++i) {
		if (_guestDevices[i].kind == 0) {
			freeSlot = i;
			break;
		}
	}

	if (freeSlot == _guestDevices.size()) {
		_registers[10] = deviceNoHandles;
		return;
	}

	auto& devices = _context.devices();
	constexpr auto priority =
	    lexxos::devices::DeviceResourcePriority::Normal;
	constexpr auto owner = "lexx-app";

	lexxos::devices::DeviceResourceLock lock{};
	void* instance = nullptr;
	lexxos::platform::GpioPin pin{};
	bool inputAllowed = false;
	bool outputAllowed = false;

	switch (kind) {
		case 1: {
			auto const* entry = devices.gpioPin(name.data());
			if (!entry) {
				_registers[10] = deviceNotFound;
				return;
			}

			pin = entry->pin;
			inputAllowed = entry->inputAllowed;
			outputAllowed = entry->outputAllowed;
			lock = devices.lockGpioPin(name.data(), priority, owner);
			break;
		}
		case 2: {
			auto* bus = devices.i2cBus(name.data());
			if (!bus) {
				_registers[10] = deviceNotFound;
				return;
			}

			// I2C devices share one controller; address-level access is
			// validated per transaction instead of locking the whole bus.
			instance = bus;
			break;
		}
		case 3: {
			auto* bus = devices.spiBus(name.data());
			if (!bus) {
				_registers[10] = deviceNotFound;
				return;
			}

			// SPI handle разделяется; вся операция с CS проходит через очередь.
			instance = bus;
			break;
		}
		case 4: {
			auto* bus = devices.uartBus(name.data());
			if (!bus) {
				_registers[10] = deviceNotFound;
				return;
			}

			lock = devices.lockUartBus(name.data(), priority, owner);
			instance = bus;
			break;
		}
		default:
			return;
	}

	if (!lock && kind != 2U && kind != 3U) {
		_registers[10] = deviceBusy;
		return;
	}

	bool ready = true;
	switch (kind) {
		case 1:
			ready = _context.platform().gpio.configureInput
			    || _context.platform().gpio.configureOutput;
			break;
		case 2:
			ready = static_cast<lexxos::devices::I2cBus*>(
			    instance)
			            ->isReady();
			break;
		case 3:
			ready = static_cast<lexxos::devices::SpiBus*>(
			    instance)
			            ->isReady();
			break;
		case 4:
			ready = static_cast<lexxos::devices::UartBus*>(
			    instance)
			            ->isReady();
			break;
	}

	if (!ready) {
		_registers[10] = deviceNotReady;
		return;
	}

	auto& slot = _guestDevices[freeSlot];
	slot.lock = std::move(lock);
	slot.instance = instance;
	slot.pin = pin;
	slot.inputAllowed = inputAllowed;
	slot.outputAllowed = outputAllowed;
	slot.kind = kind;

	_registers[10] = lexxGuestDeviceHandleBase
	    + static_cast<std::uint32_t>(freeSlot);
}

void Rv32iVirtualMachine::handleDeviceCloseSyscall() {
	auto* slot = guestDevice(_registers[10]);
	_registers[10] = deviceInvalid;
	if (!slot) {
		return;
	}

	slot->lock.release();
	slot->kind = 0;
	slot->instance = nullptr;
	slot->pin = {};
	slot->inputAllowed = false;
	slot->outputAllowed = false;
	_registers[10] = 0;
}

void Rv32iVirtualMachine::handlePlayToneSyscall() {
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

} // namespace lexxos::external_program::detail
