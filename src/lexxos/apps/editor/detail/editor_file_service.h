#pragma once

#include <string_view>

#include <lexxos/apps/editor/detail/text_editor_types.h>

namespace lexxos::os {
class AppContext;
} // namespace lexxos::os

namespace lexxos::apps::text_editor_programs::detail {

class EditorFileService final {
public:
	[[nodiscard]] static bool load(lexxos::os::AppContext& context, std::string_view path,
	    TextBuffer& buffer, bool& createdNewFile);
	[[nodiscard]] static bool save(lexxos::os::AppContext& context, std::string_view path,
	    TextBuffer const& buffer);
};

} // namespace lexxos::apps::text_editor_programs::detail
