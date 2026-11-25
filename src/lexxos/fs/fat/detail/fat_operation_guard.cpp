#include <lexxos/fs/fat/detail/fat_operation_guard.h>

#include <lexxos/fs/fat/fat_volume.h>

namespace lexxos::fs::fat::detail {

FatOperationGuard::FatOperationGuard(FatVolume const& volume)
    : _volume(volume)
    , _locked(_volume.lockOperation()) {
}

FatOperationGuard::~FatOperationGuard() {
	if (_locked) {
		_volume.unlockOperation();
	}
}

bool FatOperationGuard::locked() const {
	return _locked;
}

} // namespace lexxos::fs::fat::detail
