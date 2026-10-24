#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::device_programs {

/**
 * @lexxos-command-doc display-power
 * @source src/lexxos/apps/devices/display_power_program.cpp
 * @description
 * Показывает или меняет режим питания display controller. На PicoCalc поверх режима дисплея применяется board policy:
 * `off`/`sleep` гасят LCD и keyboard backlight, а `on` восстанавливает последние уровни подсветки. На ESP32-P4 DSI
 * дисплеях команда использует device-level power/backlight API.
 *
 * @parameters
 * - Без параметров — показать режим основного дисплея.
 * - `on|off|sleep [name]` — переключить основной или указанный display device.
 * - `timeout` — показать текущую idle-timeout настройку.
 * - `timeout <seconds|off> [off|sleep]` — сохранить таймаут бездействия; диапазон секунд `0..86400`.
 *
 * @examples
 * ```text
 * lexx:/> display-power
 * lexx:/> display-power off
 * lexx:/> display-power on
 * lexx:/> display-power timeout 60 sleep
 * ```
 *
 * @author
 * Первый `KeyDown` после `off`/`sleep` будит экран и не передаётся дальше в shell. Настройка таймаута сохраняется в
 * `/mnt/sd/.lexx/display-power.conf`; если файла ещё нет, используется runtime default платы.
 * @end
 */
class DisplayPowerProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&, lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::device_programs
