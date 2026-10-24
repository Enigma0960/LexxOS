#include <lexxos/runtime/capabilities.h>

#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/platform/board.h>
#include <lexxos/platform/platform_services.h>

namespace {

lexxos::platform::BoardDescriptor const& activeBoard(lexxos::platform::PlatformServices const& platform) {
	auto const& board = platform.board;
	return board.current != nullptr ? board.current() : lexxos::platform::board();
}

void addDisplayCapabilities(lexxos::runtime::CapabilitySet& capabilities, lexxos::devices::DeviceManager const& devices) {
	for (std::size_t index = 0; index < devices.displayDeviceCount(); ++index) {
		auto const& descriptor = devices.displayDeviceAt(index);
		auto const* display = descriptor.device;
		if (display == nullptr) {
			continue;
		}

		capabilities.add(lexxos::runtime::Capability::Display);
		if (display->nativeFormat() == lexxos::devices::PixelFormat::Rgb565) {
			capabilities.add(lexxos::runtime::Capability::DisplayRgb565);
		}
		if (display->supportsPowerMode()) {
			capabilities.add(lexxos::runtime::Capability::DisplayPowerMode);
		}
	}
}

void addDeviceRegistryCapabilities(lexxos::runtime::CapabilitySet& capabilities, lexxos::devices::DeviceManager const& devices) {
	if (devices.blockDeviceCount() > 0) {
		capabilities.add(lexxos::runtime::Capability::StorageBlock);
	}
	if (devices.inputDeviceCount() > 0) {
		capabilities.add(lexxos::runtime::Capability::InputKeyboard);
	}
	if (devices.pointingDeviceCount() > 0) {
		capabilities.add(lexxos::runtime::Capability::InputPointer);
	}
	if (devices.gamepadDeviceCount() > 0) {
		capabilities.add(lexxos::runtime::Capability::InputGamepad);
	}
	if (devices.picoCalcSouthbridgeServiceCount() > 0) {
		capabilities.add(lexxos::runtime::Capability::PicoCalcSouthbridge);
	}
}

void addBoardCapabilities(
    lexxos::runtime::CapabilitySet& capabilities,
    lexxos::platform::BoardDescriptor const& board,
    lexxos::platform::PlatformServices const& platform) {
	if (lexxos::platform::effectiveSdInterface(board) != lexxos::platform::SdInterface::None) {
		capabilities.add(lexxos::runtime::Capability::StorageRemovable);
	}
	if (board.audioBeep.isValid() || board.audioCodec.isValid()) {
		capabilities.add(lexxos::runtime::Capability::AudioTone);
	}
	if (board.audioCodec.isValid()) {
		capabilities.add(lexxos::runtime::Capability::AudioPcm);
	}
	if (board.wireless.wifi) {
		capabilities.add(lexxos::runtime::Capability::WirelessWifi);
	}
	if (board.wireless.bluetooth) {
		capabilities.add(lexxos::runtime::Capability::WirelessBluetooth);
	}
	if (board.psram.isQuadSpiLike()) {
		capabilities.add(lexxos::runtime::Capability::ExternalPsram);
	}
	if (board.userGpio.isValid()) {
		capabilities.add(lexxos::runtime::Capability::GpioUser);
	}
	if (board.softwareRtc.isValid()) {
		capabilities.add(lexxos::runtime::Capability::RtcSoftware);
	}
	if (platform.power.reboot != nullptr) {
		capabilities.add(lexxos::runtime::Capability::PowerReboot);
	}
	if (platform.power.readBattery != nullptr) {
		capabilities.add(lexxos::runtime::Capability::PowerBattery);
	}
	if (platform.flashRegions.findRegion != nullptr) {
		capabilities.add(lexxos::runtime::Capability::FlashNamedRegions);
	}
}

} // namespace

lexxos::runtime::CapabilitySet lexxos::runtime::detectCapabilities(
    lexxos::devices::DeviceManager const& devices,
    lexxos::platform::PlatformServices const& platform) {
	CapabilitySet capabilities{};
	addDisplayCapabilities(capabilities, devices);
	addDeviceRegistryCapabilities(capabilities, devices);
	addBoardCapabilities(capabilities, activeBoard(platform), platform);
	return capabilities;
}
