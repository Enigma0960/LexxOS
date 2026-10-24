#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::rtc_programs {

/**
 * @lexxos-command-doc rtc-sync
 * @source src/lexxos/apps/rtc/rtc_sync_program.cpp
 * @description
 * Получает UTC время с NTP сервера через активный сетевой интерфейс и записывает его в выбранный RTC backend.
 * На Pico W используется CYW43 LwIP path, на ESP-IDF — стандартные сетевые сокеты.
 *
 * @parameters
 * - `--server <host>` / `-n <host>` — необязательный NTP сервер; по умолчанию `pool.ntp.org`.
 * - `--timeout <ms>` / `-t <ms>` — необязательный общий таймаут DNS + NTP; по умолчанию `8000`; максимум `60000`.
 * - `--source auto|software|picocalc|ds3231|pcf8563` / `-s ...` — RTC backend; по умолчанию `auto`.
 * - `--bus <name>` / `-b <name>` — I2C bus для внешнего RTC; по умолчанию `debug`.
 * - `--address <addr>` / `-a <addr>` — адрес внешнего RTC; если не задан, используется адрес backend-а.
 *
 * @examples
 * ```text
 * lexx:/> wifi-connect MyNetwork mypassword
 * lexx:/> rtc-sync
 * rtc-sync: resolving pool.ntp.org
 * rtc-sync: sending NTP request
 * source: picocalc-southbridge
 * network time: 2026-06-28 12:34:56 UTC
 * rtc sync: ok
 * ```
 *
 * @author
 * Первый сетевой клиент LexxOS: минимальный NTP over UDP без SNTP daemon и без сохранения сетевых настроек.
 * @end
 */
class RtcSyncProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::rtc_programs
