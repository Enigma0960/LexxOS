#include <lexxos/program/arguments.h>

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

void expect(bool condition, char const* message) {
	if (!condition) {
		std::cerr << "contract test failed: " << message << '\n';
		std::exit(1);
	}
}

void checkNamedArguments() {
	lexxos::program::ParsedArguments arguments;

	expect(arguments.setNamed("verbose", "true", true), "setNamed stores a named value");
	expect(arguments.has("verbose"), "has reports a present named value");
	expect(arguments.value("verbose") == "true", "value returns the stored named value");
	expect(arguments.valueOr("missing", "fallback") == "fallback", "valueOr returns fallback for missing values");

	expect(arguments.setNamed("verbose", "false", false), "setNamed updates an existing named value");
	expect(!arguments.has("verbose"), "has follows the updated present flag");
	expect(arguments.value("verbose") == "false", "value follows the updated named value");
}

void checkPositionalArguments() {
	lexxos::program::ParsedArguments arguments;

	expect(arguments.addPositional("source", "/tmp/input.txt"), "addPositional stores the first positional value");
	expect(arguments.addPositional("target", "/tmp/output.txt"), "addPositional stores the second positional value");
	expect(arguments.positionalCount() == 2, "positionalCount tracks stored positional values");
	expect(arguments.positional(0) == "/tmp/input.txt", "positional(index) returns the first value");
	expect(arguments.positional(1) == "/tmp/output.txt", "positional(index) returns the second value");
	expect(arguments.positional("source") == "/tmp/input.txt", "positional(name) returns a named positional value");
	expect(arguments.positionalOr("missing", "fallback") == "fallback", "positionalOr returns fallback for missing values");
	expect(arguments.positional(2).empty(), "positional(index) returns an empty view for out-of-range values");
}

void checkStatusStrings() {
	using lexxos::program::ArgumentParseStatus;
	using lexxos::program::toString;

	expect(toString(ArgumentParseStatus::Ok) == "ok", "ArgumentParseStatus::Ok string");
	expect(toString(ArgumentParseStatus::HelpRequested) == "help-requested", "ArgumentParseStatus::HelpRequested string");
	expect(toString(ArgumentParseStatus::UnknownOption) == "unknown-option", "ArgumentParseStatus::UnknownOption string");
	expect(toString(ArgumentParseStatus::MissingValue) == "missing-value", "ArgumentParseStatus::MissingValue string");
	expect(toString(ArgumentParseStatus::UnexpectedArgument) == "unexpected-argument", "ArgumentParseStatus::UnexpectedArgument string");
	expect(toString(ArgumentParseStatus::MissingRequiredArgument) == "missing-required-argument",
	    "ArgumentParseStatus::MissingRequiredArgument string");
	expect(toString(ArgumentParseStatus::TooManyArguments) == "too-many-arguments",
	    "ArgumentParseStatus::TooManyArguments string");
}

} // namespace

void checkConsoleInput();
void checkProgramInput();
void checkLifecycleEventBus();
void checkRuntimeProfile();
void checkPicoCalcKeyboardLayout();
void checkPicoCalcSouthbridgeRtcWrite();

int main() {
	checkNamedArguments();
	checkPositionalArguments();
	checkStatusStrings();
	checkConsoleInput();
	checkProgramInput();
	checkLifecycleEventBus();
	checkRuntimeProfile();
	checkPicoCalcKeyboardLayout();
	checkPicoCalcSouthbridgeRtcWrite();
	return 0;
}
