#pragma once

#include <cstdint>
#include <string_view>

namespace lexxos::devices {
class I2cBus;
} // namespace lexxos::devices

namespace lexxos::os {
class AppContext;
} // namespace lexxos::os

namespace lexxos::apps::i2c_programs::detail {

[[nodiscard]] bool parseUnsigned(std::string_view text, std::uint32_t& output);
void writeHex8(lexxos::os::AppContext& context, std::uint8_t value);
void writeUnsigned(lexxos::os::AppContext& context, std::uint32_t value);
void writeI2cBusList(lexxos::os::AppContext& context, bool errorOutput = false);
std::uint32_t writeI2cAddressTable(lexxos::os::AppContext& context,
    lexxos::devices::I2cBus& bus,
    std::uint8_t from,
    std::uint8_t to);

} // namespace lexxos::apps::i2c_programs::detail
