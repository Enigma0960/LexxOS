/* PicoCalc removable SPI1 microSD: board-policy automount, FAT32 and hotplug.
 * CMD24 write + CRC16 readback; on failure go offline until card reinsertion.
 * The independent Luckfox SDMMC boot device is never modified or auto-mounted.
 */

#include <lexxos/app/rk3506/picosd_storage.h>

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/app/rk3506/picosd_storage_service.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/rk3506/picosd_block_device.h>
#include <lexxos/devices/storage/block_device.h>
#include <lexxos/fs/fat/fat_mount_backend.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/kernel/system.h>
#include <lexxos/os/app_context.h>
#include <lexxos/os/operating_system.h>
#include <lexxos/program/program.h>
#include <lexxos/program/program_manager.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::app::rk3506 {

namespace {

	constexpr std::string_view kDiskName{"sd1"};
	constexpr std::string_view kMountPath{"/mnt/sd"};
	constexpr std::size_t kSectorSize = 512;

	constexpr std::uint32_t kDebounceSamples = 3; // 3 x 100 ms.
	constexpr std::uint32_t kRetryDelayMs = 2000;

	extern "C" std::uint32_t rk3506_timer_ticks();

	bool due(std::uint32_t now, std::uint32_t deadline) {
		return static_cast<std::int32_t>(now - deadline) >= 0;
	}

	void decimal(lexxos::os::AppContext& ctx, std::uint64_t value) {
		char buffer[21]{};
		std::size_t pos = sizeof(buffer) - 1U;

		do {
			buffer[--pos] = static_cast<char>('0' + value % 10U);
			value /= 10U;
		} while (value);

		ctx.write(buffer + pos);
	}

} // namespace

bool PicoSdStorageService::mount(lexxos::os::AppContext& ctx, bool quiet) {
	if (_filesystem.isMounted(kMountPath)) {
		ctx.writeLine("sd-diag: /mnt/sd already mounted");
		return true;
	}

	if (!_disk.isReady()) {
		ctx.writeLine("sd-diag: run 'sd-diag probe' first (or check card detect)");
		return false;
	}

	// Before advertising the device to FatFs, require a readable LBA 0.
	alignas(4) std::uint8_t sector[kSectorSize]{};
	if (!_disk.readBlocks(0, sector, 1U) || sector[510] != 0x55U || sector[511] != 0xAAU) {
		ctx.writeLine("sd-diag: invalid sector 0 / missing 55aa signature");
		return false;
	}

	auto* existing = ctx.devices().blockDevice(kDiskName);
	if (existing != nullptr && existing != &_disk) {
		ctx.writeLine("sd-diag: sd1 name already owned by another device");
		return false;
	}

	if (existing == nullptr && !ctx.devices().registerBlockDevice(kDiskName, "PicoCalc SPI1 removable microSD (board policy)", _disk)) {
		ctx.writeLine("sd-diag: no DeviceManager block-device slot");
		return false;
	}

	auto* existingVfs = ctx.vfs().blockDevice(kDiskName);
	if (existingVfs != nullptr && existingVfs != &_disk) {
		ctx.writeLine("sd-diag: sd1 VFS name already in use");
		return false;
	}

	if (existingVfs == nullptr && !ctx.vfs().registerBlockDevice(kDiskName, &_disk)) {
		ctx.writeLine("sd-diag: no VFS block-device slot");
		return false;
	}

	// Apply the removable-slot default after a successful probe and before
	// FatFs sees the write-protection state. Explicit RO and faults win.
	_state.armForMount();

	// FatFs supports both FAT VBR at LBA0 and MBR partition tables; the
	// existing FatVolume handles BPB/FAT validation and VFS integration.
	auto const status = _filesystem.mount(ctx.vfs(), kDiskName, kMountPath);
	if (status != lexxos::fs::fat::FatMountStatus::Ok) {
		_state.writeArmed = false;
		ctx.write("sd-diag: FatFs mount failed: ");
		ctx.writeLine(lexxos::fs::fat::toString(status));
		return false;
	}

	if (!quiet) {
		ctx.write(_state.writeArmed ? "storage: /dev/sd1 => /mnt/sd FAT RW; sectors="
		                            : "storage: /dev/sd1 => /mnt/sd FAT READ ONLY; sectors=");
		decimal(ctx, _disk.blockCount());
		ctx.writeLine("");
	}

	return true;
}

bool PicoSdStorageService::mountAtBoot(lexxos::os::OperatingSystem& os) {
	auto ctx = os.createAppContext({
	    .owner = lexxos::os::AppContextOwner::Kernel,
	});

	if (!_state.removablePolicy()) {
		ctx.writeLine("storage: sd1 policy not configured");
		return false;
	}

	// Boot mount runs before the background hotplug worker is created.
	// Keep the board's automount policy active so a later reinsertion
	// can recover without another manual probe.
	_state.autoEnabled = _state.removablePolicy()->autoMount;
	_state.autoSuppressed = false;
	_state.writeArmed = false;

	if (!_transport.inserted()) {
		ctx.writeLine("storage: sd1 not inserted");
		return false;
	}

	if (!_transport.probe()) {
		ctx.write("storage: sd1 probe failed: stage=");
		ctx.write(_transport.failureStage());
		ctx.write(" response=");
		decimal(ctx, _transport.lastResponse());
		ctx.writeLine("");
		return false;
	}

	// mount() applies the PicoCalc board RW policy after the probe;
	// no experimental write/boot-RW CMake options are required.
	bool const mounted = mount(ctx);
	if (!mounted) {
		_state.writeArmed = false;
	}

	return mounted;
}

int PicoSdStorageService::run(lexxos::os::AppContext& ctx, lexxos::program::ProgramInvocation const& inv,
    lexxos::program::ParsedArguments const&) {
	auto const action = inv.argc >= 2 ? std::string_view(inv.argv[1]) : "status";

	if (action == "probe") {
		if (_filesystem.isMounted(kMountPath)) {
			ctx.writeLine("sd-media: probe refused while mounted");
			return 1;
		}

		_state.writeArmed = false;
		// A missing card marks the end of a media session even when the
		// user explicitly requests a probe.
		if (!_transport.inserted()) {
			_state.cardRemoved();
		}

		bool const ok = _transport.probe();
		ctx.write(ok ? "sd-media: probe ok: stage=" : "sd-media: probe failed: stage=");
		ctx.write(_transport.failureStage());
		ctx.write(" response=");
		decimal(ctx, _transport.lastResponse());
		ctx.writeLine("");
		return ok ? 0 : 1;
	}

	if (action == "mount") {
		bool const ok = mount(ctx);
		if (ok) {
			_state.autoSuppressed = _state.writeArmed; // Never auto-reprobe a RW session.
		}

		return ok ? 0 : 1;
	}

	if (action == "rw") {
		// Normal mounts use board RW; explicit RO persists across remounts.
		bool const enable = inv.argc == 4 && std::string_view(inv.argv[2]) == "on" && std::string_view(inv.argv[3]) == "I-ACCEPT-DATA-LOSS";
		bool const disable = inv.argc == 3 && std::string_view(inv.argv[2]) == "off";

		if (!enable && !disable) {
			ctx.writeLine("usage: sd-media rw [off | on I-ACCEPT-DATA-LOSS]");
			ctx.writeLine("WARNING: only use RW on a disposable test card.");
			return 1;
		}

#if defined(LEXXOS_RK3506_SD_FORCE_READ_ONLY)
		if (enable) {
			ctx.writeLine("storage: RW disabled by the recovery read-only build");
			return 1;
		}
#endif
		if (enable && _state.writeFaulted) {
			ctx.writeLine("storage: RW refused after failed sector write; inspect card before restarting");
			return 1;
		}

		// FatFs drive protection is latched at mount; no live transitions.
		if (_filesystem.isMounted(kMountPath) || ctx.vfs().isMountTarget(kMountPath)) {
			ctx.writeLine("sd-diag: unmount /mnt/sd before changing RW mode");
			return 1;
		}

		if (enable && (_state.autoEnabled || !_disk.isReady())) {
			ctx.writeLine("sd-diag: RW refused; run 'sd-media auto off', then ensure card ready");
			return 1;
		}

		_state.manualReadOnly = disable;
		_state.writeArmed = enable;
		if (enable) {
			_state.autoSuppressed = true;
		}

		ctx.writeLine(enable ? "storage: RW manually enabled for this card"
		                     : "storage: read-only mode selected");
		return 0;
	}

	if (action == "auto") {
		if (inv.argc != 3) {
			ctx.writeLine("usage: sd-media auto [on | off]");
			return 1;
		}

		std::string_view const value(inv.argv[2]);
		if (value != "on" && value != "off") {
			ctx.writeLine("usage: sd-media auto [on | off]");
			return 1;
		}

		// Never change a mounted volume's protection status mid-session.
		if (value == "on" && _state.writeArmed && (_filesystem.isMounted(kMountPath) || ctx.vfs().isMountTarget(kMountPath))) {
			ctx.writeLine("sd-diag: unmount RW volume before enabling automount");
			return 1;
		}

		_state.autoEnabled = value == "on";
		if (_state.autoEnabled) {
			_state.writeArmed = false;
			_state.autoSuppressed = _state.writeFaulted;
		}

		ctx.writeLine(_state.autoEnabled ? "sd-diag: automount enabled" : "sd-diag: automount disabled");
		return 0;
	}

	if (action == "status") {
		ctx.write("sd-media: protocol-stage=");
		ctx.write(_transport.failureStage());
		ctx.write(" response=");
		decimal(ctx, _transport.lastResponse());
		ctx.writeLine("");
		ctx.write("sd-diag mount: ");
		ctx.writeLine(_filesystem.isMounted(kMountPath)
		        ? (_state.writeArmed ? "/mnt/sd rw" : "/mnt/sd ro")
		        : "not mounted");
		ctx.write("sd-diag write: ");
		ctx.writeLine(_state.writeArmed ? "enabled" : "read-only");
		if (_state.writeFaulted) {
			ctx.writeLine("sd-diag write-fault: latched until card removal");
		}

		ctx.write("sd-diag card: ");
		ctx.writeLine(_transport.inserted() ? "inserted" : "absent");
		ctx.write("sd-diag automount: ");
		ctx.writeLine(!_state.autoEnabled ? "off" : _state.autoSuppressed ? "suppressed until reinsertion"
		                                                                  : "on");

		// Report the same policy snapshot used by manual mount and hotplug.
		for (auto const& slot : _state.policy()) {
			ctx.write("storage /dev/");
			ctx.write(slot.deviceName == nullptr ? "?" : slot.deviceName);
			ctx.write(slot.role == lexxos::hardware::products::StorageRole::System ? " system" : " removable");
			ctx.write(slot.hotSwappable ? " hotplug" : " fixed");
			ctx.write(slot.autoMount ? " automount" : " manual");
			ctx.writeLine(slot.readOnlyByDefault ? " default-ro" : " default-rw");
		}

		return 0;
	}

	if (action == "recover") {
		_state.autoSuppressed = true;

		// Emergency force-unmount only after the card is no longer ready.
		// An interrupted FAT write may require checking the card on a PC.
		if (!_filesystem.isMounted(kMountPath)) {
			_state.writeArmed = false;
			ctx.writeLine("sd-diag: not mounted");
			return 0;
		}

		if (_disk.isReady()) {
			ctx.writeLine("sd-diag: recover refused: sd1 ready; use unmount");
			return 1;
		}

		auto const status = _filesystem.forceUnmount(ctx.vfs(), kMountPath);
		_state.writeArmed = false;
		ctx.write("sd-diag: recovery unmount: ");
		ctx.writeLine(lexxos::vfs::toString(status));
		return status == lexxos::vfs::UnmountStatus::Ok ? 0 : 1;
	}

	if (action == "unmount") {
		_state.autoSuppressed = true;

		if (!_filesystem.isMounted(kMountPath)) {
			_state.writeArmed = false;
			ctx.writeLine("sd-diag: not mounted");
			return 0;
		}

		auto const status = _filesystem.unmount(ctx.vfs(), kMountPath);

		if (status == lexxos::vfs::UnmountStatus::Ok) {
			_state.writeArmed = false;
		}

		ctx.write("sd-diag: unmount: ");
		ctx.writeLine(lexxos::vfs::toString(status));
		return status == lexxos::vfs::UnmountStatus::Ok ? 0 : 1;
	}

	ctx.writeLine("usage: sd-media [probe | mount | status | unmount | recover | auto on|off | rw on|off]");
	return 1;
}

// The standard `mount /dev/sd1 /mnt/sd` and `umount /mnt/sd`
// commands use these board hooks.  They do not touch the boot SDMMC.
// Board-configured removable-slot service. Never scans the system sd0.
// The FreeRTOS worker starts only after the boot mount. FatVolume serializes
// its own operations, while every sector I/O checks card-detect and fails
// closed on removal.
void PicoSdStorageService::hotplug(lexxos::os::OperatingSystem& os) {
	auto ctx = os.createAppContext({.owner = lexxos::os::AppContextOwner::Kernel});
	auto const* const slot = _state.removablePolicy();

	if (slot == nullptr) {
		ctx.writeLine("storage: missing removable sd1 policy; no card access");
		return;
	}

	// Never reset writeArmed, autoEnabled or manual suppression here:
	// the boot mount may already be RW, or the user may have unmounted it.
	// Treat a mounted volume as an initially present card so that a
	// subsequent removal must pass the debounce before allowing reprobe.
	bool observed = _transport.inserted();
	bool inserted = observed || _filesystem.isMounted(kMountPath);
	std::uint32_t samples = kDebounceSamples;
	std::uint32_t nextRetry{};

	while (!lexxos::kernel::isShutdownRequested()) {
		bool const present = _transport.inserted();
		if (present == observed) {
			if (samples < kDebounceSamples) {
				++samples;
			}
		} else {
			observed = present;
			samples = 1;
		}

		// On removal, fail closed immediately; never wait to touch the medium.
		if (!present && (_transport.ready() || _filesystem.isMounted(kMountPath))) {
			// Fail closed on the first missing sample, but do not clear a
			// latched write failure until absence is debounced. A transient
			// card-detect glitch must not silently rearm FAT writes.
			bool const hadWrites = _state.writeArmed;
			_state.writeArmed = false;
			if (hadWrites) {
				_state.autoSuppressed = true;
			}

			_transport.invalidateRemoved();

			if (_filesystem.isMounted(kMountPath) || ctx.vfs().isMountTarget(kMountPath)) {
				(void) _filesystem.forceUnmount(ctx.vfs(), kMountPath);
			}

			nextRetry = 0;
		}

		if (samples >= kDebounceSamples && present != inserted) {
			inserted = present;
			nextRetry = 0;

			if (!inserted) {
				_state.cardRemoved();
				_state.autoSuppressed = false;
				_transport.invalidateRemoved();
			}
		}

		if (inserted && _filesystem.isMounted(kMountPath) && !_transport.ready()) {
			// On RW media failures, do not automatically reprobe/rewrite FAT.
			bool const hadWrites = _state.writeArmed;
			_state.writeArmed = false;

			if (hadWrites) {
				_state.autoSuppressed = true;
			}

			(void) _filesystem.forceUnmount(ctx.vfs(), kMountPath);
			ctx.writeLine("sd-diag: lost card, FAT mount invalidated");
			nextRetry = rk3506_timer_ticks() + kRetryDelayMs;
		}

		// A mounted card must keep its current read/write state. A repeated
		// _transport.probe() returns false while mounted, which is not a fault.
		// Probe exactly once for a new/unmounted card, after debounce and retry.
		if (inserted && _state.autoEnabled && !_state.autoSuppressed && !_filesystem.isMounted(kMountPath) && !ctx.vfs().isMountTarget(kMountPath) && due(rk3506_timer_ticks(), nextRetry)) {
			if (_transport.probe()) {
				// mount() applies board policy or explicit RO before FatFs.
				if (!mount(ctx, true)) {
					_state.writeArmed = false;
					nextRetry = rk3506_timer_ticks() + kRetryDelayMs;
				}
			} else {
				_state.writeArmed = false;
				nextRetry = rk3506_timer_ticks() + kRetryDelayMs;
			}
		}

		lexxos::kernel::sleepMs(100);
	}
}

} // namespace lexxos::app::rk3506
