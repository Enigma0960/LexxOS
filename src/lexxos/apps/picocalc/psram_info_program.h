#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::picocalc_programs {

/**
 * @lexxos-command-doc psram-info
 * @source src/lexxos/apps/picocalc/psram_info_program.cpp
 * @description
 * Печатает распиновку PSRAM из активного профиля платы и читает JEDEC ID командой `0x9F` через консервативный
 * программный SPI. На PicoCalc это диагностика первичного запуска внешней PSRAM без DMA и общих IRQ.
 *
 * @parameters
 * Команда не принимает пользовательских параметров.
 *
 * @examples
 * ```text
 * lexx:/> psram-info
 * psram: cs=20 sck=21 mosi=2 miso=3 sio2=4 sio3=5
 * jedec-id: 00 00 00 0D 5D 53 31 76
 * ```
 *
 * @author
 * Проверка на реальном ClockworkPi PicoCalc / Pico W подтвердила ответ `00 00 00 0D 5D 53 31 76`. Если все байты
 * равны `00` или `ff`, команда считает, что PSRAM не ответила.
 * @end
 */
class PsramInfoProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::picocalc_programs
