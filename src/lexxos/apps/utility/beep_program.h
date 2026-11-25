#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/devices/audio_device.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program.h>

namespace lexxos::apps::utility_programs {

/**
 * @lexxos-command-doc beep
 * @platforms sim espidf
 * @source src/lexxos/apps/utility/beep_program.h
 * @description
 * Проигрывает короткий диагностический тон через переносимый `AudioDevice`. В sim-сборках команда использует
 * `emu_pwm_audio` или `emu_adc_audio`, чтобы быстро проверить аудиовывод эмулятора без PicoCalc GPIO/PWM зависимостей.
 *
 * @parameters
 * - `--frequency <hz>` / `-f <hz>` — частота тона; по умолчанию берется из устройства.
 * - `--duration <ms>` / `-d <ms>` — длительность; по умолчанию берется из устройства.
 * - `--volume <0..255>` / `-v <0..255>` — громкость; по умолчанию `128`.
 *
 * @examples
 * ```text
 * lexx:/> beep
 * lexx:/> beep --frequency 1200 --duration 80 --volume 96
 * ```
 * @end
 */
class BeepProgram {
public:
	[[nodiscard]] static lexxos::program::ProgramDescriptor descriptor();

private:
	static constexpr lexxos::program::ProgramArgumentSpec arguments[] = {
	    {
	     .name = "frequency",
	     .shortName = 'f',
	     .valueName = "hz",
	     .help = "tone frequency in Hz",
	     .kind = lexxos::program::ProgramArgumentKind::Option,
	     .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
	     .required = false,
	     },
	    {
	     .name = "duration",
	     .shortName = 'd',
	     .valueName = "ms",
	     .help = "beep duration in milliseconds",
	     .kind = lexxos::program::ProgramArgumentKind::Option,
	     .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
	     .required = false,
	     },
	    {
	     .name = "volume",
	     .shortName = 'v',
	     .valueName = "0..255",
	     .help = "tone volume, default 128",
	     .kind = lexxos::program::ProgramArgumentKind::Option,
	     .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
	     .required = false,
	     },
	};
	static constexpr std::size_t argumentCount = sizeof(arguments) / sizeof(arguments[0]);
	static constexpr std::uint8_t defaultVolume = 128;

	[[nodiscard]] static bool parseOptionalUnsigned(
	    lexxos::program::ParsedArguments const& arguments,
	    std::string_view name,
	    std::uint32_t& value);

	static int run(
	    lexxos::os::AppContext& context,
	    lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const& arguments);
};

} // namespace lexxos::apps::utility_programs
