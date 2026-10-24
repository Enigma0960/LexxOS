#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::picocalc_programs {

/**
 * @lexxos-command-doc psram-test
 * @source src/lexxos/apps/picocalc/psram_test_program.cpp
 * @description
 * Выполняет осторожную проверку чтения и записи небольшого диапазона PSRAM: читает исходные байты, пишет два шаблона,
 * проверяет чтение и восстанавливает исходное содержимое.
 *
 * @parameters
 * - `--address <addr>` / `-a <addr>` — необязательный 24-битный начальный адрес; десятичный или `0xhex`; по умолчанию
 *   `0`.
 * - `--length <bytes>` / `-l <bytes>` — необязательная длина; по умолчанию `256`; диапазон `1..256`.
 *
 * @examples
 * ```text
 * lexx:/> psram-test
 * psram-test: address=0x00000000 length=256
 * psram-test: ok restored=yes
 *
 * lexx:/> psram-test --address 0x100 --length 128
 * psram-test: address=0x00000100 length=128
 * psram-test: ok restored=yes
 * ```
 *
 * @author
 * Диапазон намеренно ограничен 256 байтами, чтобы команда оставалась безопасной диагностикой до появления
 * распределителя памяти, пула framebuffer/cache или блочного представления PSRAM.
 * @end
 */
class PsramTestProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::picocalc_programs
