#include <lexxos/os/operating_system.h>
#include <lexxos/platform/platform_services.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/program/program_supervisor.h>

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

void expect(bool condition, char const* message) {
	if (!condition) {
		std::cerr << "operating system contract test failed: " << message << '\n';
		std::exit(1);
	}
}

int g_leftConsoleWrites = 0;
int g_rightConsoleWrites = 0;
int g_supervisedProgramRuns = 0;
int g_backgroundProgramRuns = 0;

void writeLeftConsole(std::string_view) {
	++g_leftConsoleWrites;
}

void writeRightConsole(std::string_view) {
	++g_rightConsoleWrites;
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
}

} // namespace

int main() {
	checkIndependentProgramRegistries();
	checkTaskProfileLaunchesThroughSupervisor();
	checkGraphicalTaskProfilePreservesFocusInterface();
	checkBackgroundTaskDoesNotTakeFocus();
	checkTaskProfileReceivesMemoryArena();
	checkDisabledProgramsStayRegisteredButUnavailable();
	checkPlatformServicesAreInstanceScoped();
	return 0;
}
