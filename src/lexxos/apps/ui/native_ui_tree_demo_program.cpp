#include <lexxos/apps/ui/native_ui_tree_demo_program.h>

#include <lexxos/os/app_context.h>
#include <lexxos/ui/native_transport.h>
#include <ui-demo/shared_tree_demo.h>

namespace lexxos::apps::ui_programs {

program::ProgramDescriptor NativeUiTreeDemoProgram::descriptor() {
	return {
	    .name = "ui-native-tree-demo",
	    .help = "run native nested LexxUI tree demo",
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

int NativeUiTreeDemoProgram::run(os::AppContext& context,
    program::ProgramInvocation const&, program::ParsedArguments const&) {
	ui::NativeTransport native{context};
	lexxos::ui_demo::TreeDemo demo{native.transport()};
	return demo.run();
}

} // namespace lexxos::apps::ui_programs
