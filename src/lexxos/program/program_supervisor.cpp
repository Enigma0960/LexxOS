#include <lexxos/program/program_supervisor.h>

#include <array>
#include <atomic>

#if defined(LEXXOS_PLATFORM_HOST) || defined(LEXXOS_PLATFORM_SIM)
	#include <mutex>
#endif

#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/cancellation_token.h>
#include <lexxos/program/detail/program_supervisor_process.h>
#include <lexxos/program/program_manager.h>

#if defined(LEXXOS_KERNEL_FREERTOS)
	#include <FreeRTOS.h>
	#include <task.h>
#endif

namespace lexxos::program {
namespace {

	using ProcessSlot = lexxos::program::detail::ProgramProcessSlot;

	std::array<ProcessSlot, ProgramSupervisor::maxProcesses> g_processes{};
#if defined(LEXXOS_PLATFORM_HOST) || defined(LEXXOS_PLATFORM_SIM)
	std::mutex g_processTableLock;
#endif
	ProgramProcessId g_nextProcessId{1};
	ProgramProcessId g_focusedProcessId{0};

	enum class ProcessStartMode : std::uint8_t {
		Foreground,
		Background,
	};

	class ProcessTableGuard final {
	public:
		ProcessTableGuard() {
#if defined(LEXXOS_KERNEL_FREERTOS)
			taskENTER_CRITICAL();
#elif defined(LEXXOS_PLATFORM_HOST) || defined(LEXXOS_PLATFORM_SIM)
			g_processTableLock.lock();
#endif
		}

		ProcessTableGuard(ProcessTableGuard const&) = delete;
		ProcessTableGuard& operator=(ProcessTableGuard const&) = delete;

		~ProcessTableGuard() {
#if defined(LEXXOS_KERNEL_FREERTOS)
			taskEXIT_CRITICAL();
#elif defined(LEXXOS_PLATFORM_HOST) || defined(LEXXOS_PLATFORM_SIM)
			g_processTableLock.unlock();
#endif
		}
	};

	ProgramProcessState loadState(ProcessSlot const& slot) {
		return slot.state.load(std::memory_order_acquire);
	}

	void storeState(ProcessSlot& slot, ProgramProcessState state) {
		slot.state.store(state, std::memory_order_release);
	}

	ProcessSlot* findSlotById(ProgramProcessId id) {
		for (auto& slot : g_processes) {
			if (slot.id == id && loadState(slot) != ProgramProcessState::Empty) {
				return &slot;
			}
		}
		return nullptr;
	}

	ProcessSlot* allocateSlot() {
		for (auto& slot : g_processes) {
			if (loadState(slot) == ProgramProcessState::Empty) {
				storeState(slot, ProgramProcessState::Starting);
				return &slot;
			}
		}

		ProcessSlot* oldestFinishedSlot = nullptr;
		for (auto& slot : g_processes) {
			if (loadState(slot) != ProgramProcessState::Finished) {
				continue;
			}
			if (oldestFinishedSlot == nullptr || slot.id < oldestFinishedSlot->id) {
				oldestFinishedSlot = &slot;
			}
		}

		if (oldestFinishedSlot != nullptr) {
			storeState(*oldestFinishedSlot, ProgramProcessState::Starting);
		}

		return oldestFinishedSlot;
	}

	ProgramProcessId nextProcessId() {
		auto const id = g_nextProcessId++;
		if (g_nextProcessId == 0) {
			g_nextProcessId = 1;
		}
		return id;
	}

	std::uint32_t alignStackSize(std::uint32_t stackSizeBytes) {
		constexpr std::uint32_t alignment = 256;
		return (stackSizeBytes + alignment - 1U) & ~(alignment - 1U);
	}

	std::uint32_t effectiveStackSize(ProgramRuntimeProfile const& runtime) {
		auto stackSizeBytes = runtime.stackSizeBytes;
		auto const minimumStackSize = runtime.launchMode == ProgramLaunchMode::Inline
		    ? ProgramSupervisor::inlineTaskStackBytes
		    : ProgramSupervisor::minimumTaskStackBytes;
		if (stackSizeBytes < minimumStackSize) {
			stackSizeBytes = minimumStackSize;
		}
		return alignStackSize(stackSizeBytes);
	}

	void* resetSlot(ProcessSlot& slot) {
		auto* const detachedMemory = slot.memoryArena.detachInternalStorage();
		if (g_focusedProcessId == slot.id) {
			g_focusedProcessId = 0;
		}

		slot.id = 0;
		slot.arguments.clear();
		slot.request = ProgramStartRequest{};
		slot.runtime = ProgramRuntimeProfile{};
		slot.result = ProgramExitResult{};
		slot.stackFreeBytes = 0;
		slot.cancellation.reset();
		storeState(slot, ProgramProcessState::Empty);
		return detachedMemory;
	}

	void processTask(void* userData) {
		auto& slot = *static_cast<ProcessSlot*>(userData);
		if (loadState(slot) != ProgramProcessState::StopRequested) {
			storeState(slot, ProgramProcessState::Running);
		}
		slot.result = slot.context.launch(slot.request);
#if defined(LEXXOS_KERNEL_FREERTOS)
		slot.stackFreeBytes = static_cast<std::uint32_t>(uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t));
#else
		slot.stackFreeBytes = 0;
#endif
		void* detachedMemory = nullptr;
		{
			ProcessTableGuard guard;
			detachedMemory = slot.memoryArena.detachInternalStorage();
			if (g_focusedProcessId == slot.id) {
				g_focusedProcessId = 0;
			}
			storeState(slot, ProgramProcessState::Finished);
		}
		ProgramMemoryArena::freeDetachedInternalStorage(detachedMemory);
	}

	ProgramProcessSnapshot snapshotOf(ProcessSlot const& slot) {
		auto const memory = slot.memoryArena.snapshot();
		return {
		    .id = slot.id,
		    .state = loadState(slot),
		    .name = slot.request.name,
		    .launchStatus = slot.result.launchStatus,
		    .exitCode = slot.result.exitCode,
		    .stackSizeBytes = slot.runtime.stackSizeBytes,
		    .stackFreeBytes = slot.stackFreeBytes,
		    .priority = slot.runtime.priority,
		    .launchMode = slot.runtime.launchMode,
		    .memoryClass = slot.runtime.memoryClass,
		    .externalMemoryQuotaBytes = slot.runtime.externalMemoryQuotaBytes,
		    .externalMemoryReservedBytes = memory.reservedExternalBytes,
		    .internalMemoryQuotaBytes = slot.runtime.internalMemoryQuotaBytes,
		    .internalMemoryReservedBytes = memory.reservedInternalBytes,
		    .memoryArenaStatus = memory.status,
		    .interfaceKind = slot.runtime.interfaceKind,
		};
	}

	ProgramSupervisorResult failed(ProgramSupervisorStatus status) {
		return {
		    .status = status,
		    .id = 0,
		    .finalState = ProgramProcessState::Empty,
		    .exitResult = {
		                   .launchStatus = ProgramLaunchStatus::InvalidRequest,
		                   .exitCode = 1,
		                   },
		};
	}

	bool isActiveState(ProgramProcessState state) {
		return state == ProgramProcessState::Starting
		    || state == ProgramProcessState::Running
		    || state == ProgramProcessState::StopRequested;
	}

	bool isFocusableProcess(ProcessSlot const& slot) {
		return isActiveState(loadState(slot)) && slot.runtime.launchMode == ProgramLaunchMode::ForegroundTask;
	}

	std::size_t collectFocusableProcessIds(std::array<ProgramProcessId, ProgramSupervisor::maxProcesses>& ids) {
		std::size_t count = 0;
		for (auto const& slot : g_processes) {
			if (!isFocusableProcess(slot)) {
				continue;
			}
			ids[count++] = slot.id;
		}
		return count;
	}

	ProcessSlot const* findFocusableSlotById(ProgramProcessId id) {
		if (id == 0) {
			return nullptr;
		}
		for (auto const& slot : g_processes) {
			if (slot.id == id && isFocusableProcess(slot)) {
				return &slot;
			}
		}
		return nullptr;
	}

	ProgramFocusSnapshot focusSnapshotLocked() {
		std::array<ProgramProcessId, ProgramSupervisor::maxProcesses> activeIds{};
		auto const activeCount = collectFocusableProcessIds(activeIds);
		if (activeCount == 0) {
			g_focusedProcessId = 0;
			return {.target = ProgramFocusTarget::Shell, .processId = 0, .processName = {}, .activeProgramCount = 0};
		}

		if (auto const* focused = findFocusableSlotById(g_focusedProcessId); focused != nullptr) {
			return {
			    .target = ProgramFocusTarget::Program,
			    .processId = focused->id,
			    .processName = focused->request.name,
			    .activeProgramCount = activeCount,
			    .interfaceKind = focused->runtime.interfaceKind,
			};
		}

		g_focusedProcessId = 0;
		return {.target = ProgramFocusTarget::Shell, .processId = 0, .processName = {}, .activeProgramCount = activeCount};
	}

	bool hasActiveProcessNamed(std::string_view name) {
		for (auto const& slot : g_processes) {
			if (!isActiveState(loadState(slot))) {
				continue;
			}
			if (slot.request.name == name) {
				return true;
			}
		}

		return false;
	}

	ProgramSupervisorStatus validateStartRequest(
	    lexxos::os::AppContext& parentContext,
	    ProgramStartRequest const& request,
	    ProcessStartMode startMode,
	    ProgramDescriptor const*& descriptor) {
		if (request.name.empty() || request.argc <= 0 || request.argv == nullptr) {
			return ProgramSupervisorStatus::InvalidRequest;
		}

		descriptor = parentContext.programs().find(request.name);
		if (descriptor == nullptr) {
			return ProgramSupervisorStatus::NotFound;
		}

		if (!descriptor->enabled) {
			return ProgramSupervisorStatus::Disabled;
		}

		if (startMode == ProcessStartMode::Background && !descriptor->runtime.allowBackground) {
			return ProgramSupervisorStatus::BackgroundNotAllowed;
		}

		if (hasActiveProcessNamed(request.name)) {
			return ProgramSupervisorStatus::AlreadyRunning;
		}

		return ProgramSupervisorStatus::Started;
	}

	ProgramSupervisorStatus prepareSlot(
	    ProcessSlot& slot,
	    lexxos::os::AppContext& parentContext,
	    ProgramStartRequest const& request,
	    ProgramDescriptor const& descriptor,
	    ProcessStartMode startMode,
	    void*& detachedMemory) {
		if (!slot.arguments.assign(request.argc, request.argv)) {
			return ProgramSupervisorStatus::ArgumentsTooLarge;
		}

		slot.id = nextProcessId();
		slot.runtime = descriptor.runtime;
		detachedMemory = slot.memoryArena.detachInternalStorage();
		if (startMode == ProcessStartMode::Background) {
			slot.runtime.launchMode = ProgramLaunchMode::BackgroundTask;
		}
		slot.runtime.stackSizeBytes = effectiveStackSize(slot.runtime);
		slot.result = ProgramExitResult{};
		slot.stackFreeBytes = 0;
		slot.cancellation.reset();
		slot.context = parentContext.createChild({
		    .owner = lexxos::os::AppContextOwner::Program,
		    .processId = slot.id,
		    .shell = parentContext.shell(),
		    .workingDirectory = parentContext.workingDirectory(),
		    .standardInput = request.standardInput,
		    .standardOutput = request.standardOutput,
		    .standardError = request.standardError,
		    .cancellationToken = &slot.cancellation,
		    .memoryArena = &slot.memoryArena,
		});
		slot.request = ProgramStartRequest{
		    .name = slot.arguments.argv[0],
		    .argc = slot.arguments.argc,
		    .argv = slot.arguments.argv.data(),
		    .standardInput = nullptr,
		    .standardOutput = nullptr,
		    .standardError = nullptr,
		    .cancellationToken = &slot.cancellation,
		};
		return ProgramSupervisorStatus::Started;
	}

	ProgramSupervisorStatus reserveProcessMemory(ProcessSlot& slot) {
		auto const status = slot.memoryArena.reserve(slot.runtime);
		if (status == ProgramMemoryArenaStatus::Ready) {
			return ProgramSupervisorStatus::Started;
		}
		if (status == ProgramMemoryArenaStatus::ExternalUnavailable) {
			return ProgramSupervisorStatus::ExternalMemoryUnavailable;
		}
		return ProgramSupervisorStatus::MemoryUnavailable;
	}

	bool createProcessTask(ProcessSlot& slot) {
		return lexxos::kernel::createTask(processTask, &slot, {
		                                                          .name = slot.arguments.argv[0],
		                                                          .stackSizeBytes = slot.runtime.stackSizeBytes,
		                                                          .priority = slot.runtime.priority,
		                                                          .executionClass = lexxos::kernel::TaskExecutionClass::Program,
		                                                          .coreAffinity = slot.runtime.coreAffinity,
		                                                      });
	}

	ProgramSupervisorResult startProcess(lexxos::os::AppContext& parentContext, ProgramStartRequest const& request, ProcessStartMode startMode) {
		ProgramDescriptor const* descriptor = nullptr;
		ProcessSlot* slot = nullptr;
		void* detachedMemory = nullptr;
		void* resetMemory = nullptr;
		ProgramSupervisorStatus deferredFailure = ProgramSupervisorStatus::Started;

		{
			ProcessTableGuard guard;
			auto const validationStatus = validateStartRequest(parentContext, request, startMode, descriptor);
			if (validationStatus != ProgramSupervisorStatus::Started || descriptor == nullptr) {
				return failed(validationStatus);
			}

			slot = allocateSlot();
			if (slot == nullptr) {
				return failed(ProgramSupervisorStatus::TooManyProcesses);
			}

			auto const prepareStatus = prepareSlot(*slot, parentContext, request, *descriptor, startMode, detachedMemory);
			if (prepareStatus != ProgramSupervisorStatus::Started) {
				resetMemory = resetSlot(*slot);
				deferredFailure = prepareStatus;
			} else if (startMode == ProcessStartMode::Foreground && isFocusableProcess(*slot)) {
				// allocateSlot() reserves a table entry, but prepareSlot() assigns the real process id
				// and runtime profile. Select the foreground task only after those fields are valid.
				g_focusedProcessId = slot->id;
			}
		}
		ProgramMemoryArena::freeDetachedInternalStorage(detachedMemory);
		ProgramMemoryArena::freeDetachedInternalStorage(resetMemory);

		if (deferredFailure != ProgramSupervisorStatus::Started) {
			return failed(deferredFailure);
		}

		auto const memoryStatus = reserveProcessMemory(*slot);
		if (memoryStatus != ProgramSupervisorStatus::Started) {
			void* failedStartMemory = nullptr;
			{
				ProcessTableGuard guard;
				failedStartMemory = resetSlot(*slot);
			}
			ProgramMemoryArena::freeDetachedInternalStorage(failedStartMemory);
			return failed(memoryStatus);
		}

		if (!createProcessTask(*slot)) {
			void* failedStartMemory = nullptr;
			{
				ProcessTableGuard guard;
				failedStartMemory = resetSlot(*slot);
			}
			ProgramMemoryArena::freeDetachedInternalStorage(failedStartMemory);
			return failed(ProgramSupervisorStatus::TaskCreateFailed);
		}

		ProgramProcessState finalState = ProgramProcessState::Starting;
		ProgramExitResult exitResult{};
		{
			ProcessTableGuard guard;
			finalState = loadState(*slot);
			if (finalState == ProgramProcessState::Finished) {
				exitResult = slot->result;
			}
		}

		return {
		    .status = ProgramSupervisorStatus::Started,
		    .id = slot->id,
		    .finalState = finalState,
		    .exitResult = exitResult,
		};
	}

} // namespace

std::string_view toString(ProgramProcessState state) {
	switch (state) {
		case ProgramProcessState::Empty:
			return "empty";
		case ProgramProcessState::Starting:
			return "starting";
		case ProgramProcessState::Running:
			return "running";
		case ProgramProcessState::StopRequested:
			return "stop-requested";
		case ProgramProcessState::Finished:
			return "finished";
	}
	return "unknown";
}

std::string_view toString(ProgramSupervisorStatus status) {
	switch (status) {
		case ProgramSupervisorStatus::Started:
			return "started";
		case ProgramSupervisorStatus::InvalidRequest:
			return "invalid-request";
		case ProgramSupervisorStatus::NotFound:
			return "not-found";
		case ProgramSupervisorStatus::Disabled:
			return "disabled";
		case ProgramSupervisorStatus::BackgroundNotAllowed:
			return "background-not-allowed";
		case ProgramSupervisorStatus::AlreadyRunning:
			return "already-running";
		case ProgramSupervisorStatus::TooManyProcesses:
			return "too-many-processes";
		case ProgramSupervisorStatus::ArgumentsTooLarge:
			return "arguments-too-large";
		case ProgramSupervisorStatus::ExternalMemoryUnavailable:
			return "external-memory-unavailable";
		case ProgramSupervisorStatus::MemoryUnavailable:
			return "memory-unavailable";
		case ProgramSupervisorStatus::TaskCreateFailed:
			return "task-create-failed";
	}
	return "unknown";
}

ProgramSupervisorResult ProgramSupervisor::startForeground(
    lexxos::os::AppContext& parentContext,
    ProgramStartRequest const& request) {
	auto result = startProcess(parentContext, request, ProcessStartMode::Foreground);
	return result;
}

ProgramSupervisorResult ProgramSupervisor::startBackground(
    lexxos::os::AppContext& parentContext,
    ProgramStartRequest const& request) {
	auto result = startProcess(parentContext, request, ProcessStartMode::Background);
	if (result.started()) {
		result.finalState = ProgramProcessState::Starting;
	}
	return result;
}

ProgramFocusSnapshot ProgramSupervisor::focus() {
	ProcessTableGuard guard;
	return focusSnapshotLocked();
}

ProgramFocusSnapshot ProgramSupervisor::focusNext() {
	ProcessTableGuard guard;

	std::array<ProgramProcessId, ProgramSupervisor::maxProcesses> activeIds{};
	auto const activeCount = collectFocusableProcessIds(activeIds);
	if (activeCount == 0) {
		g_focusedProcessId = 0;
		return {.target = ProgramFocusTarget::Shell, .processId = 0, .processName = {}, .activeProgramCount = 0};
	}

	if (g_focusedProcessId == 0) {
		g_focusedProcessId = activeIds[0];
		return focusSnapshotLocked();
	}

	for (std::size_t index = 0; index < activeCount; ++index) {
		if (activeIds[index] != g_focusedProcessId) {
			continue;
		}

		g_focusedProcessId = (index + 1U < activeCount) ? activeIds[index + 1U] : 0;
		return focusSnapshotLocked();
	}

	g_focusedProcessId = activeIds[0];
	return focusSnapshotLocked();
}

bool ProgramSupervisor::processFocused(ProgramProcessId id) {
	if (id == 0) {
		return false;
	}
	ProcessTableGuard guard;
	auto const snapshot = focusSnapshotLocked();
	return snapshot.target == ProgramFocusTarget::Program && snapshot.processId == id;
}

void ProgramSupervisor::focusShell() {
	ProcessTableGuard guard;
	g_focusedProcessId = 0;
}

bool ProgramSupervisor::requestStop(ProgramProcessId id) {
	ProcessTableGuard guard;
	auto* const slot = findSlotById(id);
	if (slot == nullptr) {
		return false;
	}

	auto const state = loadState(*slot);
	if (state != ProgramProcessState::Starting && state != ProgramProcessState::Running && state != ProgramProcessState::StopRequested) {
		return false;
	}

	slot->cancellation.request();
	storeState(*slot, ProgramProcessState::StopRequested);
	return true;
}

std::size_t ProgramSupervisor::clearFinished() {
	std::size_t cleared = 0;
	std::array<void*, ProgramSupervisor::maxProcesses> detachedMemory{};
	{
		ProcessTableGuard guard;
		for (auto& slot : g_processes) {
			if (loadState(slot) != ProgramProcessState::Finished) {
				continue;
			}

			detachedMemory[cleared] = resetSlot(slot);
			++cleared;
		}
	}

	for (std::size_t index = 0; index < cleared; ++index) {
		ProgramMemoryArena::freeDetachedInternalStorage(detachedMemory[index]);
	}
	return cleared;
}

void ProgramSupervisor::visitProcesses(ProgramProcessVisitor visitor, void* userData, bool& any) const {
	if (visitor == nullptr) {
		return;
	}

	std::array<ProgramProcessSnapshot, ProgramSupervisor::maxProcesses> snapshots{};
	std::size_t snapshotCount = 0;
	{
		ProcessTableGuard guard;
		for (auto const& slot : g_processes) {
			if (loadState(slot) == ProgramProcessState::Empty) {
				continue;
			}
			snapshots[snapshotCount++] = snapshotOf(slot);
		}
	}

	for (std::size_t index = 0; index < snapshotCount; ++index) {
		any = true;
		visitor(snapshots[index], userData);
	}
}

ProgramSupervisor& supervisor() {
	static ProgramSupervisor instance;
	return instance;
}

} // namespace lexxos::program
