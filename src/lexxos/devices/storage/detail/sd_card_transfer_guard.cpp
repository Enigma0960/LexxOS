#include <lexxos/devices/storage/detail/sd_card_transfer_guard.h>

#include <lexxos/devices/storage/detail/sd_card_transfer_mutex.h>

namespace lexxos::devices::storage::detail {

SdCardTransferGuard::SdCardTransferGuard() {
	_locked = SdCardTransferMutex::instance().lock();
}

SdCardTransferGuard::~SdCardTransferGuard() {
	if (_locked) {
		SdCardTransferMutex::instance().unlock();
	}
}

bool SdCardTransferGuard::locked() const {
	return _locked;
}

} // namespace lexxos::devices::storage::detail
