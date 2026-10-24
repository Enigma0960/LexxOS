#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace lexxos::program {

enum class ProgramArgumentKind : std::uint8_t {
	Flag,
	Option,
	Positional,
};

enum class ProgramArgumentValueType : std::uint8_t {
	String,
	Path,
	Unsigned,
};

struct ProgramArgumentSpec {
	std::string_view name;
	char shortName{0};
	std::string_view valueName{};
	std::string_view help;
	ProgramArgumentKind kind{ProgramArgumentKind::Option};
	ProgramArgumentValueType valueType{ProgramArgumentValueType::String};
	bool required{false};
	std::string_view defaultValue{};
	// Commands declaring sensitive arguments are excluded from shell history.
	bool sensitive{false};
};

struct ProgramArgumentList {
	ProgramArgumentSpec const* data{nullptr};
	std::size_t size{0};
};

enum class ArgumentParseStatus : std::uint8_t {
	Ok,
	HelpRequested,
	UnknownOption,
	MissingValue,
	UnexpectedArgument,
	MissingRequiredArgument,
	TooManyArguments,
};

std::string_view toString(ArgumentParseStatus status);

struct ArgumentParseError {
	ArgumentParseStatus status{ArgumentParseStatus::Ok};
	std::string_view token{};
	std::string_view argument{};
};

class ParsedArguments {
public:
	static constexpr std::size_t maxNamedValues = 32;
	static constexpr std::size_t maxPositionals = 32;

	bool setNamed(std::string_view name, std::string_view value, bool present);
	bool addPositional(std::string_view name, std::string_view value);

	[[nodiscard]] bool has(std::string_view name) const;
	[[nodiscard]] std::string_view value(std::string_view name) const;
	[[nodiscard]] std::string_view valueOr(std::string_view name, std::string_view fallback) const;
	[[nodiscard]] std::size_t positionalCount() const;
	[[nodiscard]] std::string_view positional(std::size_t index) const;
	[[nodiscard]] std::string_view positional(std::string_view name) const;
	[[nodiscard]] std::string_view positionalOr(std::string_view name, std::string_view fallback) const;

private:
	struct NamedValue {
		char const* name{nullptr};
		char const* value{nullptr};
		bool present{false};
	};

	struct PositionalValue {
		char const* name{nullptr};
		char const* value{nullptr};
	};

	std::array<NamedValue, maxNamedValues> _named{};
	std::size_t _namedCount{0};
	std::array<PositionalValue, maxPositionals> _positionals{};
	std::size_t _positionalCount{0};
};

} // namespace lexxos::program
