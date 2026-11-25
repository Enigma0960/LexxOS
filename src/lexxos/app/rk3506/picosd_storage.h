#pragma once

#include <lexxos/hardware/products/storage_policy.h>

namespace lexxos::os {

class AppContext;

} // namespace lexxos::os

namespace lexxos::program {

struct ProgramInvocation;
class ParsedArguments;
class ProgramManager;

} // namespace lexxos::program

namespace lexxos::devices {

class SdCardBackend;

} // namespace lexxos::devices

namespace lexxos::platform::rk3506 {

// Required before installing commands/hooks or starting hotplug. Rebinding a
// configured service to different policy is rejected without changing live state.
bool configurePicoSdStorage(std::span<lexxos::hardware::products::StoragePolicy const> slots);

// Shared instance: the same device is used by FatFs, VFS and sd_card.
lexxos::devices::SdCardBackend& picoSdBackend();

void picoSdAutoSuppressUntilRemoval();
bool picoSdPrepareManualMount();
void picoSdManualMountFinished(bool success);
void picoSdNotifyManualUnmount();
void picoSdHotplugTask(void* userData);
bool picoSdFatMounted();
int picoSdRunMediaProgram(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&, lexxos::program::ParsedArguments const&);
void registerPicoSdMountProgram(lexxos::program::ProgramManager& programs);

} // namespace lexxos::platform::rk3506
