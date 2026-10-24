#include <lexx/lexx.h>

#include "file_operations.h"
#include "input_reader.h"

namespace {

constexpr int kEntries = 96;
constexpr int kNameLength = 64;
constexpr int kPathLength = 128;
constexpr int kMinimumColumns = 40;
constexpr int kMaximumColumns = 100;
constexpr int kMinimumRows = 14;
constexpr int kMaximumRows = 40;
constexpr int kViewerHistory = 128;
constexpr int kViewerMaxBytesPerPage = 8192;
constexpr int kViewerColumns = kMaximumColumns - 2;

struct Panel {
	char path[kPathLength]{};
	lexx::DirectoryEntry items[kEntries]{};
	int count{0};
	int selected{0};
	int firstVisible{0};
	bool truncated{false};
	bool valid{false};
};

struct Settings {
	int columns{50};
	int rows{22};
	bool showHidden{true}; // Preserve the behavior of previous releases.
};

struct Offset {
	lexx::uint32_t low{0};
	lexx::uint32_t high{0};
};

struct ViewerPage {
	Offset next{};
	bool hasMore{false};
	bool ok{true};
};

Settings g_settings{};
Panel g_panels[2]{};
lexx::DirectoryEntry g_page[lexx::directory_page_size]{};
char g_viewLines[kMaximumRows][kViewerColumns + 1]{};
Offset g_history[kViewerHistory]{};

int g_active = 0;
char g_message[96] = "Tab Switch | Enter Open | Backspace Up";
char g_frame[12288]{};

lexx::size_t g_frameLength = 0;

lexx::size_t length(char const* text) {
	lexx::size_t count = 0;
	while (text[count] != 0) {
		++count;
	}
	return count;
}

bool copy(char* destination, lexx::size_t capacity, char const* value) {
	auto const count = length(value);
	if (count >= capacity) {
		return false;
	}
	for (lexx::size_t i = 0; i <= count; ++i) {
		destination[i] = value[i];
	}
	return true;
}

void message(char const* text) {
	(void) copy(g_message, sizeof(g_message), text);
}

char lower(char ch) {
	return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch + ('a' - 'A')) : ch;
}

bool sameName(char const* left, char const* right) {
	for (int i = 0; i < kNameLength; ++i) {
		if (left[i] != right[i]) {
			return false;
		}
		if (left[i] == 0) {
			return true;
		}
	}
	return true;
}

bool comesBefore(lexx::DirectoryEntry const& a, lexx::DirectoryEntry const& b) {
	if (a.type == lexx::node_directory && b.type != lexx::node_directory) {
		return true;
	}
	if (a.type != lexx::node_directory && b.type == lexx::node_directory) {
		return false;
	}
	for (int i = 0; i < kNameLength; ++i) {
		auto const left = lower(a.name[i]);
		auto const right = lower(b.name[i]);
		if (left != right) {
			return left < right;
		}
		if (left == 0) {
			return false;
		}
	}
	return false;
}

void sort(Panel& panel) {
	for (int index = 1; index < panel.count; ++index) {
		auto key = panel.items[index];
		int pos = index;
		while (pos > 0 && comesBefore(key, panel.items[pos - 1])) {
			panel.items[pos] = panel.items[pos - 1];
			--pos;
		}
		panel.items[pos] = key;
	}
}

int panelRows() {
	return g_settings.rows - 6;
}

int panelWidth() {
	return (g_settings.columns - 2) / 2;
}

int viewerWidth() {
	return g_settings.columns - 2;
}

int viewerRows() {
	return g_settings.rows - 4;
}

void keepSelectionVisible(Panel& panel) {
	if (panel.count == 0) {
		panel.selected = 0;
		panel.firstVisible = 0;
		return;
	}
	if (panel.selected < 0) {
		panel.selected = 0;
	}
	if (panel.selected >= panel.count) {
		panel.selected = panel.count - 1;
	}
	if (panel.firstVisible > panel.selected) {
		panel.firstVisible = panel.selected;
	}
	if (panel.selected >= panel.firstVisible + panelRows()) {
		panel.firstVisible = panel.selected - panelRows() + 1;
	}
}

bool load(Panel& panel) {
	panel.count = 0;
	panel.firstVisible = 0;
	panel.selected = 0;
	panel.truncated = false;
	panel.valid = false;
	lexx::FileStat stat{};

	if (lexx::stat(panel.path, stat) < 0 || stat.type != lexx::node_directory) {
		message("Directory not available");
		return false;
	}

	// Paged VFS enumeration includes hidden entries. Scan past those entries
	// rather than stopping after 96 raw directory records.
	for (lexx::uint32_t offset = 0; offset < 4096U;) {
		auto const result = lexx::list_directory(panel.path, offset, g_page);
		if (result < 0) {
			message("Cannot read directory");
			return false;
		}
		if (result == 0) {
			break;
		}

		for (int i = 0; i < result; ++i) {
			auto const* name = g_page[i].name;
			if (name[0] == 0) {
				continue;
			}
			if (name[0] == '.' && (name[1] == 0 || (name[1] == '.' && name[2] == 0))) {
				continue;
			}
			if (!g_settings.showHidden && name[0] == '.') {
				continue;
			}
			if (panel.count >= kEntries) {
				panel.truncated = true;
				break;
			}
			panel.items[panel.count++] = g_page[i];
		}

		if (panel.truncated) {
			break;
		}

		offset += static_cast<lexx::uint32_t>(result);

		if (offset >= 4096U) {
			panel.truncated = true;
			break;
		}

		if (result < static_cast<int>(lexx::directory_page_size)) {
			break;
		}

		lexx::yield();
	}
	sort(panel);
	panel.valid = true;
	return true;
}

void reloadPreservingSelection(Panel& panel) {
	char previous[kNameLength]{};

	if (panel.valid && panel.selected < panel.count) {
		(void) copy(previous, sizeof(previous), panel.items[panel.selected].name);
	}

	if (!load(panel)) {
		return;
	}

	if (previous[0] == 0) {
		return;
	}

	for (int i = 0; i < panel.count; ++i) {
		if (sameName(previous, panel.items[i].name)) {
			panel.selected = i;
			keepSelectionVisible(panel);
			return;
		}
	}
}

bool join(char* output, char const* base, char const* name) {
	lexx::size_t count = length(base);
	lexx::size_t nameCount = length(name);
	bool const separator = count != 1 || base[0] != '/';
	if (count + (separator ? 1U : 0U) + nameCount >= kPathLength) {
		return false;
	}
	for (lexx::size_t i = 0; i < count; ++i) {
		output[i] = base[i];
	}
	if (separator) {
		output[count++] = '/';
	}
	for (lexx::size_t i = 0; i <= nameCount; ++i) {
		output[count + i] = name[i];
	}
	return true;
}

void parent(Panel& panel) {
	auto size = length(panel.path);
	while (size > 1 && panel.path[size - 1] == '/') {
		--size;
	}
	while (size > 1 && panel.path[size - 1] != '/') {
		--size;
	}
	if (size > 1) {
		--size;
	}
	panel.path[size] = 0;
	if (load(panel)) {
		message("Parent directory");
	}
}

// -------------------------------------------------------------------------
// Buffered ANSI drawing (one write per complete screen, not per character).
// -------------------------------------------------------------------------
void append(char const* text) {
	while (*text && g_frameLength + 1 < sizeof(g_frame)) {
		g_frame[g_frameLength++] = *text++;
	}
}

void character(char value) {
	if (g_frameLength + 1 < sizeof(g_frame)) {
		g_frame[g_frameLength++] = value;
	}
}

void decimal(lexx::uint32_t number) {
	char digits[12]{};
	int used = 0;

	do {
		digits[used++] = static_cast<char>('0' + number % 10U);
		number /= 10U;
	} while (number != 0 && used < 11);

	while (used) {
		character(digits[--used]);
	}
}

void cursor(int row, int column) {
	append("\x1b[");
	decimal(static_cast<lexx::uint32_t>(row));
	character(';');
	decimal(static_cast<lexx::uint32_t>(column));
	character('H');
}

void field(char const* text, int width) {
	int index = 0;
	while (index < width && text[index] != 0) {
		character(text[index++]);
	}
	while (index++ < width) {
		character(' ');
	}
}

void fieldNumber(lexx::uint32_t value, int width) {
	char digits[12]{};
	int used = 0;

	do {
		digits[used++] = static_cast<char>('0' + (value % 10U));
		value /= 10U;
	} while (value && used < 11);

	if (used > width) {
		field(">999K", width);
		return;
	}

	while (width-- > used) {
		character(' ');
	}

	while (used) {
		character(digits[--used]);
	}
}

void labelRow(int row, char const* text, char const* style) {
	cursor(row, 1);
	append(style);
	field(text, g_settings.columns);
}

void flush() {
	append("\x1b[0m");
	(void) lexx::write(g_frame, g_frameLength);
}

void drawPanel(int panelIndex) {
	auto const& panel = g_panels[panelIndex];
	// Give the right panel every remaining column after the left panel and divider.
	// With an even window width, two equal integer widths leave one cell unpainted.
	int const leftWidth = panelWidth();
	int const width = panelIndex == 0 ? leftWidth : g_settings.columns - leftWidth - 1;
	int const column = panelIndex == 0 ? 1 : leftWidth + 2;
	bool const active = panelIndex == g_active;

	cursor(2, column);
	append(active ? "\x1b[1;37;44m" : "\x1b[37;44m");
	field(panel.path, width);
	for (int line = 0; line < panelRows(); ++line) {
		cursor(3 + line, column);
		int const position = panel.firstVisible + line;
		bool const selected = active && position == panel.selected && position < panel.count;
		append(selected ? "\x1b[30;47m" : "\x1b[37;44m");
		if (!panel.valid) {
			field(line == 0 ? "[unavailable]" : "", width);
		} else if (position < panel.count) {
			auto const& item = panel.items[position];
			if (!selected) {
				if (!selected) {
					append(item.type == lexx::node_directory ? "\x1b[1;36;44m" : "\x1b[37;44m");
				}
			}
			character(' ');
			field(item.name, width - 9);
			character(' ');
			if (item.type == lexx::node_directory) {
				field("<DIR>", 6);
			} else if (item.size_high != 0) {
				field(" >4G", 6);
			} else {
				fieldNumber(item.size_low, 6);
			}
			character(' ');
		} else {
			field("", width);
		}
	}
	cursor(g_settings.rows - 3, column);
	append("\x1b[30;46m");
	field(panel.truncated ? "List limited to 96 items" : "", width);
}

void draw() {
	g_frameLength = 0;
	labelRow(1, " Lexx Commander [file manager]", "\x1b[1;37;44m");
	drawPanel(0);
	drawPanel(1);

	for (int row = 2; row < g_settings.rows - 2; ++row) {
		cursor(row, panelWidth() + 1);
		append("\x1b[36;44m|");
	}

	labelRow(g_settings.rows - 2, g_message, "\x1b[30;46m");
	labelRow(g_settings.rows - 1, " F3 View  F5 Copy  F6 Move  F8 Delete", "\x1b[30;47m");
	labelRow(g_settings.rows, " N Rename  F9 Options  R Refresh  Q Exit", "\x1b[37;44m");

	flush();
}

void moveSelection(Panel& panel, int delta) {
	if (panel.count == 0) {
		return;
	}

	int next = panel.selected + delta;
	if (next < 0) {
		next = 0;
	}

	if (next >= panel.count) {
		next = panel.count - 1;
	}

	panel.selected = next;
	keepSelectionVisible(panel);
}

// -------------------------------------------------------------------------
// Persistent state is intentionally NOT written: guest storage-write remains
// denied by LexxOS. Options apply for the lifetime of the running process.
// -------------------------------------------------------------------------
void drawSettings(int selected) {
	g_frameLength = 0;
	labelRow(1, " Lexx Commander - OPTIONS", "\x1b[1;37;44m");
	for (int row = 2; row <= g_settings.rows; ++row) {
		labelRow(row, "", "\x1b[37;44m");
	}
	for (int option = 0; option < 3; ++option) {
		cursor(4 + option * 2, 3);
		append(option == selected ? "\x1b[30;47m" : "\x1b[37;44m");
		if (option == 0) {
			append(" Window width:  ");
			decimal(static_cast<lexx::uint32_t>(g_settings.columns));
		} else if (option == 1) {
			append(" Window height: ");
			decimal(static_cast<lexx::uint32_t>(g_settings.rows));
		} else {
			append(" Hidden entries: ");
			append(g_settings.showHidden ? "show" : "hide");
		}
		append("\x1b[0m");
	}
	labelRow(g_settings.rows - 2, " Up/Down select  Left/Right change", "\x1b[30;46m");
	labelRow(g_settings.rows - 1, " Enter toggles hidden entries", "\x1b[30;47m");
	labelRow(g_settings.rows, " Esc/Q return to panels", "\x1b[37;44m");
	flush();
}

void options(fileman::InputReader& input) {
	int selection = 0;
	drawSettings(selection);
	while (true) {
		auto const key = input.poll();
		if (key == fileman::InputKey::None) {
			lexx::yield();
			continue;
		}
		if (key == fileman::InputKey::Escape || key == fileman::InputKey::Quit || key == fileman::InputKey::Options) {
			break;
		}
		if (key == fileman::InputKey::Up && selection > 0) {
			--selection;
		}
		if (key == fileman::InputKey::Down && selection < 2) {
			++selection;
		}
		if (key == fileman::InputKey::Left || key == fileman::InputKey::Right || key == fileman::InputKey::Enter) {
			int const delta = key == fileman::InputKey::Left ? -2 : 2;
			if (selection == 0 && key != fileman::InputKey::Enter) {
				int next = g_settings.columns + delta;
				if (next >= kMinimumColumns && next <= kMaximumColumns) {
					g_settings.columns = next;
				}
			} else if (selection == 1 && key != fileman::InputKey::Enter) {
				int next = g_settings.rows + delta;
				if (next >= kMinimumRows && next <= kMaximumRows) {
					g_settings.rows = next;
				}
			} else if (selection == 2) {
				g_settings.showHidden = !g_settings.showHidden;
				for (auto& panel : g_panels) {
					reloadPreservingSelection(panel);
				}
			}
		}
		lexx::write("\x1b[2J\x1b[H");
		drawSettings(selection);
	}
	for (auto& panel : g_panels) {
		keepSelectionVisible(panel);
	}
	lexx::write("\x1b[2J\x1b[H");
	message("Options updated (session only)");
}

bool offsetLess(Offset left, Offset right) {
	if (left.high != right.high) {
		return left.high < right.high;
	}
	return left.low < right.low;
}

void advance(Offset& offset) {
	++offset.low;
	if (offset.low == 0) {
		++offset.high;
	}
}

void resetViewLines() {
	for (int row = 0; row < viewerRows(); ++row) {
		for (int column = 0; column < viewerWidth(); ++column) {
			g_viewLines[row][column] = ' ';
		}
		g_viewLines[row][viewerWidth()] = 0;
	}
}

ViewerPage readViewerPage(lexx::uint32_t fd, Offset start) {
	resetViewLines();
	ViewerPage output{};
	output.next = start;

	if (lexx::seek(fd, start.low, start.high) < 0) {
		output.ok = false;
		return output;
	}

	char buffer[128]{};
	int row = 0;
	int column = 0;
	int consumed = 0;
	bool reachedEnd = false;

	while (row < viewerRows() && consumed < kViewerMaxBytesPerPage) {
		auto const count = lexx::read(fd, buffer, sizeof(buffer));
		if (count == 0) {
			reachedEnd = true;
			break;
		}

		if (count == lexx::no_data || count == lexx::cancelled || count > sizeof(buffer)) {
			output.ok = false;
			return output;
		}

		for (lexx::uint32_t i = 0; i < count && row < viewerRows() && consumed < kViewerMaxBytesPerPage; ++i) {
			auto const byte = static_cast<unsigned char>(buffer[i]);
			advance(output.next);
			++consumed;

			if (byte == '\r') {
				continue;
			}

			if (byte == '\n') {
				++row;
				column = 0;
				continue;
			}

			if (byte == '\t') {
				// ASCII-only terminal: render tabs as spaces up to a 4-cell boundary.
				do {
					g_viewLines[row][column++] = ' ';
					if (column >= viewerWidth()) {
						column = 0;
						++row;
						break;
					}
				} while ((column & 3) != 0);
				continue;
			}

			g_viewLines[row][column] = byte >= 32 && byte <= 126 ? static_cast<char>(byte) : '.';
			if (++column >= viewerWidth()) {
				column = 0;
				++row;
			}
		}
		lexx::yield();
	}

	// Dynamically generated /proc and /sys files report size=0 in stat().
	// Stop at read() EOF rather than trusting the advertised length.
	// Peek at the exact next-page offset: read() may have fetched extra bytes
	// beyond the last visible character in its 128-byte buffer.
	if (!reachedEnd) {
		if (lexx::seek(fd, output.next.low, output.next.high) < 0) {
			output.ok = false;
			return output;
		}

		char probe{};
		auto const count = lexx::read(fd, &probe, 1);

		if (count == 1U) {
			output.hasMore = true;
		} else if (count != 0U) {
			output.ok = false;
		}
	}

	return output;
}

void drawViewer(char const* path, int page, ViewerPage const& data) {
	g_frameLength = 0;
	labelRow(1, " Lexx Commander - FILE VIEW [read only]", "\x1b[1;37;44m");
	labelRow(2, path, "\x1b[1;37;44m");

	for (int row = 0; row < viewerRows(); ++row) {
		cursor(row + 3, 1);
		append("\x1b[37;44m ");
		field(g_viewLines[row], viewerWidth());
		character(' '); // Complete the blue rectangle's rightmost column.
	}

	cursor(g_settings.rows - 1, 1);
	append("\x1b[30;46m");
	auto const statusStart = g_frameLength;
	append(" Page ");
	decimal(static_cast<lexx::uint32_t>(page + 1));
	append(data.ok ? (data.hasMore ? "  More data" : "  End of file") : "  Read failed");

	// CSI K clears the whole terminal row, overflowing our configured width.
	for (int painted = static_cast<int>(g_frameLength - statusStart);
	    painted < g_settings.columns; ++painted) {
		character(' ');
	}

	labelRow(g_settings.rows, " PgUp/PgDn Page  J/K Page  Esc/Q Back", "\x1b[30;47m");
	flush();
}

void showFile(char const* path, fileman::InputReader& input) {
	lexx::FileStat stat{};
	if (lexx::stat(path, stat) < 0 || stat.type != lexx::node_regular_file) {
		message("Cannot stat selected file");
		return;
	}

	auto const fd = lexx::open_read(path);

	if (fd < 0) {
		message("Cannot open selected file");
		return;
	}

	int historyCount = 1;
	int pageIndex = 0;
	g_history[0] = {};
	lexx::write("\x1b[2J\x1b[H\x1b[?25l");
	while (true) {
		auto const data = readViewerPage(static_cast<lexx::uint32_t>(fd), g_history[pageIndex]);
		drawViewer(path, pageIndex, data);
		while (true) {
			auto const key = input.poll();
			if (key == fileman::InputKey::None) {
				lexx::yield();
				continue;
			}
			if (key == fileman::InputKey::Escape || key == fileman::InputKey::Quit || key == fileman::InputKey::View || key == fileman::InputKey::Backspace) {
				(void) lexx::close(static_cast<lexx::uint32_t>(fd));
				lexx::write("\x1b[2J\x1b[H");
				return;
			}
			if (key == fileman::InputKey::PageDown || key == fileman::InputKey::Down || key == fileman::InputKey::Right) {
				if (pageIndex + 1 < historyCount) {
					++pageIndex;
					break;
				}

				if (!data.ok || !data.hasMore || !offsetLess(g_history[pageIndex], data.next)) {
					continue;
				}

				if (historyCount == kViewerHistory) {
					for (int i = 1; i < kViewerHistory; ++i) {
						g_history[i - 1] = g_history[i];
					}
					g_history[kViewerHistory - 1] = data.next;
					pageIndex = kViewerHistory - 1;
				} else {
					g_history[historyCount++] = data.next;
					++pageIndex;
				}

				break;
			}
			if (key == fileman::InputKey::PageUp || key == fileman::InputKey::Up || key == fileman::InputKey::Left) {
				if (pageIndex > 0) {
					--pageIndex;
					break;
				}
			}
		}
	}
}

void openSelected(Panel& panel, bool forceView, fileman::InputReader& input) {
	if (!panel.valid || panel.count == 0) {
		return;
	}

	char newPath[kPathLength]{};
	if (!join(newPath, panel.path, panel.items[panel.selected].name)) {
		message("Path exceeds 127 bytes");
		return;
	}

	if (panel.items[panel.selected].type == lexx::node_directory && !forceView) {
		if (!copy(panel.path, sizeof(panel.path), newPath)) {
			return;
		}

		(void) load(panel);

		return;
	}

	if (panel.items[panel.selected].type == lexx::node_regular_file) {
		showFile(newPath, input);
	}
}

// -------------------------------------------------------------------------
// Confirm destructive operations and collect a single new filename.
// -------------------------------------------------------------------------
void showDialog(char const* heading, char const* path, char const* details) {
	g_frameLength = 0;
	labelRow(1, " Lexx Commander - FILE OPERATION", "\x1b[1;37;44m");
	for (int row = 2; row <= g_settings.rows; ++row) {
		labelRow(row, "", "\x1b[37;44m");
	}
	labelRow(3, heading, "\x1b[1;37;44m");
	labelRow(4, path, "\x1b[37;44m");
	labelRow(g_settings.rows - 1, details, "\x1b[30;46m");
	flush();
}

bool confirmDeletion(char const* path) {
	showDialog(" DELETE selected item (directory: recursively)?", path, " Press Y to delete, any other key cancels");
	for (;;) {
		auto const value = lexx::read_char();
		if (value == lexx::no_data) {
			lexx::yield();
			continue;
		}
		if (value == lexx::cancelled) {
			return false;
		}
		return value == 'y' || value == 'Y';
	}
}

bool renamePrompt(char* name, lexx::size_t capacity, char const* path, char const* current) {
	if (!copy(name, capacity, current)) {
		return false;
	}
	lexx::size_t used = length(name);
	for (;;) {
		showDialog(" Rename selected item", path, " Enter=accept  Esc=cancel  Backspace=erase");
		cursor(6, 2);
		append("\x1b[30;47m");
		field(name, g_settings.columns - 3);
		flush();
		for (;;) {
			auto const value = lexx::read_char();
			if (value == lexx::no_data) {
				lexx::yield();
				continue;
			}
			if (value == lexx::cancelled || value == 0x1b) {
				return false;
			}
			if (value == '\r' || value == '\n') {
				if (used == 0 || (used == 1 && name[0] == '.') || (used == 2 && name[0] == '.' && name[1] == '.')) {
					return false;
				}
				return true;
			}
			if (value == 0x7f || value == '\b') {
				if (used) {
					name[--used] = 0;
				}
				break;
			}
			if (value >= 32 && value < 127 && value != '/' && value != '\\' && used + 1 < capacity) {
				name[used++] = static_cast<char>(value);
				name[used] = 0;
				break;
			}
		}
	}
}

void performOperation(fileman::InputKey action) {
	auto& sourcePanel = g_panels[g_active];
	auto& targetPanel = g_panels[1 - g_active];
	if (!sourcePanel.valid || sourcePanel.count == 0 || sourcePanel.selected >= sourcePanel.count) {
		message("No source item selected");
		return;
	}
	char const* name = sourcePanel.items[sourcePanel.selected].name;
	char source[kPathLength]{};
	if (!join(source, sourcePanel.path, name)) {
		message("Source path too long");
		return;
	}
	char target[kPathLength]{};
	if (action == fileman::InputKey::Copy || action == fileman::InputKey::Move) {
		if (!targetPanel.valid || !join(target, targetPanel.path, name)) {
			message("Destination unavailable or too long");
			return;
		}
	} else if (action == fileman::InputKey::Rename) {
		char newName[kNameLength]{};
		if (!renamePrompt(newName, sizeof(newName), source, name)) {
			lexx::write("\x1b[2J\x1b[H");
			message("Rename cancelled");
			return;
		}
		if (!join(target, sourcePanel.path, newName)) {
			message("New path too long");
			return;
		}
	}
	if (action == fileman::InputKey::Delete && !confirmDeletion(source)) {
		lexx::write("\x1b[2J\x1b[H");
		message("Delete cancelled");
		return;
	}

	// Never rename onto or copy over existing files. Non-atomic cross-mount
	// moves copy everything before removing the original; failures retain source.
	fileman::FileOpResult result{};
	switch (action) {
		case fileman::InputKey::Copy  : result = fileman::copyPath(source, target); break;
		case fileman::InputKey::Move  : result = fileman::movePath(source, target); break;
		case fileman::InputKey::Rename: result = fileman::renamePath(source, target); break;
		case fileman::InputKey::Delete: result = fileman::deletePath(source); break;
		default                       : return;
	}
	lexx::write("\x1b[2J\x1b[H");
	reloadPreservingSelection(sourcePanel);
	reloadPreservingSelection(targetPanel);
	message(fileman::describe(result));
}

void initializePanel(Panel& panel, char const* preferred) {
	if (!copy(panel.path, sizeof(panel.path), preferred) || !load(panel)) {
		(void) copy(panel.path, sizeof(panel.path), "/");
		(void) load(panel);
	}
}

int appMain() {
	auto const left = lexx::argc() > 1 ? lexx::argument(1) : "/mnt/sd";
	auto const right = lexx::argc() > 2 ? lexx::argument(2) : "/";
	initializePanel(g_panels[0], left);
	initializePanel(g_panels[1], right);
	message("Tab panel  Arrows select  Enter open");
	lexx::write("\x1b[2J\x1b[H\x1b[?25l");
	draw();
	fileman::InputReader keyboard{};
	while (true) {
		auto const key = keyboard.poll();

		if (key == fileman::InputKey::None) {
			lexx::yield();
			continue;
		}

		auto& panel = g_panels[g_active];
		if (key == fileman::InputKey::Quit || key == fileman::InputKey::Escape) {
			break;
		}
		if (key == fileman::InputKey::Tab) {
			g_active = 1 - g_active;
		} else if (key == fileman::InputKey::Up) {
			moveSelection(panel, -1);
		} else if (key == fileman::InputKey::Down) {
			moveSelection(panel, 1);
		} else if (key == fileman::InputKey::Left) {
			g_active = 0;
		} else if (key == fileman::InputKey::Right) {
			g_active = 1;
		} else if (key == fileman::InputKey::Backspace) {
			parent(panel);
		} else if (key == fileman::InputKey::Enter) {
			openSelected(panel, false, keyboard);
		} else if (key == fileman::InputKey::View) {
			openSelected(panel, true, keyboard);
		} else if (key == fileman::InputKey::Options) {
			options(keyboard);
		} else if (key == fileman::InputKey::Copy || key == fileman::InputKey::Move || key == fileman::InputKey::Rename || key == fileman::InputKey::Delete) {
			performOperation(key);
		} else if (key == fileman::InputKey::Refresh) {
			reloadPreservingSelection(panel);
			message("Directory refreshed");
		} else {
			continue;
		}
		draw();
	}
	lexx::write("\x1b[0m\x1b[?25h\x1b[2J\x1b[H");
	return 0;
}

} // namespace

extern "C" [[noreturn]] void _start() {
	lexx::exit(appMain());
}
