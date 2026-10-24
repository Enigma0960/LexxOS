#pragma once

#include <string_view>

#include <lexxos/apps/editor/detail/text_editor_types.h>

namespace lexxos::os {
class AppContext;
} // namespace lexxos::os

namespace lexxos::apps::text_editor_programs::detail {

class EditorController final {
public:
	static int run(lexxos::os::AppContext& context, std::string_view path,
	    TextBuffer& buffer, EditorState& state);
};

} // namespace lexxos::apps::text_editor_programs::detail
