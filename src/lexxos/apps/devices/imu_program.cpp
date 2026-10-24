#include <lexxos/apps/devices/imu_program.h>

#include <cstdint>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/sensors/sensor.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::device_programs {
namespace {

	constexpr lexxos::program::ProgramArgumentSpec imuArguments[] = {
	    {
         .name = "samples",
         .shortName = 'n',
         .valueName = "count",
         .help = "sample count",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "1",
	     },
	    {
         .name = "interval",
         .shortName = 'i',
         .valueName = "milliseconds",
         .help = "delay between samples",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "100",
	     },
	    {
         .name = "name",
         .valueName = "name",
         .help = "sensor device name",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	};

	void writeSigned(lexxos::os::AppContext& context, std::int32_t value) {
		if (value < 0) {
			context.write("-");
			value = -value;
		}
		writeUnsigned(context, static_cast<std::uint32_t>(value));
	}

	void writeSample(lexxos::os::AppContext& context, lexxos::devices::MotionSensorSample const& sample) {
		context.write("sample: t=");
		writeUnsigned(context, sample.timestampMs);
		context.write("ms chip=");
		writeHex8(context, sample.chipId);
		context.write(" status=");
		writeHex8(context, sample.status);
		context.write(" error=");
		writeHex8(context, sample.error);
		context.write(" accel=");
		writeSigned(context, sample.accelX);
		context.write(",");
		writeSigned(context, sample.accelY);
		context.write(",");
		writeSigned(context, sample.accelZ);
		context.write(" gyro=");
		writeSigned(context, sample.gyroX);
		context.write(",");
		writeSigned(context, sample.gyroY);
		context.write(",");
		writeSigned(context, sample.gyroZ);
		context.write(" temp=");
		writeSigned(context, sample.temperatureCentiC / 100);
		context.write(".");
		auto const fraction = sample.temperatureCentiC < 0 ? -(sample.temperatureCentiC % 100) : sample.temperatureCentiC % 100;
		if (fraction < 10) {
			context.write("0");
		}
		writeUnsigned(context, static_cast<std::uint32_t>(fraction));
		context.writeLine("C");
	}

} // namespace

lexxos::program::ProgramDescriptor ImuProgram::descriptor() {
	return {
	    .name = "imu",
	    .help = "read the primary motion sensor",
	    .description = "Reads samples from a registered MotionSensor, such as the Cardputer-Adv BMI270.",
	    .usage = "imu [-n samples] [-i milliseconds] [name]",
	    .arguments = argumentList(imuArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int ImuProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto const requestedName = arguments.positional("name");
	auto* sensor = !requestedName.empty() ? context.devices().sensorDevice(requestedName) : context.devices().primarySensorDevice();
	if (sensor == nullptr) {
		context.writeErrorLine("imu: motion sensor is not registered");
		return 1;
	}
	if (!sensor->isReady()) {
		context.writeError("imu: sensor is not ready: ");
		context.writeLine(lexxos::devices::toString(sensor->state()));
		return 1;
	}

	auto lock = context.devices().lockSensorDevice(sensor->name(), lexxos::devices::DeviceResourcePriority::Interactive, "imu");
	if (!lock) {
		context.writeErrorLine("imu: sensor device is busy");
		return 1;
	}

	std::uint32_t samples = 1;
	std::uint32_t intervalMs = 100;
	if (!parseUnsigned32(arguments.valueOr("samples", "1"), samples) || samples == 0 || samples > 1000) {
		context.writeErrorLine("imu: invalid sample count");
		return 1;
	}
	if (!parseUnsigned32(arguments.valueOr("interval", "100"), intervalMs) || intervalMs > 60000) {
		context.writeErrorLine("imu: invalid interval");
		return 1;
	}

	auto* motion = sensor->asMotionSensor();
	if (motion == nullptr) {
		context.writeErrorLine("imu: selected sensor is not a motion sensor");
		return 1;
	}
	context.write("imu: device=");
	context.write(sensor->name());
	context.write(" state=");
	context.writeLine(lexxos::devices::toString(sensor->state()));
	for (std::uint32_t index = 0; index < samples; ++index) {
		if (context.pollCancellationRequest()) {
			context.writeLine("imu: canceled");
			return 130;
		}
		lexxos::devices::MotionSensorSample sample{};
		if (!motion->readSample(sample)) {
			context.writeErrorLine("imu: failed to read sample");
			return 1;
		}
		writeSample(context, sample);
		if (index + 1U < samples && intervalMs != 0) {
			lexxos::kernel::sleepMs(intervalMs);
		}
	}
	return 0;
}

} // namespace lexxos::apps::device_programs
