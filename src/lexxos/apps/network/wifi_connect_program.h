#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::network_programs {

/**
 * @lexxos-command-doc wifi-connect
 * @source src/lexxos/apps/network/wifi_connect_program.cpp
 * @description
 * Подключает плату через её Wi-Fi backend: CYW43/LwIP на Pico или ESP-IDF/ESP-Hosted на ESP32.
 * После получения IP доступны сетевые команды `ping` и `rtc-sync`.
 *
 * @parameters
 * - `--timeout <ms>` / `-t <ms>` — необязательное время ожидания подключения; по умолчанию `30000`; максимум `120000`.
 * - `--auth <mode>` / `-a <mode>` — необязательный режим аутентификации: `auto`, `open`, `wpa`, `wpa2`, `mixed`;
 *   по умолчанию `auto`.
 * - `<ssid>` — имя Wi-Fi сети, до 32 байт.
 * - `[password]` — пароль сети. Для `open` можно не указывать.
 *
 * @examples
 * ```text
 * lexx:/> wifi-connect MyNetwork mypassword
 * wifi-connect: connecting to MyNetwork...
 * wifi-connect: connected
 *
 * lexx:/> wifi-connect --auth open Guest
 * ```
 *
 * @author
 * Сканирование выполняет `wifi-scan`. Команда подключения не записывает конфигурацию автоподключения;
 * ей управляет `wifi-config`. Вызовы команды исключены из истории shell, включая ошибочные.
 * @end
 */
class WifiConnectProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(
	    lexxos::os::AppContext& context,
	    lexxos::program::ProgramInvocation const& invocation,
	    lexxos::program::ParsedArguments const& arguments);
};

} // namespace lexxos::apps::network_programs
