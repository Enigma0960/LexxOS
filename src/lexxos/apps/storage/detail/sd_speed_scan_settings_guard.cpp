#include <lexxos/apps/storage/detail/sd_speed_scan_settings_guard.h>

#include <lexxos/devices/sd_card.h>

namespace lexxos::apps::storage_programs::detail {

SdSpeedScanSettingsGuard::SdSpeedScanSettingsGuard()
    : _readCrc(lexxos::devices::sd_card::readCrcValidation())
    , _multiBlock(lexxos::devices::sd_card::multiBlockReadEnabled())
    , _dmaTransfers(lexxos::devices::sd_card::dmaTransfersEnabled())
    , _highSpeedWrites(lexxos::devices::sd_card::highSpeedWritesEnabled()) {
}

SdSpeedScanSettingsGuard::~SdSpeedScanSettingsGuard() {
	lexxos::devices::sd_card::setReadCrcValidation(_readCrc);
	lexxos::devices::sd_card::setMultiBlockRead(_multiBlock);
	lexxos::devices::sd_card::setDmaTransfersEnabled(_dmaTransfers);
	lexxos::devices::sd_card::setHighSpeedWritesEnabled(_highSpeedWrites);
}

} // namespace lexxos::apps::storage_programs::detail
