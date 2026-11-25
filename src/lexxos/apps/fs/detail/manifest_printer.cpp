#include <lexxos/apps/fs/detail/manifest_printer.h>

#include <lexxos/os/app_context.h>

namespace lexxos::apps::filesystem_programs::detail {

using os::AppContext;

namespace {

	bool jsonNumber(std::string_view text) {
		std::size_t i = 0;
		auto digit = [&] {
			return i < text.size() && text[i] >= '0' && text[i] <= '9';
		};
		if (i < text.size() && text[i] == '-') {
			++i;
		}

		if (!digit()) {
			return false;
		}

		if (text[i] == '0') {
			++i;
		} else {
			while (digit()) {
				++i;
			}
		}

		if (i < text.size() && text[i] == '.') {
			++i;
			if (!digit()) {
				return false;
			}

			while (digit()) {
				++i;
			}
		}

		if (i < text.size() && (text[i] == 'e' || text[i] == 'E')) {
			++i;
			if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
				++i;
			}

			if (!digit()) {
				return false;
			}

			while (digit()) {
				++i;
			}
		}

		return i == text.size();
	}

} // namespace

ManifestPrinter::ManifestPrinter(AppContext& context, std::string_view json, bool emit)
    : context_(context)
    , json_(json)
    , emit_(emit) {
	constexpr std::string_view prefix = "manifest.";
	for (auto ch : prefix) {
		label_[length_++] = ch;
	}
}

bool ManifestPrinter::run() {
	spaces();
	if (!object(0)) {
		return false;
	}

	spaces();
	return position_ == json_.size();
}

void ManifestPrinter::spaces() {
	while (position_ < json_.size() && (json_[position_] == ' ' || json_[position_] == '\n' || json_[position_] == '\r' || json_[position_] == '\t')) {
		++position_;
	}
}

bool ManifestPrinter::consume(char ch) {
	spaces();
	if (position_ >= json_.size() || json_[position_] != ch) {
		return false;
	}

	++position_;
	return true;
}

bool ManifestPrinter::string(std::string_view& value) {
	if (!consume('"')) {
		return false;
	}

	auto const begin = position_;
	while (position_ < json_.size()) {
		char const ch = json_[position_++];
		if (ch == '"') {
			value = json_.substr(begin, position_ - begin - 1);
			return true;
		}

		if (static_cast<unsigned char>(ch) < 0x20U) {
			return false;
		}

		if (ch != '\\') {
			continue;
		}

		if (position_ >= json_.size()) {
			return false;
		}

		char const escape = json_[position_++];
		if (escape == 'u') {
			for (unsigned i = 0; i < 4; ++i) {
				if (position_ >= json_.size()) {
					return false;
				}

				char const digit = json_[position_++];
				if (!((digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f') || (digit >= 'A' && digit <= 'F'))) {
					return false;
				}
			}
		} else if (escape != '"' && escape != '\\' && escape != '/' && escape != 'b' && escape != 'f' && escape != 'n' && escape != 'r' && escape != 't') {
			return false;
		}
	}

	return false;
}

bool ManifestPrinter::scalar(bool show) {
	spaces();
	std::string_view data;
	if (position_ >= json_.size()) {
		return false;
	}

	if (json_[position_] == '"') {
		if (!string(data)) {
			return false;
		}
	} else {
		auto const begin = position_;
		while (position_ < json_.size()) {
			char const ch = json_[position_];
			if (ch == ',' || ch == '}' || ch == ']' || ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t') {
				break;
			}

			++position_;
		}

		if (position_ == begin) {
			return false;
		}

		data = json_.substr(begin, position_ - begin);

		// JSON scalar other than a string: number, boolean or null.
		if (data != "true" && data != "false" && data != "null" && !jsonNumber(data)) {
			return false;
		}
	}

	if (emit_ && show) {
		context_.write(std::string_view{label_.data(), length_});
		context_.write(": ");
		context_.writeLine(data);
	}

	return true;
}

bool ManifestPrinter::value(unsigned depth, bool show) {
	if (depth > 8) {
		return false;
	}

	spaces();
	if (position_ >= json_.size()) {
		return false;
	}

	if (json_[position_] == '{') {
		return object(depth + 1, show);
	}

	if (json_[position_] == '[') {
		if (emit_ && show) {
			context_.write(std::string_view{label_.data(), length_});
			context_.writeLine(": [array]");
		}

		if (!consume('[')) {
			return false;
		}

		spaces();
		if (consume(']')) {
			return true;
		}

		do {
			if (!value(depth + 1, false)) {
				return false;
			}

			spaces();

			if (consume(']')) {
				return true;
			}
		} while (consume(','));

		return false;
	}

	return scalar(show);
}

bool ManifestPrinter::object(unsigned depth, bool show) {
	if (depth > 8 || !consume('{')) {
		return false;
	}

	spaces();

	if (consume('}')) {
		return true;
	}

	do {
		std::string_view key;
		if (!string(key) || !consume(':')) {
			return false;
		}

		auto const oldLength = length_;
		if (key.size() + length_ + 1 >= label_.size()) {
			return false;
		}

		for (char ch : key) {
			label_[length_++] = ch;
		}

		spaces();
		bool const nested = position_ < json_.size() && json_[position_] == '{';
		if (nested) {
			label_[length_++] = '.';
		}

		if (!value(depth, show)) {
			return false;
		}

		length_ = oldLength;
		spaces();

		if (consume('}')) {
			return true;
		}

	} while (consume(','));

	return false;
}

} // namespace lexxos::apps::filesystem_programs::detail
