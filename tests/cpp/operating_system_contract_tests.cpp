#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include <lexxos/apps/fs/detail/raw_console_mode_guard.h>
#include <lexxos/apps/fs/detail/ymodem_program_support.h>
#include <lexxos/os/operating_system.h>
#include <lexxos/platform/platform_services.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/program/program_supervisor.h>

namespace lexxos::apps {

std::uint32_t uptimeMs(lexxos::os::AppContext const& context) {
	auto const& clock = context.platform().clock;
	return clock.uptimeMs != nullptr ? clock.uptimeMs() : 0U;
}

void sleepMs(lexxos::os::AppContext const& context, std::uint32_t milliseconds) {
	auto const& clock = context.platform().clock;
	if (clock.sleepMs != nullptr) {
		clock.sleepMs(milliseconds);
	}
}

} // namespace lexxos::apps

namespace {

void expect(bool condition, char const* message) {
	if (!condition) {
		std::cerr << "operating system contract test failed: " << message << '\n';
		std::exit(1);
	}
}

int g_leftConsoleWrites = 0;
int g_rightConsoleWrites = 0;
std::string g_leftConsoleRawBytes;
int g_supervisedProgramRuns = 0;
std::uint32_t g_supervisedProcessId = 0;
int g_backgroundProgramRuns = 0;

void writeLeftConsole(std::string_view) {
	++g_leftConsoleWrites;
}

void writeRightConsole(std::string_view) {
	++g_rightConsoleWrites;
}

void writeLeftRawConsole(std::string_view bytes) {
	g_leftConsoleRawBytes.append(bytes);
}

int leftOnlyProgram(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	expect(context.hasOperatingSystem(), "left program receives an operating system context");
	expect(context.programs().find("left-only") != nullptr, "left program sees left registry");
	expect(context.programs().find("right-only") == nullptr, "left program does not see right registry");
	return 17;
}

int rightOnlyProgram(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	expect(context.hasOperatingSystem(), "right program receives an operating system context");
	expect(context.programs().find("right-only") != nullptr, "right program sees right registry");
	expect(context.programs().find("left-only") == nullptr, "right program does not see left registry");
	return 23;
}

int supervisedProgram(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	expect(context.owner() == lexxos::os::AppContextOwner::Program, "task-profile program runs in a program context");
	g_supervisedProcessId = context.processId();
	expect(context.programInput().setInputMode(g_supervisedProcessId, lexxos::app::program_input::InputMode::Events), "program claims events mode");
	++g_supervisedProgramRuns;
	return 31;
}

int graphicalProgram(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	expect(context.owner() == lexxos::os::AppContextOwner::Program, "graphical task runs in a program context");
	auto const focus = lexxos::program::supervisor().focus();
	expect(focus.programFocused(), "graphical task is focused while running");
	expect(focus.graphicalProgramFocused(), "graphical task focus reports graphical interface");
	expect(focus.interfaceKind == lexxos::program::ProgramInterfaceKind::Graphical,
	    "graphical task focus preserves interface kind");
	return 41;
}

int backgroundProgram(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	expect(context.owner() == lexxos::os::AppContextOwner::Program, "background task runs in a program context");
	auto const focus = lexxos::program::supervisor().focus();
	expect(!focus.programFocused(), "background task does not own terminal focus while running");
	expect(!focus.graphicalProgramFocused(), "background graphical task does not switch graphical focus");
	++g_backgroundProgramRuns;
	return 43;
}

int arenaProgram(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	auto* const arena = context.memoryArena();
	expect(arena != nullptr, "task-profile program receives process memory arena");
	auto* const memory = arena->allocate(32, 8);
	expect(memory != nullptr, "process memory arena allocates internal workspace");
	return 37;
}

void checkIndependentProgramRegistries() {
	lexxos::os::OperatingSystem left;
	lexxos::os::OperatingSystem right;

	expect(left.programs().registerProgram({
	           .name = "left-only",
	           .help = "left test program",
	           .handler = leftOnlyProgram,
	       }),
	    "register left-only program");
	expect(right.programs().registerProgram({
	           .name = "right-only",
	           .help = "right test program",
	           .handler = rightOnlyProgram,
	       }),
	    "register right-only program");

	auto leftContext = left.createAppContext({});
	auto rightContext = right.createAppContext({});
	char const* leftArgv[] = {"left-only"};
	char const* rightArgv[] = {"right-only"};

	expect(leftContext.hasOperatingSystem(), "left context is bound to an operating system");
	expect(rightContext.hasOperatingSystem(), "right context is bound to an operating system");
	expect(&leftContext.devices() != &rightContext.devices(), "contexts expose independent device managers");
	expect(leftContext.canLaunch("left-only") == lexxos::program::ProgramLaunchStatus::Available,
	    "left context can launch left-only");
	expect(leftContext.canLaunch("right-only") == lexxos::program::ProgramLaunchStatus::NotFound,
	    "left context cannot launch right-only");
	expect(rightContext.canLaunch("right-only") == lexxos::program::ProgramLaunchStatus::Available,
	    "right context can launch right-only");
	expect(rightContext.canLaunch("left-only") == lexxos::program::ProgramLaunchStatus::NotFound,
	    "right context cannot launch left-only");

	auto const leftResult = leftContext.launch({
	    .name = "left-only",
	    .argc = 1,
	    .argv = leftArgv,
	});
	expect(leftResult.launchStatus == lexxos::program::ProgramLaunchStatus::Available, "left launch is available");
	expect(leftResult.exitCode == 17, "left launch returns handler exit code");

	auto const rightResult = rightContext.launch({
	    .name = "right-only",
	    .argc = 1,
	    .argv = rightArgv,
	});
	expect(rightResult.launchStatus == lexxos::program::ProgramLaunchStatus::Available, "right launch is available");
	expect(rightResult.exitCode == 23, "right launch returns handler exit code");
}

void checkTaskProfileLaunchesThroughSupervisor() {
	lexxos::os::OperatingSystem os;
	expect(os.programs().registerProgram({
	           .name = "supervised",
	           .help = "task-profile test program",
	           .handler = supervisedProgram,
	           .runtime = {
	                       .launchMode = lexxos::program::ProgramLaunchMode::ForegroundTask,
	                       },
	}),
	    "register supervised program");

	auto context = os.createAppContext({
	    .owner = lexxos::os::AppContextOwner::Shell,
	});
	char const* argv[] = {"supervised"};
	g_supervisedProgramRuns = 0;
	auto const result = context.launch({
	    .name = "supervised",
	    .argc = 1,
	    .argv = argv,
	});
	expect(result.launchStatus == lexxos::program::ProgramLaunchStatus::Available, "supervised launch is available");
	expect(result.exitCode == 31, "supervised launch returns handler exit code");
	expect(g_supervisedProgramRuns == 1, "supervised handler ran once");
	expect(context.programInput().inputMode(g_supervisedProcessId) == lexxos::app::program_input::InputMode::Terminal, "task completion releases input mode");
}

void checkGraphicalTaskProfilePreservesFocusInterface() {
	lexxos::program::supervisor().clearFinished();
	lexxos::os::OperatingSystem os;
	expect(os.programs().registerProgram({
	           .name = "graphical",
	           .help = "graphical task-profile test program",
	           .handler = graphicalProgram,
	           .runtime = {
	                       .launchMode = lexxos::program::ProgramLaunchMode::ForegroundTask,
	                       .interfaceKind = lexxos::program::ProgramInterfaceKind::Graphical,
	                       },
	}),
	    "register graphical program");

	auto context = os.createAppContext({
	    .owner = lexxos::os::AppContextOwner::Shell,
	});
	char const* argv[] = {"graphical"};
	auto const result = context.launch({
	    .name = "graphical",
	    .argc = 1,
	    .argv = argv,
	});
	expect(result.launchStatus == lexxos::program::ProgramLaunchStatus::Available, "graphical launch is available");
	expect(result.exitCode == 41, "graphical launch returns handler exit code");
	lexxos::program::supervisor().clearFinished();
}

void checkBackgroundTaskDoesNotTakeFocus() {
	lexxos::program::supervisor().clearFinished();
	lexxos::os::OperatingSystem os;
	expect(os.programs().registerProgram({
	           .name = "background",
	           .help = "background task-profile test program",
	           .handler = backgroundProgram,
	           .runtime = {
	                       .launchMode = lexxos::program::ProgramLaunchMode::ForegroundTask,
	                       .allowBackground = true,
	                       .interfaceKind = lexxos::program::ProgramInterfaceKind::Graphical,
	                       },
	}),
	    "register background program");

	auto context = os.createAppContext({
	    .owner = lexxos::os::AppContextOwner::Shell,
	});
	char const* argv[] = {"background"};
	g_backgroundProgramRuns = 0;
	auto const result = lexxos::program::supervisor().startBackground(context, {
	                                                                               .name = "background",
	                                                                               .argc = 1,
	                                                                               .argv = argv,
	                                                                           });
	expect(result.started(), "background launch starts");
	expect(g_backgroundProgramRuns == 1, "background handler ran once");
	auto const focus = lexxos::program::supervisor().focus();
	expect(!focus.programFocused(), "background launch leaves terminal focus on shell");
	lexxos::program::supervisor().clearFinished();
}

void checkTaskProfileReceivesMemoryArena() {
	lexxos::os::OperatingSystem os;
	expect(os.programs().registerProgram({
	           .name = "arena",
	           .help = "arena test program",
	           .handler = arenaProgram,
	           .runtime = {
	                       .launchMode = lexxos::program::ProgramLaunchMode::ForegroundTask,
	                       .internalMemoryQuotaBytes = 64,
	                       },
	}),
	    "register arena program");

	auto context = os.createAppContext({
	    .owner = lexxos::os::AppContextOwner::Shell,
	});
	char const* argv[] = {"arena"};
	auto const result = context.launch({
	    .name = "arena",
	    .argc = 1,
	    .argv = argv,
	});
	expect(result.launchStatus == lexxos::program::ProgramLaunchStatus::Available, "arena launch is available");
	expect(result.exitCode == 37, "arena launch returns handler exit code");
}

void checkDisabledProgramsStayRegisteredButUnavailable() {
	lexxos::os::OperatingSystem os;
	expect(os.programs().registerProgram({
	           .name = "disabled",
	           .help = "disabled test program",
	           .handler = supervisedProgram,
	           .enabled = false,
	       }),
	    "register disabled program");

	auto context = os.createAppContext({});
	char const* argv[] = {"disabled"};
	expect(context.programs().find("disabled") != nullptr, "disabled program remains registered");
	expect(context.canLaunch("disabled") == lexxos::program::ProgramLaunchStatus::Disabled,
	    "disabled program is not launchable");

	auto const result = context.launch({
	    .name = "disabled",
	    .argc = 1,
	    .argv = argv,
	});
	expect(result.launchStatus == lexxos::program::ProgramLaunchStatus::Disabled, "disabled launch reports disabled");
	expect(result.exitCode == 126, "disabled launch returns shell-compatible exit code");
}

void checkPlatformServicesAreInstanceScoped() {
	static constexpr lexxos::platform::PlatformServices leftServices{
	    .console = {
	                .write = writeLeftConsole,
	                .writeRaw = writeLeftRawConsole,
	                },
	};
	static constexpr lexxos::platform::PlatformServices rightServices{
	    .console = {
	                .write = writeRightConsole,
	                },
	};

	lexxos::os::OperatingSystem left{leftServices};
	lexxos::os::OperatingSystem right{rightServices};

	auto leftContext = left.createAppContext({});
	auto rightContext = right.createAppContext({});

	expect(&left.platform() == &leftServices, "left operating system keeps injected platform services");
	expect(&right.platform() == &rightServices, "right operating system keeps injected platform services");
	expect(&left.platform() != &right.platform(), "operating systems can use independent platform services");
	expect(&left.runtimeProfile() == &lexxos::runtime::standardRuntimeProfile,
	    "operating system exposes the active runtime profile");
	expect(&leftContext.platform() == &leftServices, "left context exposes injected platform services");
	expect(&rightContext.platform() == &rightServices, "right context exposes injected platform services");

	g_leftConsoleWrites = 0;
	g_rightConsoleWrites = 0;
	leftContext.write("left");
	expect(g_leftConsoleWrites == 1, "left context writes to left console service");
	expect(g_rightConsoleWrites == 0, "left context does not write to right console service");

	auto leftChild = leftContext.createChild({});
	leftChild.write("child");
	expect(g_leftConsoleWrites == 2, "left child context writes to left console service");
	expect(g_rightConsoleWrites == 0, "left child context does not write to right console service");

	rightContext.write("right");
	expect(g_leftConsoleWrites == 2, "right context does not write to left console service");
	expect(g_rightConsoleWrites == 1, "right context writes to right console service");

	namespace ymodem = lexxos::apps::filesystem_programs::detail::ymodem;
	std::uint8_t const payload[] = {0x00U, 0x0AU, 0x0DU, 0xFFU};
	g_leftConsoleRawBytes.clear();
	expect(ymodem::writeRaw(leftContext, payload, sizeof(payload)), "YMODEM can write via the binary console");
	expect(g_leftConsoleRawBytes == std::string_view{"\x00\x0a\x0d\xff", 4}, "YMODEM forwards every byte unchanged");
	expect(g_leftConsoleWrites == 2, "YMODEM bypasses the text console when a raw writer is available");
	expect(ymodem::writeRaw(rightContext, payload, sizeof(payload)), "YMODEM supports consoles with one writer");
	expect(g_rightConsoleWrites == 2, "YMODEM falls back to the existing console writer");
}

void checkSharedCoreState() {
	lexxos::os::OperatingSystem system;
	system.initialize();
	auto context = system.core().createContext();
	expect(&system.devices() == &context.devices(), "Standard and core must share devices");
	expect(&system.platform() == &context.platform(), "Standard and core must share platform");
	expect(&context.events() == &lexxos::lifecycle::defaultEventBus(), "Standard lifecycle bus changed");
	expect(context.devices().registerGpioPin("core-output", "mock", {7}, false, true), "core GPIO registration");
	expect(system.devices().gpioPin("core-output") != nullptr, "Standard missed core registration");
	system.refreshCapabilities();
	expect(system.capabilities().bits() == context.capabilities().bits(), "Standard and core capabilities diverged");
	system.initialize();
	expect(context.devices().gpioPinCount() == 0, "Standard reset not reflected in core");
}

void checkIndependentInputServices() {
	lexxos::platform::PlatformServices platform{};
	lexxos::vfs::Vfs leftVfs, rightVfs;
	leftVfs.initialize(platform);
	rightVfs.initialize(platform);
	lexxos::lifecycle::EventBus leftEvents, rightEvents;
	lexxos::app::ProgramInputService leftInput{leftEvents}, rightInput{rightEvents};
	lexxos::app::ConsoleInputService leftConsole, rightConsole;
	lexxos::os::OperatingSystem left{platform, leftVfs, leftInput, leftConsole};
	lexxos::os::OperatingSystem right{platform, rightVfs, rightInput, rightConsole};
	auto leftContext = left.createAppContext({.processId = 7});
	auto rightContext = right.createAppContext({.processId = 7});
	auto child = leftContext.createChild({});
	expect(&child.programInput() == &leftInput, "child inherits input service");
	expect(&left.core().events() == &leftEvents, "core uses injected event bus");
	expect(&right.core().events() == &rightEvents, "other core has independent event bus");
	using lexxos::app::program_input::InputMode;
	expect(leftContext.programInput().setInputMode(7, InputMode::Events), "set left process mode");
	expect(rightContext.programInput().inputMode(7) == InputMode::Terminal, "same process id in another OS is independent");
	lexxos::app::program_input::EventCursor leftCursor{}, rightCursor{}, secondLeftCursor{};
	leftInput.reset(leftCursor);
	leftInput.reset(secondLeftCursor);
	rightInput.reset(rightCursor);
	lexxos::devices::InputEvent event{.type = lexxos::devices::InputEventType::KeyDown,
	    .key = lexxos::devices::KeyCode::A,
	    .timestampMs = 123};
	leftInput.publish(event);
	expect(leftInput.pollEvent(leftCursor, event) && event.timestampMs == 123, "left input receives its event");
	expect(leftInput.pollEvent(secondLeftCursor, event), "independent subscriber receives the same event");
	expect(!rightInput.pollEvent(rightCursor, event), "event does not cross OS boundary");
	lexxos::lifecycle::Event lifecycleEvent{};
	expect(leftEvents.latest(lifecycleEvent) && lifecycleEvent.type == lexxos::lifecycle::EventType::Input,
	    "input is published on injected event bus");
	expect(!rightEvents.latest(lifecycleEvent), "input does not leak to another event bus");
	expect(leftConsole.inject("\x1b[A"), "left escape sequence accepted");
	expect(rightConsole.inject("R"), "right console input accepted");
	leftConsole.setMode(lexxos::app::console_input::Mode::RawProgram);
	expect(rightConsole.mode() == lexxos::app::console_input::Mode::Shell, "raw mode is local to OS");
	leftConsole.setMode(lexxos::app::console_input::Mode::Program);
	{
		lexxos::apps::filesystem_programs::detail::ymodem::RawConsoleModeGuard raw{leftConsole};
		expect(leftConsole.mode() == lexxos::app::console_input::Mode::RawProgram, "guard enters raw mode");
		expect(rightConsole.mode() == lexxos::app::console_input::Mode::Shell, "guard leaves other OS unchanged");
	}
	expect(leftConsole.mode() == lexxos::app::console_input::Mode::Program, "guard restores the owning console mode");
	char byte{};
	expect(rightContext.consoleInput().readChar(platform, byte) && byte == 'R', "right console only receives its byte");
	expect(!rightConsole.readChar(platform, byte), "right input exhausted independently");
	expect(leftConsole.pendingInjected() == 3, "right reads preserve left escape sequence");
	rightConsole.clearInjected();
	expect(leftConsole.pendingInjected() == 3, "right clear preserves left queue");
	leftInput.releaseInputMode(7);
	expect(leftInput.inputMode(7) == InputMode::Terminal, "released process resets mode");
	for (std::uint32_t id = 1; id <= lexxos::runtime::standardRuntimeProfile.programs.maxProcesses; ++id) {
		expect(leftInput.setInputMode(id, InputMode::Events), "fill mode slots");
	}

	expect(!leftInput.setInputMode(999, InputMode::Events), "mode table is bounded");
	leftInput.releaseInputMode(1);
	expect(leftInput.setInputMode(999, InputMode::Events), "released slot can be reused");
	expect(!leftInput.setInputMode(0, InputMode::Events), "shell cannot own a process mode");
	expect(!leftInput.setInputMode(7, static_cast<InputMode>(255)), "invalid mode rejected");
	expect(leftVfs.writeText("/tmp/isolated", "left") == lexxos::vfs::WriteStatus::Ok, "write left VFS");
	lexxos::vfs::Stat stat{};
	expect(!rightVfs.stat("/tmp/isolated", stat), "file does not cross VFS boundary");
}

} // namespace

void checkSdmmcCommand();

void checkBoardDeviceProviders();
void checkInfoProgram();

int main() {
	checkBoardDeviceProviders();
	checkInfoProgram();
	checkSdmmcCommand();
	checkIndependentInputServices();
	checkSharedCoreState();
	checkIndependentProgramRegistries();
	checkTaskProfileLaunchesThroughSupervisor();
	checkGraphicalTaskProfilePreservesFocusInterface();
	checkBackgroundTaskDoesNotTakeFocus();
	checkTaskProfileReceivesMemoryArena();
	checkDisabledProgramsStayRegisteredButUnavailable();
	checkPlatformServicesAreInstanceScoped();
	return 0;
}
