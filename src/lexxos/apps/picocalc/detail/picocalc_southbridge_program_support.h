#pragma once

#include <cstdint>
#include <string_view>

#include <lexxos/os/app_context.h>
#include <lexxos/program/arguments.h>

namespace lexxos::apps::i2c_programs::picocalc_southbridge_detail {

struct CommandOptions {
	std::string_view busName;
	std::uint8_t address{0};
	std::uint32_t pollCount{0};
	bool directRequested{false};
};

[[nodiscard]] bool parseCommandOptions(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments,
    CommandOptions& options);
[[nodiscard]] bool hasSouthbridgeService(lexxos::os::AppContext& context);
void writeDirectI2cRequired(lexxos::os::AppContext& context);
[[nodiscard]] int runServiceBacked(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments,
    CommandOptions const& options);
[[nodiscard]] int runDirectI2c(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments,
    CommandOptions const& options);

} // namespace lexxos::apps::i2c_programs::picocalc_southbridge_detail
