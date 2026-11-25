#pragma once

#include <cstdint>
#include <string_view>

#include <commands/command_selection.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/devices/audio_device.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program.h>

namespace lexxos::apps::utility_programs {

/**
 * @lexxos-command-doc audio
 * @source src/lexxos/apps/utility/audio_program.h
 * @description
 * Показывает primary `AudioDevice` и проигрывает простой тестовый тон через переносимый audio contract. На PicoCalc это
 * пользовательский путь поверх `picocalc-audio`, в отличие от низкоуровневой GPIO/PWM диагностики `beep`.
 *
 * @parameters
 * - Без параметров или `info` — показать устройство, готовность, поддержку tone/mix и диапазоны.
 * - `left|right|stereo` — проиграть тон в выбранный канал.
 * - `--frequency <hz>` / `-f <hz>` — необязательная частота тона; допустимый диапазон сообщает устройство.
 * - `--duration <ms>` / `-d <ms>` — необязательная длительность; допустимый максимум сообщает устройство.
 * - `--volume <0..255>` / `-v <0..255>` — необязательная громкость PWM duty; по умолчанию `128`.
 *
 * @examples
 * ```text
 * lexx:/> audio
 * lexx:/> audio left
 * lexx:/> audio stereo --frequency 1200 --duration 200 --volume 96
 * ```
 *
 * @author
 * Новые уведомления, MIDI playback и будущие интерактивные приложения должны идти через `AudioDevice`, чтобы не
 * привязываться к конкретным GPIO/PWM выводам платы.
 * @end
 */
class AudioProgram {
public:
	[[nodiscard]] static lexxos::program::ProgramDescriptor descriptor();

private:
	static constexpr lexxos::program::ProgramArgumentSpec arguments[] = {
	    {
	     .name = "target",
	     .valueName = "info|left|right|stereo",
	     .help = "target action or channel",
	     .kind = lexxos::program::ProgramArgumentKind::Positional,
	     .valueType = lexxos::program::ProgramArgumentValueType::String,
	     .required = false,
	     },
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
	     .help = "tone duration in milliseconds",
	     .kind = lexxos::program::ProgramArgumentKind::Option,
	     .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
	     .required = false,
	     },
	    {
	     .name = "volume",
	     .shortName = 'v',
	     .valueName = "0..255",
	     .help = "PWM duty volume, default 128",
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

	[[nodiscard]] static bool parseChannel(std::string_view text, lexxos::devices::AudioChannel& channel);

	static void writeChannelSupport(
	    lexxos::os::AppContext& context,
	    lexxos::devices::AudioDevice& device,
	    lexxos::devices::AudioChannel channel);

	static int writeInfo(lexxos::os::AppContext& context, lexxos::devices::AudioDevice& device);

#if LEXXOS_COMMAND_TEST_OPERATIONS
	static int runTone(
	    lexxos::os::AppContext& context,
	    lexxos::devices::AudioDevice& device,
	    lexxos::devices::AudioChannel channel,
	    lexxos::program::ParsedArguments const& arguments);

#endif

	static int run(
	    lexxos::os::AppContext& context,
	    lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const& arguments);
};

} // namespace lexxos::apps::utility_programs
