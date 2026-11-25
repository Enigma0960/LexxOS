#include <lexxos/apps/fs/detail/info_output.h>

#include <lexxos/os/app_context.h>

namespace lexxos::apps::filesystem_programs::detail {

using os::AppContext;

void writeInfoLabel(AppContext& context, std::string_view key, std::string_view value) {
	context.write(key);
	context.write(": ");
	context.writeLine(value);
}

} // namespace lexxos::apps::filesystem_programs::detail
