#pragma once

#include <cstddef>
#include <cstdint>

#include <lexxos/devices/rk3506/picosd_media_hooks.h>

namespace lexxos::platform::rk3506 {

void picoSdSetMediaHooks(PicoSdMediaHooks hooks);

bool picoSdReady();
bool picoSdCardInserted();
bool picoSdProbeAuto();
void picoSdInvalidateRemoved();
std::uint64_t picoSdSectorCount();
bool picoSdReadSector(std::uint32_t lba, std::uint8_t* destination);
bool picoSdReadSectors(std::uint32_t lba, std::uint8_t* destination, std::size_t count);
bool picoSdWriteSectors(std::uint32_t lba, std::uint8_t const* source, std::size_t count);

} // namespace lexxos::platform::rk3506
