#pragma once

namespace lexxos::apps::storage_programs::detail {

class SdSpeedScanSettingsGuard {
public:
	SdSpeedScanSettingsGuard();

	SdSpeedScanSettingsGuard(SdSpeedScanSettingsGuard const&) = delete;
	SdSpeedScanSettingsGuard& operator=(SdSpeedScanSettingsGuard const&) = delete;

	~SdSpeedScanSettingsGuard();

private:
	bool _readCrc{false};
	bool _multiBlock{false};
	bool _dmaTransfers{false};
	bool _highSpeedWrites{false};
};

} // namespace lexxos::apps::storage_programs::detail
