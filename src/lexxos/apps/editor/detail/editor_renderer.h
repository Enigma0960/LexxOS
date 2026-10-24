#pragma once

#include <string_view>

#include <lexxos/apps/editor/detail/editor_screen_metrics.h>
#include <lexxos/apps/editor/detail/text_editor_types.h>

namespace lexxos::io {
class OutputStream;
} // namespace lexxos::io

namespace lexxos::apps::text_editor_programs::detail {

class EditorRenderer final {
public:
	static void render(lexxos::io::OutputStream& output, std::string_view path,
	    TextBuffer const& buffer, EditorState& state, EditorScreenMetrics const& metrics);
	static void writeExitLine(lexxos::io::OutputStream& output);
};

} // namespace lexxos::apps::text_editor_programs::detail
