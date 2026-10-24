#pragma once

namespace lexxos::devices::storage::detail {

class SdCardTransferGuard {
public:
	SdCardTransferGuard();

	SdCardTransferGuard(SdCardTransferGuard const&) = delete;
	SdCardTransferGuard& operator=(SdCardTransferGuard const&) = delete;

	~SdCardTransferGuard();

	[[nodiscard]] bool locked() const;

private:
	bool _locked{false};
};

} // namespace lexxos::devices::storage::detail
