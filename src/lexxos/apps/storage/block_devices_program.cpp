#include <lexxos/apps/storage/block_devices_program.h>

#include <cstddef>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/common/pretty_table.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/storage/block_device.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::storage_programs {
namespace {

	constexpr lexxos::apps::PrettyTableColumn blockDeviceColumns[] = {
	    {       .name = "name",  .align = lexxos::apps::PrettyTableAlign::Left,  .minWidth = 4, .maxWidth = 16},
	    {      .name = "state",  .align = lexxos::apps::PrettyTableAlign::Left,  .minWidth = 5,  .maxWidth = 9},
	    {       .name = "lock",  .align = lexxos::apps::PrettyTableAlign::Left,  .minWidth = 4,  .maxWidth = 4},
	    {   .name = "priority",  .align = lexxos::apps::PrettyTableAlign::Left,  .minWidth = 8, .maxWidth = 11},
	    {      .name = "owner",  .align = lexxos::apps::PrettyTableAlign::Left,  .minWidth = 5, .maxWidth = 16},
	    { .name = "block-size", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 10, .maxWidth = 10},
	    {.name = "block-count", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 11, .maxWidth = 14},
	    {.name = "description",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 11, .maxWidth = 32},
	};

	bool blockDeviceTableRow(std::size_t rowIndex, lexxos::apps::PrettyTableRow& row, void* userData) {
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

lexxos::program::ProgramDescriptor BlockDevicesProgram::descriptor() {
	return {
	    .name = "block-devs",
	    .help = "show registered block devices",
	    .description = "Lists block devices registered in DeviceManager and shows their current readiness and geometry.",
	    .usage = "block-devs",
	    .handler = run,
	};
}

int BlockDevicesProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	auto& devices = context.devices();
	if (devices.blockDeviceCount() == 0) {
		context.writeLine("no block devices registered");
		return 0;
	}

	lexxos::apps::PrettyTable table{blockDeviceColumns, sizeof(blockDeviceColumns) / sizeof(blockDeviceColumns[0])};
	table.print(context, blockDeviceTableRow, &devices);
	return 0;
}

} // namespace lexxos::apps::storage_programs
