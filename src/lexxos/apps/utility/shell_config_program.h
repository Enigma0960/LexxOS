#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::utility_programs {

/**
 * @lexxos-command-doc shell-config
 * @source src/lexxos/apps/utility/shell_config_program.cpp
 * @description
 * Показывает и обновляет настройки shell line editor. Сейчас config хранит состояние fish-like autosuggestion и
 * историю ввода, которую shell лениво загружает из `/mnt/sd/.lexx/shell.conf` после появления SD mount.
 *
 * @parameters
 * - Без параметров / `show` — показать путь config, флаг загрузки, состояние autosuggest и размер истории.
 * - `load` — принудительно перечитать `/mnt/sd/.lexx/shell.conf`.
 * - `save` — записать текущие настройки и историю в `/mnt/sd/.lexx/shell.conf`.
 * - `autosuggest on|off` — включить или выключить серую подсказку из истории.
 *
 * @examples
 * ```text
 * lexx:/> shell-config show
 * lexx:/> shell-config autosuggest off
 * lexx:/> shell-config save
 * lexx:/> shell-config autosuggest on
 * ```
 *
 * @author
 * Команды с чувствительными аргументами исключаются из истории. Старые записи Wi-Fi-команд фильтруются
 * при загрузке; `save` перезаписывает файл уже без них. Это не стирает копии и прежние данные на носителе.
 * Autosuggest показывает только suffix самого свежего совпадения из истории, когда курсор стоит в конце строки.
 * Принять подсказку можно `Right`, `End` или `Ctrl-F`; `Left`, backspace, новый ввод или `Ctrl-C` очищают ghost text.
 * @end
 */
class ShellConfigProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::utility_programs
