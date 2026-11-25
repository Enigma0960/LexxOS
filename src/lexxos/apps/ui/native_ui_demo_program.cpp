#include <lexxos/apps/ui/native_ui_demo_program.h>

#include <lexxos/os/app_context.h>
#include <lexxos/ui/native_transport.h>
#include <ui-demo/shared_tree_demo.h>
#include <ui-demo/shared_ui_demo.h>

namespace lexxos::apps::ui_programs {

program::ProgramDescriptor NativeUiDemoProgram::descriptor() {
	return {
	    .name = "ui-native-demo",
	    .help = "run unified LexxUI control gallery (same as ui-demo.lexx)",
	    .handler = run,
	    .runtime = {
	                .launchMode = program::ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 8192U,
	                .cancellable = true,
	                .allowBackground = false,
	                .interfaceKind = program::ProgramInterfaceKind::Graphical,
	                },
	    .legacy = false,
	    .enabled = true,
	};
}

int NativeUiDemoProgram::run(os::AppContext& context,
    program::ProgramInvocation const&, program::ParsedArguments const&) {
	ui::NativeTransport native{context};
	lexxos::ui_demo::TreeDemo demo{native.transport()};
	return demo.run();
}

} // namespace lexxos::apps::ui_programs
