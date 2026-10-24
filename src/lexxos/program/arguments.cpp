#include <lexxos/program/arguments.h>

#include <cstring>

namespace lexxos::program {

namespace {

	char const* storeStringView(std::string_view value) {
		return value.empty() ? "" : value.data();
	}

	bool equals(char const* left, std::string_view right) {
		if (left == nullptr) {
			return right.empty();
		}
		return std::strlen(left) == right.size() && std::memcmp(left, right.data(), right.size()) == 0;
	}

	std::string_view viewOf(char const* value) {
		return value != nullptr ? std::string_view{value} : std::string_view{};
	}

} // namespace

std::string_view toString(ArgumentParseStatus status) {
	switch (status) {
		case ArgumentParseStatus::Ok:
			return "ok";
		case ArgumentParseStatus::HelpRequested:
			return "help-requested";
		case ArgumentParseStatus::UnknownOption:
			return "unknown-option";
		case ArgumentParseStatus::MissingValue:
			return "missing-value";
		case ArgumentParseStatus::UnexpectedArgument:
			return "unexpected-argument";
		case ArgumentParseStatus::MissingRequiredArgument:
			return "missing-required-argument";
		case ArgumentParseStatus::TooManyArguments:
			return "too-many-arguments";
	}
	return "unknown";
}

bool ParsedArguments::setNamed(std::string_view name, std::string_view value, bool present) {
	for (std::size_t index = 0; index < _namedCount; ++index) {
		if (equals(_named[index].name, name)) {
			_named[index].value = storeStringView(value);
			_named[index].present = present;
			return true;
		}
	}

	if (_namedCount >= _named.size()) {
		return false;
	}

	_named[_namedCount] = NamedValue{
	    .name = storeStringView(name),
	    .value = storeStringView(value),
	    .present = present,
	};
	++_namedCount;
	return true;
}

bool ParsedArguments::addPositional(std::string_view name, std::string_view value) {
	if (_positionalCount >= _positionals.size()) {
		return false;
	}

	_positionals[_positionalCount] = PositionalValue{
	    .name = storeStringView(name),
	    .value = storeStringView(value),
	};
	++_positionalCount;
	return true;
}

bool ParsedArguments::has(std::string_view name) const {
	for (std::size_t index = 0; index < _namedCount; ++index) {
		if (equals(_named[index].name, name)) {
			return _named[index].present;
		}
	}
	return false;
}

std::string_view ParsedArguments::value(std::string_view name) const {
	for (std::size_t index = 0; index < _namedCount; ++index) {
		if (equals(_named[index].name, name)) {
			return viewOf(_named[index].value);
		}
	}
	return {};
}

std::string_view ParsedArguments::valueOr(std::string_view name, std::string_view fallback) const {
	auto const result = value(name);
	return result.empty() ? fallback : result;
}

std::size_t ParsedArguments::positionalCount() const {
	return _positionalCount;
}

std::string_view ParsedArguments::positional(std::size_t index) const {
	return index < _positionalCount ? viewOf(_positionals[index].value) : std::string_view{};
}

std::string_view ParsedArguments::positional(std::string_view name) const {
	for (std::size_t index = 0; index < _positionalCount; ++index) {
		if (equals(_positionals[index].name, name)) {
			return viewOf(_positionals[index].value);
		}
	}
	return {};
}

std::string_view ParsedArguments::positionalOr(std::string_view name, std::string_view fallback) const {
	auto const result = positional(name);
	return result.empty() ? fallback : result;
}

} // namespace lexxos::program
