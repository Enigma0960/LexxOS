#include <lexxos/shell/shell_history_policy.h>

#include <array>

#include <lexxos/runtime/runtime_profile.h>

namespace lexxos::shell::detail {

// Keep the history filter cheap on the console task stack. Plain (unquoted)
// tokens are checked as views into the input, regardless of their length.
// Oversized decoded tokens are rejected rather than risking missed secrets.
constexpr std::size_t maxDecodedHistoryTokenLength = 128U;

bool hasSensitiveArguments(lexxos::program::ProgramArgumentList arguments) {
	for (std::size_t index = 0; index < arguments.size; ++index) {
		if (arguments.data[index].sensitive) {
			return true;
		}
	}
	return false;
}

bool historyLineAllowed(std::string_view line, SensitiveCommandLookup lookup, void const* context) {
	std::array<char, maxDecodedHistoryTokenLength> token{};
	std::size_t cursor = 0;
	// Inspect every decoded token conservatively, including commands after redirects.
	// This also excludes e.g. `echo wifi-connect`, rather than risking saved credentials.
	while (cursor < line.size()) {
		while (cursor < line.size() && (line[cursor] == ' ' || line[cursor] == '\t')) {
			++cursor;
		}

		auto const tokenStart = cursor;
		auto tokenEnd = cursor;

		std::size_t length = 0;
		char quote = 0;
		bool decoded = false;

		while (cursor < line.size()) {
			char value = line[cursor++];
			if (quote != 0) {
				if (value == quote) {
					quote = 0;
					decoded = true;
					continue;
				}
				if (quote == '"' && value == '\\' && cursor < line.size()) {
					value = line[cursor++];
					decoded = true;
				}
			} else {
				if (value == ' ' || value == '\t') {
					break;
				}
				if (value == '\'' || value == '"') {
					quote = value;
					decoded = true;
					continue;
				}
				if (value == '\\' && cursor < line.size()) {
					value = line[cursor++];
					decoded = true;
				}
			}

			if (length < token.size()) {
				token[length] = value;
			}

			++length;
			tokenEnd = cursor;
		}

		if (quote != 0 || (decoded && length > token.size())) {
			return false;
		}

		auto const name = decoded ? std::string_view{token.data(), length}
		                          : line.substr(tokenStart, tokenEnd - tokenStart);

		// Old SD history may be loaded on a build without Wi-Fi command registration.
		if (name == "wifi-connect" || name == "wifi-config" || (lookup != nullptr && lookup(name, context))) {
			return false;
		}
	}
	return true;
}

} // namespace lexxos::shell::detail
