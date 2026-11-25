#pragma once
#include <memory>

#include <lexxos/devices/input/analog_input.h>
#include <lexxos/devices/output/pwm_output.h>
#include <lexxos/platform/board.h>

namespace lexxos::platform::espidf {

std::unique_ptr<devices::AnalogInput> createCeAnalogInput(AnalogInputConfig const& config);
std::unique_ptr<devices::PwmOutput> createCePwmOutput(PwmOutputConfig const& config);

} // namespace lexxos::platform::espidf
