#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::i2c_programs {

/**
 * @lexxos-command-doc picocalc-sb
 * @source src/lexxos/apps/picocalc/picocalc_southbridge_program.cpp
 * @description
 * Показывает состояние PicoCalc southbridge через зарегистрированный сервис `picocalc-southbridge`. В обычной
 * PicoCalc-сборке не читает FIFO клавиатуры напрямую и не выполняет прямую запись, пока клавиатура владеет southbridge.
 *
 * @parameters
 * - `--direct-i2c` — необязательный диагностический флаг; разрешён только без зарегистрированного сервиса southbridge.
 * - `--bus <name>` / `-b <name>` — необязательное имя I2C-шины для прямого режима; по умолчанию `debug`.
 * - `--address <addr>` / `-a <addr>` — необязательный 7-битный I2C-адрес для прямого режима; по умолчанию `0x1F`.
 * - `--poll <count>` / `-p <count>` — устаревший опрос FIFO клавиатуры; пропускается при активной клавиатуре.
 * - `--delay <ms>` / `-d <ms>` — устаревшая задержка для старого пути опроса.
 * - `--lcd <0..255>`, `--kbd <0..255>`, `--reset <seconds>` — прямые записи; отклоняются при активном владельце
 *   клавиатуры.
 *
 * @examples
 * ```text
 * lexx:/> picocalc-sb
 * lexx:/> picocalc-sb --poll 2
 * lexx:/> picocalc-sb --direct-i2c
 * ```
 *
 * @author
 * Главный инвариант: не обходить `picocalc-keyboard` прямым I2C-чтением FIFO. Для обычной работы используйте
 * `backlight`, `display-power`, `power` и `poweroff`.
 * @end
 */
class PicoCalcSouthbridgeProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::i2c_programs
