#include <lexxos/apps/core/memory_program.h>

#include <cstdint>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/feature.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/app_context.h>
#include <lexxos/runtime/selected_runtime_profile.h>

#if defined(LEXXOS_RK3506_MMU_MANAGED)
	#include <lexxos/platform/rockchip/rk3506/fdt_memory_map.h>
	#include <lexxos/platform/rockchip/rk3506/mmu_managed.h>
#endif

#if defined(LEXXOS_RK3506_DMA_SAFE)
	#include <FreeRTOS.h>
	#include <task.h>

	#include <lexxos/devices/rk3506/picosd_dma_pl330.h>
	#if FEATURE_ENABLED(DISPLAY)
		#include <lexxos/devices/rk3506/picocalc_lcd_dma_pl330.h>
	#endif
#endif

namespace lexxos::apps::system_programs {

namespace {

	constexpr lexxos::program::ProgramArgumentSpec memoryArguments[] = {
	    {
	     .name = "view",
	     .valueName = "profile|map|pages|page-test|regions|extended-test|fdt|dma-test|dma-stats",
	     .help = "show memory limits, FDT, DMA diagnostics and page pools",
	     .kind = lexxos::program::ProgramArgumentKind::Positional,
	     .required = false,
	     },
	};

	void writeLimit(lexxos::os::AppContext& context, std::string_view name,
	    std::uint64_t value, std::string_view unit = {}) {
		context.write(name);
		context.write(": ");
		lexxos::apps::writeUnsigned64(context, value);

		if (!unit.empty()) {
			context.write(" ");
			context.write(unit);
		}

		context.writeLine("");
	}

	void writeProfile(lexxos::os::AppContext& context) {
		auto const& profile = lexxos::runtime::runtimeProfile();
		context.writeLine("resolved runtime memory profile (compile-time):");
		context.write("chip profile: ");
		context.writeLine(profile.origin.chip);
		context.write("platform profile: ");
		context.writeLine(profile.origin.platform);
		context.write("board: ");
		context.writeLine(profile.origin.board);

		context.writeLine("[kernel]");
		writeLimit(context, "heap", profile.kernel.heapBytes, "bytes");
		writeLimit(context, "task stack pool", profile.kernel.taskStackPoolBytes, "bytes");
		writeLimit(context, "task slots", profile.kernel.taskSlots);
		writeLimit(context, "minimum task stack", profile.kernel.minimumTaskStackBytes, "bytes");
		writeLimit(context, "maximum task stack", profile.kernel.maximumTaskStackBytes, "bytes");

		context.writeLine("[system task stacks]");
		writeLimit(context, "console", profile.taskStacks.consoleTaskBytes, "bytes");
		writeLimit(context, "system events", profile.taskStacks.systemEventTaskBytes, "bytes");
		writeLimit(context, "console mirror", profile.taskStacks.consoleMirrorTaskBytes, "bytes");
		writeLimit(context, "local console input", profile.taskStacks.localConsoleInputTaskBytes, "bytes");
		writeLimit(context, "health", profile.taskStacks.healthTaskBytes, "bytes");
		writeLimit(context, "debug display", profile.taskStacks.debugDisplayTaskBytes, "bytes");
		writeLimit(context, "display power", profile.taskStacks.displayPowerTaskBytes, "bytes");

		context.writeLine("[programs]");
		writeLimit(context, "registered programs", profile.programs.maxPrograms);
		writeLimit(context, "concurrent processes", profile.programs.maxProcesses);
		writeLimit(context, "argument count", profile.programs.maxArguments);
		writeLimit(context, "argument storage", profile.programs.argumentStorageBytes, "bytes");
		writeLimit(context, "default stack", profile.programs.defaultStackSizeBytes, "bytes");
		writeLimit(context, "minimum task stack", profile.programs.minimumTaskStackBytes, "bytes");
		writeLimit(context, "inline task stack", profile.programs.inlineTaskStackBytes, "bytes");
		writeLimit(context, "foreground cleanup delay", profile.programs.foregroundCleanupDelayMs, "ms");
		writeLimit(context, "default internal workspace", profile.programs.defaultInternalWorkspaceBytes, "bytes");
		writeLimit(context, "default external workspace", profile.programs.defaultExternalWorkspaceBytes, "bytes");

		context.writeLine("[shell]");
		writeLimit(context, "maximum line length", profile.shell.maxLineLength, "bytes");
		writeLimit(context, "maximum arguments", profile.shell.maxArguments);
		writeLimit(context, "history entries", profile.shell.maxHistoryEntries);
		writeLimit(context, "injected input ring", profile.shell.injectedInputBytes, "bytes");

		context.writeLine("[queues]");
		writeLimit(context, "lifecycle events", profile.events.lifecycleQueueCapacity);
		writeLimit(context, "program input", profile.events.programInputQueueCapacity);

		context.writeLine("[UI]");
		writeLimit(context, "resolved controls", profile.ui.maxResolvedControls);
		writeLimit(context, "v2 wire nodes", 16);
		writeLimit(context, "v3 tree nodes", profile.ui.maxTreeNodes);
		writeLimit(context, "UI list stores", profile.ui.maxListStores);

		context.writeLine("[VFS]");
		writeLimit(context, "maximum path length", profile.vfs.maxPathLength, "bytes");
		writeLimit(context, "maximum name length", profile.vfs.maxNameLength, "bytes");
		writeLimit(context, "block devices", profile.vfs.maxBlockDevices);
		writeLimit(context, "filesystem mounts", profile.vfs.maxFilesystemMounts);
		writeLimit(context, "tmp files", profile.vfs.maxTmpFiles);
		writeLimit(context, "maximum tmp file size", profile.vfs.maxTmpFileSizeBytes, "bytes");
		writeLimit(context, "filesystem factories", profile.vfs.maxFilesystemFactories);

		context.writeLine("[device registry]");
		writeLimit(context, "I2C buses", profile.devices.maxI2cBuses);
		writeLimit(context, "block devices", profile.devices.maxBlockDevices);
		writeLimit(context, "displays", profile.devices.maxDisplayDevices);
		writeLimit(context, "input devices", profile.devices.maxInputDevices);
		writeLimit(context, "pointing devices", profile.devices.maxPointingDevices);
		writeLimit(context, "gamepads", profile.devices.maxGamepadDevices);
		writeLimit(context, "audio devices", profile.devices.maxAudioDevices);
		writeLimit(context, "microphones", profile.devices.maxMicrophoneDevices);
		writeLimit(context, "sensors", profile.devices.maxSensorDevices);
		writeLimit(context, "infrared devices", profile.devices.maxInfraredDevices);
		writeLimit(context, "PicoCalc southbridge services", profile.devices.maxPicoCalcSouthbridgeServices);

		context.writeLine("Note: zero kernel pool means the backend owns its allocator;");
		context.writeLine("      zero workspace quota means no default reservation.");
	}

} // namespace

lexxos::program::ProgramDescriptor MemoryProgram::descriptor() {
	return {
	    .name = "mem",
	    .help = "show kernel memory statistics or configured limits",
	    .description = "Shows live heap statistics, or the resolved compile-time runtime memory profile.",
	    .usage = "mem [profile|map|pages|page-test|regions|extended-test|fdt|dma-test|dma-stats]",
	    .arguments = lexxos::apps::argumentList(memoryArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int MemoryProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto const view = arguments.positional("view");
	if (view == "profile") {
		writeProfile(context);
		return 0;
	}

#if defined(LEXXOS_RK3506_MMU_MANAGED)
	if (view == "fdt") {
		auto const& fdt = lexxos::platform::rk3506::memory::firmwareMemoryMap();
		context.write("boot FDT: ");
		context.writeLine(fdt.status);
		writeLimit(context, "FDT physical", fdt.sourceAddress);
		writeLimit(context, "FDT size", fdt.sourceBytes, "bytes");
		writeLimit(context, "RAM banks", fdt.bankCount);

		for (std::uint32_t i = 0; i < fdt.bankCount; ++i) {
			context.write("RAM bank ");
			writeUnsigned(context, i);
			context.write(": ");
			lexxos::apps::writeUnsigned64(context, fdt.banks[i].first);
			context.write(" .. ");
			lexxos::apps::writeUnsigned64(context, fdt.banks[i].past);
			context.writeLine(" (end exclusive)");
		}

		writeLimit(context, "FDT reservations", fdt.reservedCount);
		for (std::uint32_t i = 0; i < fdt.reservedCount; ++i) {
			context.write("reserved ");
			writeUnsigned(context, i);
			context.write(": ");
			lexxos::apps::writeUnsigned64(context, fdt.reserved[i].first);
			context.write(" .. ");
			lexxos::apps::writeUnsigned64(context, fdt.reserved[i].past);
			context.writeLine(" (end exclusive)");
		}

		context.write("static reservations identity-mapped: ");
		context.writeLine(fdt.reservationsIdentityMapped ? "yes" : "no / unverified");
		context.write("extended DDR FDT approval: ");
		context.writeLine(lexxos::platform::rk3506::memory::fdtRangeUnreserved(0x02800000U, 0x00800000U) ? "yes" : "DENIED");
		context.writeLine("FDT approval is not proof that U-Boot runtime scratch is free.");
		return fdt.valid ? 0 : 1;
	}
#endif

#if defined(LEXXOS_RK3506_MMU_MANAGED)
	if (view == "map") {
		auto const map = lexxos::platform::rk3506::memory::snapshot();
		context.write("managed MMU: ");
		context.writeLine(map.active ? "active" : "disabled");
		context.writeLine("BOOTRAM: 0x02000000..0x027fffff (4 KiB pages)");
		writeLimit(context, "RX end", map.executableEnd, "physical");
		writeLimit(context, "kernel image end", map.imageEnd, "physical");
		writeLimit(context, "DMA start", map.dmaStart, "physical");
		writeLimit(context, "DMA end", map.dmaEnd, "physical");
		writeLimit(context, "DMA pool", map.dmaPoolBytes, "bytes");
		writeLimit(context, "DMA free", map.dmaFreeBytes, "bytes");
		writeLimit(context, "D-cache line", map.cacheLineBytes, "bytes");
		context.writeLine("Other DDR: Normal-NC/XN; MMIO: Device/XN");
		return 0;
	}
#endif

#if defined(LEXXOS_RK3506_MMU_MANAGED)
	if (view == "pages" || view == "page-test") {
		if (view == "page-test") {
			auto const ok = lexxos::platform::rk3506::memory::ownedPagesSelfTest();
			context.writeLine(ok ? "page test: ok" : "page test: FAILED");
			if (!ok) {
				return 1;
			}
		}

		auto const pages = lexxos::platform::rk3506::memory::ownedPagesSnapshot();
		context.writeLine("RK3506 owned 4 KiB physical frames (WBWA RW/XN):");
		writeLimit(context, "first", pages.first, "physical");
		writeLimit(context, "past", pages.past, "physical");
		writeLimit(context, "total", pages.totalPages, "pages");
		writeLimit(context, "free", pages.freePages, "pages");
		writeLimit(context, "largest free run", pages.largestFreeRun, "pages");
		writeLimit(context, "allocations", pages.activeAllocations);

		return 0;
	}
#endif

#if defined(LEXXOS_RK3506_MMU_MANAGED)
	if (view == "regions" || view == "extended-test") {
		if (view == "extended-test") {
			if (!lexxos::platform::rk3506::memory::extendedPagesSnapshot().enabled) {
				context.writeErrorLine("extended DDR is disabled in this build");
				return 1;
			}

			if (!lexxos::platform::rk3506::memory::extendedPagesSelfTest()) {
				context.writeErrorLine("extended DDR test: FAILED");
				return 1;
			}

			context.writeLine("extended DDR test: ok");
		}

		context.writeLine("RK3506 DDR physical regions:");
		context.writeLine("0x00000000..0x01FFFFFF: firmware / FDT / boot reserved (NC)");
		context.writeLine("0x02000000..0x025FFFFF: LexxOS BOOTRAM and owned frames (WBWA)");
		context.writeLine("0x02600000..0x026FFFFF: U-Boot boot scratch (reserved)");
		context.writeLine("0x02700000..0x027EFFFF: DMA reserve (NC)");
		context.writeLine("0x027F0000..0x027FFFFF: GDB monitor (reserved)");
		context.writeLine("0x03000000..0x07FFFFFF: firmware/unclaimed (NC)");

		auto const extended = lexxos::platform::rk3506::memory::extendedPagesSnapshot();
		context.write("0x02800000..0x02FFFFFF: ");
		context.writeLine(extended.enabled ? "EXPERIMENTAL application DDR (WBWA, fault when free)" : "unclaimed DDR (NC)");
		writeLimit(context, "extended total", extended.totalPages, "pages");
		writeLimit(context, "extended free", extended.freePages, "pages");
		writeLimit(context, "extended largest run", extended.largestFreeRun, "pages");
		writeLimit(context, "extended allocations", extended.activeAllocations);
		context.writeLine("FreeRTOS heap_4 stays inside BOOTRAM");

		return 0;
	}
#endif

#if defined(LEXXOS_RK3506_MMU_MANAGED)
	if (view == "dma-test") {
		auto const before = lexxos::platform::rk3506::memory::snapshot();
		auto* const storage = static_cast<std::uint8_t*>(lexxos::platform::rk3506::memory::allocateDma(4096U));
		if (storage == nullptr) {
			context.writeErrorLine("DMA NC pool test: allocation FAILED");
			return 1;
		}

		bool ok = lexxos::platform::rk3506::memory::isDmaBuffer(storage, 4096U);
		std::uint8_t volatile* const bytes = storage;
		for (std::size_t i = 0; i < 4096U; ++i) {
			bytes[i] = static_cast<std::uint8_t>((i * 73U + 29U) & 255U);
		}

		for (std::size_t i = 0; i < 4096U; ++i) {
			if (bytes[i] != static_cast<std::uint8_t>((i * 73U + 29U) & 255U)) {
				ok = false;
				break;
			}
		}

		ok = lexxos::platform::rk3506::memory::freeDma(storage) && ok;
		ok = lexxos::platform::rk3506::memory::snapshot().dmaFreeBytes == before.dmaFreeBytes && ok;
		context.writeLine(ok ? "DMA NC pool test: ok" : "DMA NC pool test: FAILED");
		return ok ? 0 : 1;
	}
#endif

#if defined(LEXXOS_RK3506_DMA_SAFE)
	if (view == "dma-stats") {
		// На одном CPU защищаем 64-битные снимки от вытеснения задачами и IRQ.
		// Вывод и чтение регистров выполняются за пределами критической секции.
		taskENTER_CRITICAL();
		auto const sd = lexxos::platform::rk3506::picoSdDmaStatistics();
	#if FEATURE_ENABLED(DISPLAY)
		auto const lcd = lexxos::platform::rk3506::lcdDmaStatistics();
	#endif
		taskEXIT_CRITICAL();
		context.writeLine("RK3506 PL330 DMA runtime (CNTVCT = 24 MHz):");
		writeLimit(context, "SD completed sectors", sd.reads);
		writeLimit(context, "SD bytes", sd.bytes);
		writeLimit(context, "SD errors", sd.errors);
		writeLimit(context, "SD timeouts", sd.timeouts);
		writeLimit(context, "SD NC buffered sectors", sd.bufferedReads);
		writeLimit(context, "SD direct fallbacks", sd.directFallbacks);
		writeLimit(context, "SD IRQ deliveries", sd.irqInterrupts);
		writeLimit(context, "SD poll wait iterations", sd.waitPolls);
		if (sd.payloadCycles != 0U) {
			writeLimit(context, "SD payload KiB/s",
			    (static_cast<std::uint64_t>(sd.bytes) * 24000000U) / (1024U * sd.payloadCycles));
		}
	#if FEATURE_ENABLED(DISPLAY)
		writeLimit(context, "LCD completed chunks", lcd.chunks);
		writeLimit(context, "LCD RGB565 chunks", lcd.direct16Chunks);
		writeLimit(context, "LCD burst chunks", lcd.burstChunks);
		writeLimit(context, "LCD single-beat chunks", lcd.singleChunks);
		writeLimit(context, "LCD burst bytes", lcd.burstBytes);
		writeLimit(context, "LCD bytes", lcd.bytes);
		writeLimit(context, "LCD staged bytes", lcd.stagingBytes);
		writeLimit(context, "LCD staging ticks", lcd.stagingTicks);
		writeLimit(context, "LCD errors", lcd.errors);
		writeLimit(context, "LCD timeouts", lcd.timeouts);
		writeLimit(context, "LCD IRQ deliveries", lcd.irqInterrupts);
		writeLimit(context, "LCD poll completions", lcd.irqPollFallbacks);
		writeLimit(context, "LCD IRQ timed samples", lcd.timedIrqs);
		writeLimit(context, "LCD completion task sleeps", lcd.completionTaskSleeps);
		writeLimit(context, "LCD completion semaphore waits", lcd.completionSemaphoreWaits);
		writeLimit(context, "LCD completion semaphore wakes", lcd.completionSemaphoreWakeups);
		writeLimit(context, "LCD completion semaphore timeouts", lcd.completionSemaphoreTimeouts);
		writeLimit(context, "LCD completion ISR signals", lcd.completionSemaphoreSignals);
		writeLimit(context, "LCD completion WFE waits", lcd.completionWfeWaits);
		writeLimit(context, "LCD WFE time us", lcd.completionWfeTicks / 24U);
		writeLimit(context, "LCD FIFO drain task sleeps", lcd.drainTaskSleeps);
		writeLimit(context, "LCD FIFO drain spin checks", lcd.drainSpinChecks);
		writeLimit(context, "LCD PL330 IRQ time us", lcd.irqTicks / 24U);
		writeLimit(context, "LCD IRQ to task observation us", lcd.wakeupTicks / 24U);
		writeLimit(context, "LCD SPI FIFO drain us", lcd.drainTicks / 24U);

		if (lcd.timedIrqs != 0U) {
			writeLimit(context, "LCD average IRQ to task us",
			    lcd.wakeupTicks / (24U * lcd.timedIrqs));
		}

		if (lcd.chunks != 0U) {
			writeLimit(context, "LCD average FIFO drain us",
			    lcd.drainTicks / (24U * lcd.chunks));
		}

		if (lcd.waitTicks != 0U) {
			writeLimit(context, "LCD transfer KiB/s",
			    (lcd.bytes * 24000000U) / (1024U * lcd.waitTicks));
		}
	#endif
		return 0;
	}
#endif

	if (!view.empty()) {
		context.writeErrorLine("mem: unknown memory view");
		return 1;
	}

	auto const memory = lexxos::kernel::memorySnapshot();
	if (!memory.available) {
		context.writeLine("memory statistics are not available on this backend");
		return 0;
	}

	context.write("free heap: ");
	writeUnsigned(context, memory.freeHeapBytes);
	context.writeLine(" bytes");
	context.write("minimum ever free heap: ");
	writeUnsigned(context, memory.minimumEverFreeHeapBytes);
	context.writeLine(" bytes");

	return 0;
}

} // namespace lexxos::apps::system_programs
