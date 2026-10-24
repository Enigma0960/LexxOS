#include <lexxos/apps/editor/detail/editor_controller.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string_view>

#include <lexxos/app/picocalc_lcd_console.h>
#include <lexxos/apps/editor/detail/editor_file_service.h>
#include <lexxos/apps/editor/detail/editor_renderer.h>
#include <lexxos/apps/editor/detail/editor_screen_metrics.h>
#include <lexxos/io/output_stream.h>
#include <lexxos/kernel/system.h>
#include <lexxos/os/app_context.h>
#include <lexxos/terminal/terminal.h>

namespace lexxos::apps::text_editor_programs::detail {
namespace {

	void setStatus(EditorState& state, std::string_view text) {
		auto const length = std::min(text.size(), state.status.size() - 1U);
		if (length != 0) {
			std::memcpy(state.status.data(), text.data(), length);
		}
		state.status[length] = '\0';
	}

	void beep(lexxos::io::OutputStream& output) {
		output.write("\a");
	}

	void clampCursorToBuffer(TextBuffer const& buffer, EditorState& state) {
		if (buffer.lineCount == 0) {
			state.cursorLine = 0;
			state.cursorColumn = 0;
			return;
		}
		if (state.cursorLine >= buffer.lineCount) {
			state.cursorLine = buffer.lineCount - 1U;
		}
		auto const length = buffer.lineLength(state.cursorLine);
		if (state.cursorColumn > length) {
			state.cursorColumn = length;
		}
	}

	void resetStatusOnEdit(EditorState& state) {
		state.confirmQuit = false;
		state.status[0] = '\0';
	}

	bool insertCharacter(TextBuffer& buffer, EditorState& state, char value) {
		resetStatusOnEdit(state);
		if (buffer.insertChar(state.cursorLine, state.cursorColumn, value)) {
			++state.cursorColumn;
			return true;
		}
		return false;
	}

	bool insertTab(TextBuffer& buffer, EditorState& state) {
		for (std::uint8_t index = 0; index < 4; ++index) {
			if (!insertCharacter(buffer, state, ' ')) {
				return false;
			}
		}
		return true;
	}

	void moveLeft(TextBuffer const& buffer, EditorState& state) {
		state.confirmQuit = false;
		state.status[0] = '\0';
		if (state.cursorColumn != 0) {
			--state.cursorColumn;
			return;
		}
		if (state.cursorLine != 0) {
			--state.cursorLine;
			state.cursorColumn = buffer.lineLength(state.cursorLine);
		}
	}

	void moveRight(TextBuffer const& buffer, EditorState& state) {
		state.confirmQuit = false;
		state.status[0] = '\0';
		auto const length = buffer.lineLength(state.cursorLine);
		if (state.cursorColumn < length) {
			++state.cursorColumn;
			return;
		}
		if (state.cursorLine + 1U < buffer.lineCount) {
			++state.cursorLine;
			state.cursorColumn = 0;
		}
	}

	void moveUp(TextBuffer const& buffer, EditorState& state) {
		state.confirmQuit = false;
		state.status[0] = '\0';
		if (state.cursorLine != 0) {
			--state.cursorLine;
			clampCursorToBuffer(buffer, state);
		}
	}

	void moveDown(TextBuffer const& buffer, EditorState& state) {
		state.confirmQuit = false;
		state.status[0] = '\0';
		if (state.cursorLine + 1U < buffer.lineCount) {
			++state.cursorLine;
			clampCursorToBuffer(buffer, state);
		}
	}

	EditorScreenMetrics currentScreenMetrics() {
		auto const info = lexxos::app::picocalc_lcd_console::info();
		if (info.ready && info.textMode && info.rows > 0 && info.columns > 0 && info.rows <= 255 && info.columns <= 255) {
			return makeEditorScreenMetrics(static_cast<std::uint8_t>(info.rows), static_cast<std::uint8_t>(info.columns));
		}
		return defaultEditorScreenMetrics();
	}

	void pageUp(TextBuffer const& buffer, EditorState& state, EditorScreenMetrics const& metrics) {
		state.confirmQuit = false;
		state.status[0] = '\0';
		state.cursorLine = state.cursorLine > metrics.visibleTextRows ? state.cursorLine - metrics.visibleTextRows : 0;
		clampCursorToBuffer(buffer, state);
	}

	void pageDown(TextBuffer const& buffer, EditorState& state, EditorScreenMetrics const& metrics) {
		state.confirmQuit = false;
		state.status[0] = '\0';
		if (buffer.lineCount == 0) {
			return;
		}
		state.cursorLine = std::min(buffer.lineCount - 1U, state.cursorLine + metrics.visibleTextRows);
		clampCursorToBuffer(buffer, state);
	}

	void backspace(TextBuffer& buffer, EditorState& state, lexxos::io::OutputStream& output) {
		resetStatusOnEdit(state);
		if (buffer.lineCount == 0) {
			beep(output);
			return;
		}
		if (state.cursorColumn != 0) {
			--state.cursorColumn;
			(void) buffer.deleteChar(state.cursorLine, state.cursorColumn);
			return;
		}
		if (state.cursorLine == 0) {
			beep(output);
			return;
		}

		auto const previousLength = buffer.lineLength(state.cursorLine - 1U);
		if (!buffer.joinWithNextLine(state.cursorLine - 1U)) {
			setStatus(state, "cannot join lines: resulting line is too long");
			beep(output);
			return;
		}
		--state.cursorLine;
		state.cursorColumn = previousLength;
	}

	void deleteForward(TextBuffer& buffer, EditorState& state, lexxos::io::OutputStream& output) {
		resetStatusOnEdit(state);
		if (buffer.lineCount == 0) {
			beep(output);
			return;
		}
		if (state.cursorColumn < buffer.lineLength(state.cursorLine)) {
			(void) buffer.deleteChar(state.cursorLine, state.cursorColumn);
			return;
		}
		if (!buffer.joinWithNextLine(state.cursorLine)) {
			beep(output);
		}
	}

	void splitCurrentLine(TextBuffer& buffer, EditorState& state, lexxos::io::OutputStream& output) {
		resetStatusOnEdit(state);
		if (!buffer.splitLine(state.cursorLine, state.cursorColumn)) {
			setStatus(state, "cannot create new line: editor line limit reached");
			beep(output);
			return;
		}
		++state.cursorLine;
		state.cursorColumn = 0;
	}

	void saveBuffer(lexxos::os::AppContext& context, std::string_view path, TextBuffer& buffer, EditorState& state) {
		if (EditorFileService::save(context, path, buffer)) {
			buffer.modified = false;
			state.confirmQuit = false;
			setStatus(state, "file saved");
		} else {
			setStatus(state, "save failed; see previous error");
		}
	}

	bool requestQuit(TextBuffer const& buffer, EditorState& state) {
		if (!buffer.modified) {
			return true;
		}
		if (state.confirmQuit) {
			return true;
		}
		state.confirmQuit = true;
		setStatus(state, "unsaved changes: press ^Q again to discard or ^S to save");
		return false;
	}

	void initializeEditorState(EditorState& state) {
		state = {};
		setStatus(state, "^S save, ^Q quit, arrows move, Home/End, PgUp/PgDn");
	}

} // namespace

int EditorController::run(lexxos::os::AppContext& context, std::string_view path,
    TextBuffer& buffer, EditorState& state) {
	auto& output = context.standardOutput();
	initializeEditorState(state);
	lexxos::terminal::clearScreen(output);

	// The command launcher may leave the Enter key that submitted "edit" in the input queue.
	bool ignoreInitialEnter = true;
	while (!lexxos::kernel::isShutdownRequested()) {
		auto const metrics = currentScreenMetrics();
		EditorRenderer::render(output, path, buffer, state, metrics);
		auto const key = lexxos::terminal::readKey();

		// Normalize the first queued key before dispatching editor commands.
		if (ignoreInitialEnter && key.code == lexxos::terminal::KeyCode::Enter) {
			ignoreInitialEnter = false;
			continue;
		}
		ignoreInitialEnter = false;

		// Dispatch navigation, editing, save and exit commands.
		switch (key.code) {
			case lexxos::terminal::KeyCode::Character:
				if (key.character == '\t') {
					if (!insertTab(buffer, state)) {
						setStatus(state, "cannot insert tab: line is too long");
						beep(output);
					}
					break;
				}
				if (lexxos::terminal::isPrintableAscii(key)) {
					if (!insertCharacter(buffer, state, key.character)) {
						setStatus(state, "cannot insert character: line is too long");
						beep(output);
					}
				}
				break;
			case lexxos::terminal::KeyCode::Enter:
				splitCurrentLine(buffer, state, output);
				break;
			case lexxos::terminal::KeyCode::Backspace:
				backspace(buffer, state, output);
				break;
			case lexxos::terminal::KeyCode::Delete:
				deleteForward(buffer, state, output);
				break;
			case lexxos::terminal::KeyCode::ArrowLeft:
				moveLeft(buffer, state);
				break;
			case lexxos::terminal::KeyCode::ArrowRight:
				moveRight(buffer, state);
				break;
			case lexxos::terminal::KeyCode::ArrowUp:
				moveUp(buffer, state);
				break;
			case lexxos::terminal::KeyCode::ArrowDown:
				moveDown(buffer, state);
				break;
			case lexxos::terminal::KeyCode::Home:
				state.confirmQuit = false;
				state.status[0] = '\0';
				state.cursorColumn = 0;
				break;
			case lexxos::terminal::KeyCode::End:
				state.confirmQuit = false;
				state.status[0] = '\0';
				state.cursorColumn = buffer.lineLength(state.cursorLine);
				break;
			case lexxos::terminal::KeyCode::PageUp:
				pageUp(buffer, state, metrics);
				break;
			case lexxos::terminal::KeyCode::PageDown:
				pageDown(buffer, state, metrics);
				break;
			case lexxos::terminal::KeyCode::CtrlS:
				saveBuffer(context, path, buffer, state);
				break;
			case lexxos::terminal::KeyCode::CtrlQ:
			case lexxos::terminal::KeyCode::CtrlX:
			case lexxos::terminal::KeyCode::CtrlC:
				if (requestQuit(buffer, state)) {
					EditorRenderer::writeExitLine(output);
					return 0;
				}
				break;
			case lexxos::terminal::KeyCode::Escape:
				state.confirmQuit = false;
				setStatus(state, "escape ignored; use ^Q to quit or ^S to save");
				break;
			case lexxos::terminal::KeyCode::Unknown:
				break;
		}
	}

	// Restore the terminal before returning to the shell during shutdown.
	EditorRenderer::writeExitLine(output);
	return 1;
}

} // namespace lexxos::apps::text_editor_programs::detail
