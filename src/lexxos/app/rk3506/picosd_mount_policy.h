#pragma once
#include <lexxos/os/storage_mount_policy.h>

namespace lexxos::app::rk3506 {

class PicoSdMountPolicy final : public lexxos::os::StorageMountPolicy {
public:
	[[nodiscard]] std::string_view rejectionReason() const override;
	bool prepareMount(std::string_view source, std::string_view target) override;
	void mountFinished(std::string_view source, std::string_view target, bool success) override;
	void unmounted(std::string_view target) override;
};

} // namespace lexxos::app::rk3506
