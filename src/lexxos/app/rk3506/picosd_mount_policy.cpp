#include <lexxos/app/rk3506/picosd_mount_policy.h>

#include <lexxos/app/rk3506/picosd_storage.h>

namespace lexxos::app::rk3506 {

namespace {

	bool isRemovableMount(std::string_view source, std::string_view target) {
		return (source == "sd1" || source == "/dev/sd1") && target == "/mnt/sd";
	}

} // namespace

std::string_view PicoSdMountPolicy::rejectionReason() const {
	return "sd1 unavailable or already mounted";
}

bool PicoSdMountPolicy::prepareMount(std::string_view source, std::string_view target) {
	return !isRemovableMount(source, target) || lexxos::platform::rk3506::picoSdPrepareManualMount();
}

void PicoSdMountPolicy::mountFinished(std::string_view source, std::string_view target, bool success) {
	if (isRemovableMount(source, target)) {
		lexxos::platform::rk3506::picoSdManualMountFinished(success);
	}
}

void PicoSdMountPolicy::unmounted(std::string_view target) {
	if (target == "/mnt/sd") {
		lexxos::platform::rk3506::picoSdNotifyManualUnmount();
	}
}

} // namespace lexxos::app::rk3506
