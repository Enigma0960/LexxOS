#pragma once

#include <memory>

#include <lexxos/devices/output/analog_output.h>
#include <lexxos/platform/board.h>

namespace lexxos::platform::espidf {

std::unique_ptr<devices::AnalogOutput> createCeAnalogOutput(AnalogOutputConfig const& config);

} // namespace lexxos::platform::espidf
