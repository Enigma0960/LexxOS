#pragma once

#include <lexxos/composition/products/lyra_runtime_composition.h>

namespace lexxos::os {

class OperatingSystem;

} // namespace lexxos::os

namespace lexxos::composition::products {

LyraRuntimeProviders lyraRuntimeProviders(os::OperatingSystem& operatingSystem);

} // namespace lexxos::composition::products
