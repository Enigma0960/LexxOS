#pragma once

#include <lexxos/app/rk3506/picosd_service_state.h>
#include <lexxos/devices/rk3506/picosd_block_device.h>

namespace lexxos::fs::fat {

class FatMountBackend;

} // namespace lexxos::fs::fat

namespace lexxos::os {

class AppContext;
class OperatingSystem;

} // namespace lexxos::os

namespace lexxos::program {

class ProgramManager;
struct ProgramInvocation;
class ParsedArguments;

} // namespace lexxos::program

namespace lexxos::app::rk3506 {

// Backend и callback ссылаются на состояние сервиса, поэтому перемещение запрещено.
// Composition обеспечивает время жизни сервиса и обеих переданных зависимостей.
class PicoSdStorageService {
public:
	PicoSdStorageService(devices::SdMediaTransport& transport, fs::fat::FatMountBackend& filesystem);
	PicoSdStorageService(PicoSdStorageService const&) = delete;
	PicoSdStorageService& operator=(PicoSdStorageService const&) = delete;
	PicoSdStorageService(PicoSdStorageService&&) = delete;
	PicoSdStorageService& operator=(PicoSdStorageService&&) = delete;

	devices::SdCardBackend& backend();

	bool configure(std::span<hardware::products::StoragePolicy const> slots);
	bool prepareManualMount();
	void suppressUntilRemoval();
	void manualMountFinished(bool success);
	void notifyManualUnmount();
	void hotplug(os::OperatingSystem& os);
	bool mountAtBoot(os::OperatingSystem& os);
	bool fatMounted() const;

	int run(os::AppContext& ctx, program::ProgramInvocation const& invocation, program::ParsedArguments const& arguments);

	[[nodiscard]] bool configured() const;

private:
	devices::SdMediaTransport& _transport;
	fs::fat::FatMountBackend& _filesystem;
	PicoSdServiceState _state;
	devices::rk3506::PicoSdBlockDevice _disk;

private:
	static void writeFailed(void* context);
	bool mount(os::AppContext& ctx, bool quiet = false);
};

} // namespace lexxos::app::rk3506
