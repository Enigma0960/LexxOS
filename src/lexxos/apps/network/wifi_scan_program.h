#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::network_programs {

/**
 * @lexxos-command-doc wifi-scan
 * @source src/lexxos/apps/network/wifi_scan_program.cpp
 * @description
 * Сканирует сети через Wi-Fi backend платы (CYW43 или ESP-IDF/ESP-Hosted). На Pico по умолчанию
 * одинаковые записи `SSID + channel + auth` объединяются; `--raw` показывает исходные записи.
 *
 * @parameters
 * - `--timeout <ms>` / `-t <ms>` — необязательное время ожидания; по умолчанию `10000`; диапазон `0..60000`.
 * - `--limit <count>` / `-n <count>` — необязательный лимит результатов; по умолчанию `24`; диапазон `1..32`.
 * - `--raw` / `-r` — необязательный флаг; отключает объединение повторов и всегда печатает BSSID.
 * - `--bssid` / `-b` — необязательный флаг; печатает BSSID для объединённого результата.
 *
 * @examples
 * ```text
 * lexx:/> wifi-scan
 * wifi-scan: scanning...
 * networks: 5 merged=14
 *
 * lexx:/> wifi-scan --raw
 * wifi-scan: scanning...
 * networks: 24 dropped=1
 * ```
 *
 * @author
 * Подтверждено на реальном ClockworkPi PicoCalc / Pico W: сканирование завершилось, обычный режим объединил повторы, а
 * сырой режим показал записи BSSID/channel/RSSI/auth.
 * @end
 */
class WifiScanProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(
	    lexxos::os::AppContext& context,
	    lexxos::program::ProgramInvocation const& invocation,
	    lexxos::program::ParsedArguments const& arguments);
};

} // namespace lexxos::apps::network_programs
