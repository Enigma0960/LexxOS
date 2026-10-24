#include <lexxos/apps/fs/sync_program.h>

#include <lexxos/os/app_context.h>
#include <lexxos/vfs/vfs.h>

#if defined(LEXXOS_PLATFORM_SIM)
	#include <lexxos/fs/fat/fat_volume.h>
// Simulator-only bridge; stage 0 checks hashes, 1 applies after unmount, 2 cancels.
extern "C" int lexxos_sim_host_directory_sync(int stage);
#endif

namespace lexxos::apps::filesystem_programs {

lexxos::program::ProgramDescriptor SyncProgram::descriptor() {
	return {
	    .name = "sync",
	    .help = "synchronize mounted writable filesystems",
	    .description = "Flushes mounted filesystems; in the simulator --sd-dir also reconciles the host directory with FAT32.",
	    .usage = "sync",
	    .handler = run,
	};
}

int SyncProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	auto const status = lexxos::vfs::sync();
	if (status != lexxos::vfs::SyncStatus::Ok) {
		context.write("sync: ");
		context.writeLine(lexxos::vfs::toString(status));
		return 1;
	}

#if defined(LEXXOS_PLATFORM_SIM)
	if (lexxos::fs::fat::isMounted("/mnt/sd")) {
		int const pending = lexxos_sim_host_directory_sync(0);
		if (pending < 0) {
			context.writeLine("sync: host directory check failed (see emulator log)");
			return 1;
		}
		if (pending > 0) {
			// Must not change FAT sectors while the FatFs volume is mounted.
			auto const unmounted = lexxos::fs::fat::unmount("/mnt/sd");
			if (unmounted != lexxos::vfs::UnmountStatus::Ok) {
				(void) lexxos_sim_host_directory_sync(2);
				context.writeLine("sync: cannot unmount /mnt/sd for host synchronization");
				return 1;
			}
			int const applied = lexxos_sim_host_directory_sync(1);
			// Always attempt remount, including after a conflict or I/O error.
			auto const remounted = lexxos::fs::fat::mount("/dev/sd0", "/mnt/sd");
			if (applied != 0 || remounted != lexxos::fs::fat::FatMountStatus::Ok) {
				context.writeLine("sync: host directory sync or FAT32 remount failed (see emulator log)");
				return 1;
			}
		}
	}
#endif

	context.writeLine("sync: ok");
	return 0;
}

} // namespace lexxos::apps::filesystem_programs
