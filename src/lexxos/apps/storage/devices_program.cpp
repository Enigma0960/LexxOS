#include <lexxos/apps/storage/devices_program.h>

#include <cstddef>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/common/pretty_table.h>
#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/storage/block_device.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::storage_programs {
namespace {

	constexpr lexxos::apps::PrettyTableColumn i2cColumns[] = {
	    {       .name = "name", .align = lexxos::apps::PrettyTableAlign::Left,  .minWidth = 4, .maxWidth = 16},
	    {      .name = "state", .align = lexxos::apps::PrettyTableAlign::Left,  .minWidth = 5,  .maxWidth = 9},
	    {       .name = "lock", .align = lexxos::apps::PrettyTableAlign::Left,  .minWidth = 4,  .maxWidth = 4},
	    {   .name = "priority", .align = lexxos::apps::PrettyTableAlign::Left,  .minWidth = 8, .maxWidth = 11},
	    {      .name = "owner", .align = lexxos::apps::PrettyTableAlign::Left,  .minWidth = 5, .maxWidth = 16},
	    {.name = "description", .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 11, .maxWidth = 36},
	};

	constexpr lexxos::apps::PrettyTableColumn blockColumns[] = {
	    {       .name = "name",  .align = lexxos::apps::PrettyTableAlign::Left,  .minWidth = 4, .maxWidth = 16},
	    {      .name = "state",  .align = lexxos::apps::PrettyTableAlign::Left,  .minWidth = 5,  .maxWidth = 9},
	    {       .name = "lock",  .align = lexxos::apps::PrettyTableAlign::Left,  .minWidth = 4,  .maxWidth = 4},
	    {   .name = "priority",  .align = lexxos::apps::PrettyTableAlign::Left,  .minWidth = 8, .maxWidth = 11},
	    {      .name = "owner",  .align = lexxos::apps::PrettyTableAlign::Left,  .minWidth = 5, .maxWidth = 16},
	    { .name = "block-size", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 10, .maxWidth = 10},
	    {.name = "block-count", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 11, .maxWidth = 14},
	    {.name = "description",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 11, .maxWidth = 32},
	};

	bool i2cTableRow(std::size_t rowIndex, lexxos::apps::PrettyTableRow& row, void* userData) {
		auto& devices = *static_cast<lexxos::devices::DeviceManager*>(userData);
		if (rowIndex >= devices.i2cBusCount()) {
			return false;
		}

		auto const& descriptor = devices.i2cBusAt(rowIndex);
		auto const status = devices.i2cBusStatusAt(rowIndex);
		row.set(0, descriptor.name);
		row.set(1, descriptor.bus != nullptr && descriptor.bus->isReady() ? "ready" : "not-ready");
		row.set(2, status.locked ? "busy" : "free");
		row.set(3, lexxos::devices::toString(status.priority));
		row.set(4, status.owner.empty() ? "-" : status.owner);
		row.set(5, descriptor.description);
		return true;
	}

	bool blockTableRow(std::size_t rowIndex, lexxos::apps::PrettyTableRow& row, void* userData) {
		auto& devices = *static_cast<lexxos::devices::DeviceManager*>(userData);
		if (rowIndex >= devices.blockDeviceCount()) {
			return false;
		}

		auto const& descriptor = devices.blockDeviceAt(rowIndex);
		auto const* device = descriptor.device;
		auto const status = devices.blockDeviceStatusAt(rowIndex);
		row.set(0, descriptor.name);
		row.set(1, device != nullptr && device->isReady() ? "ready" : "not-ready");
		row.set(2, status.locked ? "busy" : "free");
		row.set(3, lexxos::devices::toString(status.priority));
		row.set(4, status.owner.empty() ? "-" : status.owner);
		row.setUnsigned(5, device != nullptr ? device->blockSizeBytes() : 0U);
		row.setUnsigned64(6, device != nullptr ? device->blockCount() : 0ULL);
		row.set(7, descriptor.description);
		return true;
	}

} // namespace

lexxos::program::ProgramDescriptor DevicesProgram::descriptor() {
	return {
	    .name = "devs",
	    .help = "show registered devices",
	    .description = "Lists devices registered in DeviceManager, including I2C buses and block devices.",
	    .usage = "devs",
	    .handler = run,
	};
}

int DevicesProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	auto& devices = context.devices();

	// I2C buses.
	context.writeLine("i2c:");
	if (devices.i2cBusCount() == 0) {
		context.writeLine("  none");
	} else {
		lexxos::apps::PrettyTable table{i2cColumns, sizeof(i2cColumns) / sizeof(i2cColumns[0])};
		table.print(context, i2cTableRow, &devices);
	}

	// Block devices.
	context.writeLine("block:");
	if (devices.blockDeviceCount() == 0) {
		context.writeLine("  none");
	} else {
		lexxos::apps::PrettyTable table{blockColumns, sizeof(blockColumns) / sizeof(blockColumns[0])};
		table.print(context, blockTableRow, &devices);
	}

	return 0;
}

} // namespace lexxos::apps::storage_programs
