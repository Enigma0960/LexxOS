#pragma once

namespace lexxos::platform::rk3506 {

// Audited GPIO0 subset only. Product ownership must be checked by the caller;
// this backend neither selects devices nor authorizes access to their pins.
bool configureGpio0Output(unsigned pin, bool initialValue);
bool writeGpio0Output(unsigned pin, bool value);

} // namespace lexxos::platform::rk3506
