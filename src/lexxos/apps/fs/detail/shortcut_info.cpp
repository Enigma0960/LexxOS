#include <lexxos/apps/fs/detail/shortcut_info.h>

#include <lexxos/apps/fs/detail/info_output.h>
#include <lexxos/os/app_context.h>
#include <lexxos/shortcuts/shortcut.h>

namespace lexxos::apps::filesystem_programs::detail {

int inspectShortcutInfo(os::AppContext& context, std::string_view path) {
	shortcuts::Shortcut shortcut{};
	auto const status = shortcuts::load(context.vfs(), path, shortcut);
	if (status == shortcuts::Status::Ok) {
		writeInfoLabel(context, "type", "shortcut");
		writeInfoLabel(context, "target", shortcut.target.data());
		return 0;
	}

	writeInfoLabel(context, "type", "shortcut (invalid)");
	writeInfoLabel(context, "shortcut status", shortcuts::statusText(status));
	return 1;
}

} // namespace lexxos::apps::filesystem_programs::detail
