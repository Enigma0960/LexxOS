#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::i2c_programs {

/**
 * @lexxos-command-doc i2c-scan
 * @source src/lexxos/apps/i2c_scan_program.cpp
 * @description
 * Сканирует зарегистрированную I2C-шину и печатает таблицу адресов в стиле `i2cdetect`. Команда полезна для первичного
 * запуска плат, проверки DS3231 и осторожной диагностики периферии.
 *
 * @parameters
 * - `--bus <name>` / `-b <name>` — необязательное имя I2C-шины; по умолчанию `debug`.
 * - `<bus-name>` — необязательный короткий позиционный alias для `--bus`.
 * - `--from <addr>` — первый 7-битный адрес; десятичный или `0xhex`; по умолчанию `0x08`.
 * - `--to <addr>` — последний 7-битный адрес; десятичный или `0xhex`; по умолчанию `0x77`.
 * - `--quiet` / `-q` — печатать найденные адреса простым списком.
 *
 * @examples
 * ```text
 * lexx:/> i2c-scan
 * lexx:/> i2c-scan system
 * lexx:/> i2c-scan --bus debug --from 0x08 --to 0x77
 * lexx:/> i2c-scan --quiet
 * ```
 *
 * @author
 * Не используйте `i2c-scan` для обхода владельца southbridge во время обычного ввода с PicoCalc keyboard. Для
 * безопасного состояния PicoCalc используйте `picocalc-info` и `picocalc-sb`.
 * @end
 */
class I2cScanProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::i2c_programs
