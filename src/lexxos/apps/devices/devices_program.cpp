#include <lexxos/apps/devices/devices_program.h>

#include <cstddef>
#include <string_view>

#include <lexxos/apps/common/pretty_table.h>
#include <lexxos/devices/audio_device.h>
#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/device.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/devices/gamepad_device.h>
#include <lexxos/devices/infrared_transmitter.h>
#include <lexxos/devices/input_device.h>
#include <lexxos/devices/microphone_device.h>
#include <lexxos/devices/picocalc/picocalc_southbridge_runtime.h>
#include <lexxos/devices/pointing_device.h>
#include <lexxos/devices/sensors/sensor.h>
#include <lexxos/devices/storage/block_device.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::device_programs {
namespace {

	struct DeviceTableContext {
		lexxos::os::AppContext& appContext;
		lexxos::devices::DeviceManager& devices;
	};

	constexpr lexxos::apps::PrettyTableColumn deviceColumns[] = {
	    {       .name = "name", .align = lexxos::apps::PrettyTableAlign::Left,  .minWidth = 4, .maxWidth = 16},
	    {       .name = "type", .align = lexxos::apps::PrettyTableAlign::Left,  .minWidth = 4, .maxWidth = 12},
	    {      .name = "state", .align = lexxos::apps::PrettyTableAlign::Left,  .minWidth = 5,  .maxWidth = 9},
	    {       .name = "lock", .align = lexxos::apps::PrettyTableAlign::Left,  .minWidth = 4,  .maxWidth = 4},
	    {   .name = "priority", .align = lexxos::apps::PrettyTableAlign::Left,  .minWidth = 8, .maxWidth = 11},
	    {      .name = "owner", .align = lexxos::apps::PrettyTableAlign::Left,  .minWidth = 5, .maxWidth = 16},
	    {.name = "description", .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 11, .maxWidth = 36},
	};

	void setDeviceRow(
	    lexxos::apps::PrettyTableRow& row,
	    std::string_view name,
	    lexxos::devices::DeviceKind kind,
	    bool ready,
	    lexxos::devices::DeviceResourceStatus resource,
	    std::string_view description) {
		row.set(0, name);
		row.set(1, lexxos::devices::toString(kind));
		row.set(2, ready ? "ready" : "not-ready");
		row.set(3, resource.locked ? "busy" : "free");
		row.set(4, lexxos::devices::toString(resource.priority));
		row.set(5, resource.owner.empty() ? "-" : resource.owner);
		row.set(6, description);
	}

	bool deviceTableRow(std::size_t rowIndex, lexxos::apps::PrettyTableRow& row, void* userData) {
		auto& table = *static_cast<DeviceTableContext*>(userData);
		auto& devices = table.devices;

		if (rowIndex < devices.i2cBusCount()) {
			auto const& descriptor = devices.i2cBusAt(rowIndex);
			setDeviceRow(row, descriptor.name, lexxos::devices::DeviceKind::I2cBus,
			    descriptor.bus != nullptr && descriptor.bus->isReady(), devices.i2cBusStatusAt(rowIndex), descriptor.description);
			return true;
		}
		rowIndex -= devices.i2cBusCount();

		if (rowIndex < devices.blockDeviceCount()) {
			auto const& descriptor = devices.blockDeviceAt(rowIndex);
			setDeviceRow(row, descriptor.name, lexxos::devices::DeviceKind::BlockDevice,
			    descriptor.device != nullptr && descriptor.device->isReady(), devices.blockDeviceStatusAt(rowIndex), descriptor.description);
			return true;
		}
		rowIndex -= devices.blockDeviceCount();

		if (rowIndex < devices.displayDeviceCount()) {
			auto const& descriptor = devices.displayDeviceAt(rowIndex);
			setDeviceRow(row, descriptor.name, lexxos::devices::DeviceKind::Display,
			    descriptor.device != nullptr && descriptor.device->isReady(), devices.displayDeviceStatusAt(rowIndex), descriptor.description);
			return true;
		}
		rowIndex -= devices.displayDeviceCount();

		if (rowIndex < devices.inputDeviceCount()) {
			auto const& descriptor = devices.inputDeviceAt(rowIndex);
			setDeviceRow(row, descriptor.name, lexxos::devices::DeviceKind::Input,
			    descriptor.device != nullptr && descriptor.device->isReady(), devices.inputDeviceStatusAt(rowIndex), descriptor.description);
			return true;
		}
		rowIndex -= devices.inputDeviceCount();

		if (rowIndex < devices.pointingDeviceCount()) {
			auto const& descriptor = devices.pointingDeviceAt(rowIndex);
			setDeviceRow(row, descriptor.name, lexxos::devices::DeviceKind::Pointing,
			    descriptor.device != nullptr && descriptor.device->isReady(), devices.pointingDeviceStatusAt(rowIndex), descriptor.description);
			return true;
		}
		rowIndex -= devices.pointingDeviceCount();

		if (rowIndex < devices.gamepadDeviceCount()) {
			auto const& descriptor = devices.gamepadDeviceAt(rowIndex);
			setDeviceRow(row, descriptor.name, lexxos::devices::DeviceKind::Gamepad,
			    descriptor.device != nullptr && descriptor.device->isReady(), devices.gamepadDeviceStatusAt(rowIndex), descriptor.description);
			return true;
		}
		rowIndex -= devices.gamepadDeviceCount();

		if (rowIndex < devices.sensorDeviceCount()) {
			auto const& descriptor = devices.sensorDeviceAt(rowIndex);
			setDeviceRow(row, descriptor.name, lexxos::devices::DeviceKind::Sensor,
			    descriptor.device != nullptr && descriptor.device->isReady(), devices.sensorDeviceStatusAt(rowIndex), descriptor.description);
			return true;
		}
		rowIndex -= devices.sensorDeviceCount();

		if (rowIndex < devices.infraredDeviceCount()) {
			auto const& descriptor = devices.infraredDeviceAt(rowIndex);
			setDeviceRow(row, descriptor.name, lexxos::devices::DeviceKind::Infrared,
			    descriptor.device != nullptr && descriptor.device->isReady(), devices.infraredDeviceStatusAt(rowIndex), descriptor.description);
			return true;
		}
		rowIndex -= devices.infraredDeviceCount();

		auto const audioCount = devices.audioDeviceCount(table.appContext.platform());
		if (rowIndex < audioCount) {
			auto const descriptor = devices.audioDeviceAt(table.appContext.platform(), rowIndex);
			setDeviceRow(row, descriptor.name, lexxos::devices::DeviceKind::Audio,
			    descriptor.device != nullptr && descriptor.device->isReady(),
			    devices.audioDeviceStatusAt(table.appContext.platform(), rowIndex),
			    descriptor.description);
			return true;
		}
		rowIndex -= audioCount;

		auto const microphoneCount = devices.microphoneDeviceCount(table.appContext.platform());
		if (rowIndex < microphoneCount) {
			auto const descriptor = devices.microphoneDeviceAt(table.appContext.platform(), rowIndex);
			setDeviceRow(row, descriptor.name, lexxos::devices::DeviceKind::Microphone,
			    descriptor.device != nullptr && descriptor.device->isReady(),
			    devices.microphoneDeviceStatusAt(table.appContext.platform(), rowIndex),
			    descriptor.description);
			return true;
		}
		rowIndex -= microphoneCount;

		if (rowIndex < devices.picoCalcSouthbridgeServiceCount()) {
			auto const& descriptor = devices.picoCalcSouthbridgeServiceAt(rowIndex);
			setDeviceRow(row, descriptor.name, lexxos::devices::DeviceKind::Service,
			    descriptor.service != nullptr && descriptor.service->isReady(),
			    devices.picoCalcSouthbridgeServiceStatusAt(rowIndex),
			    descriptor.description);
			return true;
		}

		return false;
	}

} // namespace

lexxos::program::ProgramDescriptor DevicesProgram::descriptor() {
	return {
	    .name = "devices",
	    .help = "list registered devices",
	    .description = "Lists I2C, block, display, keyboard, pointer, gamepad, sensor, IR, audio and service entries registered in DeviceManager.",
	    .usage = "devices",
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int DevicesProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	auto& devices = context.devices();
	DeviceTableContext tableContext{.appContext = context, .devices = devices};
	lexxos::apps::PrettyTable table{deviceColumns, sizeof(deviceColumns) / sizeof(deviceColumns[0])};
	table.print(context, deviceTableRow, &tableContext);
	return 0;
}

} // namespace lexxos::apps::device_programs
