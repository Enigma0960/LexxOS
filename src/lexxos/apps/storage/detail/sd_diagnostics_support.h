#pragma once

#include <lexxos/devices/sd_card.h>

namespace lexxos::os {
class AppContext;
} // namespace lexxos::os

namespace lexxos::apps::storage_programs::detail {

void printDiagnosticEntry(lexxos::os::AppContext& context, lexxos::devices::SdCardDiagnosticEntry const& entry,
    char const* prefix);
void printSdDiagnostics(lexxos::os::AppContext& context, char const* prefix = "  ");
void printSdLastDiagnostic(lexxos::os::AppContext& context, char const* prefix = "  ");

} // namespace lexxos::apps::storage_programs::detail
