#include <lexxos/apps/devices/gpio_analyzer_program.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string_view>

#include <lexxos/app/program_input.h>
#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/devices/detail/display_program_support.h>
#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/devices/input_device.h>
#include <lexxos/devices/rtc/date_time.h>
#include <lexxos/feature.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/app_context.h>
#include <lexxos/platform/platform.h>
#include <lexxos/vfs/vfs.h>

#if FEATURE_ENABLED(DS3231_RTC)
	#include <lexxos/devices/rtc/ds3231.h>
#endif
#if FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
	#include <lexxos/devices/picocalc/picocalc_southbridge_resource_policy.h>
	#include <lexxos/devices/picocalc/picocalc_southbridge_runtime.h>
#endif

#if FEATURE_ENABLED(PSRAM) && FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PLATFORM_PICO)
	#include <lexxos/apps/picocalc/detail/psram_diagnostics_support.h>
#endif

namespace lexxos::apps::device_programs {
namespace {

	constexpr std::size_t maxChannels = lexxos::platform::maxBoardUserGpioPins;
	constexpr std::size_t internalSampleCapacity = 512;
	constexpr std::size_t psramSampleCapacity = 4096;
	constexpr std::size_t internalRecordingSampleCapacity = 32;
	constexpr char controlNone = '\0';
	constexpr char controlExit = 'q';
	constexpr char controlCancel = '\x03';
	constexpr char controlEscape = '\x1B';
	constexpr char controlPause = ' ';
	constexpr char controlWindowSmaller = '[';
	constexpr char controlWindowLarger = ']';
	constexpr char controlToggleOutput = 't';
	constexpr char controlToggleRecording = 'r';
	constexpr std::uint32_t gpioAnalyzerPsramBaseAddress = 0x00F00000U;
	constexpr std::uint32_t gpioAnalyzerRecordingPsramBaseAddress = 0x00F04000U;
	constexpr std::size_t psramRecordingSampleBytes = 10;
	constexpr std::uint32_t minimumSamplePeriodMs = 5;
	constexpr std::uint32_t maximumSamplePeriodMs = 1000;
	constexpr std::uint32_t minimumWindowMs = 500;
	constexpr std::uint32_t maximumWindowMs = 60000;

	constexpr std::string_view gpioAnalyzerRecordingDirectory{"/mnt/sd/apps/gpio-analyzer"};

	constexpr lexxos::program::ProgramArgumentSpec gpioAnalyzerArguments[] = {
	    {
         .name = "in",
         .shortName = 'i',
         .valueName = "pins",
         .help = "experimental: comma separated input pins or labels, for example GP8,GP9 or 8,9",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "out",
         .shortName = 'o',
         .valueName = "assignments",
         .help = "comma separated output assignments, for example GP12=1,GP28=0",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "sample",
         .shortName = 's',
         .valueName = "milliseconds",
         .help = "initial sampling period in milliseconds",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "20",
	     },
	    {
         .name = "window",
         .shortName = 'w',
         .valueName = "milliseconds",
         .help = "initial time window shown on screen in milliseconds",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "5000",
	     },
	    {
         .name = "pull",
         .shortName = 'p',
         .valueName = "board|none|up|down",
         .help = "pull mode applied to selected inputs; board uses each pin default",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
         .defaultValue = "board",
	     },
	    {
         .name = "display",
         .valueName = "name",
         .help = "display device name",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	};

	constexpr std::uint16_t palette[16] = {
	    0xFFFF, // white
	    0xF800, // red
	    0x07E0, // green
	    0x001F, // blue
	    0xFFE0, // yellow
	    0xF81F, // magenta
	    0x07FF, // cyan
	    0xFC00, // orange
	    0x8410, // gray
	    0xFBE0, // warm yellow
	    0x87F0, // mint
	    0x8010, // purple
	    0xAFE5, // light cyan
	    0xF81F, // pink
	    0x07EF, // teal
	    0xC618, // light gray
	};

	struct InputChannel {
		lexxos::platform::BoardUserGpioPin const* descriptor{nullptr};
		std::uint16_t color{0xFFFF};
		bool visible{true};
	};

	struct OutputChannel {
		lexxos::platform::BoardUserGpioPin const* descriptor{nullptr};
		bool value{false};
	};

	struct SampleBuffer {
		static_assert(maxChannels <= 16, "gpio-analyzer sample masks store up to 16 channels");

		std::uint16_t masks[internalSampleCapacity]{};
		std::size_t writeIndex{0};
		std::size_t count{0};
		std::size_t capacity{internalSampleCapacity};
		bool psram{false};
		lexxos::platform::PsramPins const* psramPins{nullptr};

		[[nodiscard]] bool initialize(lexxos::os::AppContext& context, lexxos::platform::BoardDescriptor const& board) {
			psram = false;
			psramPins = nullptr;
			capacity = internalSampleCapacity;
			clear();

#if FEATURE_ENABLED(PSRAM) && FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PLATFORM_PICO)
			if (board.psram.spi.isValid()) {
				psramPins = &board.psram;
				constexpr std::uint16_t probeMask = 0xA55A;
				std::uint16_t actualMask = 0;
				if (writePsramMask(0, probeMask) && readPsramMask(0, actualMask) && actualMask == probeMask) {
					psram = true;
					capacity = psramSampleCapacity;
					clear();
					context.writeLine("gpio-analyzer: history storage=psram samples=4096");
					return true;
				}
				psramPins = nullptr;
				context.writeLine("gpio-analyzer: PSRAM history unavailable, using internal history");
			}
#else
			(void) board;
#endif

			context.writeLine("gpio-analyzer: history storage=internal samples=512");
			clear();
			return true;
		}

		void clear() {
			if (!psram) {
				for (auto& mask : masks) {
					mask = 0;
				}
			}
			writeIndex = 0;
			count = 0;
		}

		[[nodiscard]] std::size_t sampleCapacity() const {
			return capacity;
		}

		[[nodiscard]] bool usesPsram() const {
			return psram;
		}

#if FEATURE_ENABLED(PSRAM) && FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PLATFORM_PICO)
		[[nodiscard]] bool writePsramMask(std::size_t index, std::uint16_t mask) const {
			if (psramPins == nullptr || index >= psramSampleCapacity) {
				return false;
			}
			std::uint8_t bytes[2]{
			    static_cast<std::uint8_t>(mask & 0xFFU),
			    static_cast<std::uint8_t>((mask >> 8U) & 0xFFU),
			};
			return lexxos::apps::picocalc_programs::detail::writePsramMemory(
			    *psramPins,
			    gpioAnalyzerPsramBaseAddress + static_cast<std::uint32_t>(index * sizeof(std::uint16_t)),
			    bytes,
			    sizeof(bytes));
		}

		[[nodiscard]] bool readPsramMask(std::size_t index, std::uint16_t& mask) const {
			if (psramPins == nullptr || index >= psramSampleCapacity) {
				return false;
			}
			std::uint8_t bytes[2]{};
			if (!lexxos::apps::picocalc_programs::detail::readPsramMemory(
			        *psramPins,
			        gpioAnalyzerPsramBaseAddress + static_cast<std::uint32_t>(index * sizeof(std::uint16_t)),
			        bytes,
			        sizeof(bytes))) {
				return false;
			}
			mask = static_cast<std::uint16_t>(bytes[0] | (static_cast<std::uint16_t>(bytes[1]) << 8U));
			return true;
		}
#endif

		[[nodiscard]] bool storeMask(std::size_t index, std::uint16_t mask) {
			if (index >= capacity) {
				return false;
			}
			if (psram) {
#if FEATURE_ENABLED(PSRAM) && FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PLATFORM_PICO)
				return writePsramMask(index, mask);
#else
				return false;
#endif
			}
			masks[index] = mask;
			return true;
		}

		[[nodiscard]] bool loadMask(std::size_t index, std::uint16_t& mask) const {
			if (index >= capacity) {
				return false;
			}
			if (psram) {
#if FEATURE_ENABLED(PSRAM) && FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PLATFORM_PICO)
				return readPsramMask(index, mask);
#else
				return false;
#endif
			}
			mask = masks[index];
			return true;
		}

		[[nodiscard]] bool push(InputChannel const* channels, std::size_t channelCount, std::uint16_t* storedMask = nullptr) {
			std::uint16_t mask = 0;
			for (std::size_t channelIndex = 0; channelIndex < channelCount; ++channelIndex) {
				bool value = false;
				if (channels[channelIndex].descriptor != nullptr) {
					(void) lexxos::platform::readGpio(channels[channelIndex].descriptor->pin, value);
				}

				if (value) {
					mask |= static_cast<std::uint16_t>(1U << channelIndex);
				}
			}

			if (storedMask != nullptr) {
				*storedMask = mask;
			}

			if (!storeMask(writeIndex, mask)) {
				return false;
			}

			writeIndex = (writeIndex + 1U) % capacity;
			if (count < capacity) {
				++count;
			}
			return true;
		}

		[[nodiscard]] bool maskAt(std::size_t sampleOffsetFromOldest, std::uint16_t& mask) const {
			if (count == 0 || sampleOffsetFromOldest >= count) {
				return false;
			}
			auto const oldest = (writeIndex + capacity - count) % capacity;
			auto const index = (oldest + sampleOffsetFromOldest) % capacity;
			return loadMask(index, mask);
		}

		[[nodiscard]] bool at(std::size_t channelIndex, std::size_t sampleOffsetFromOldest) const {
			if (count == 0 || channelIndex >= maxChannels || channelIndex >= 16U) {
				return false;
			}

			std::uint16_t mask = 0;
			return maskAt(sampleOffsetFromOldest, mask) && (mask & static_cast<std::uint16_t>(1U << channelIndex)) != 0;
		}
	};

	struct TextBuffer {
		char data[96]{};
		std::size_t size{0};

		void clear() {
			size = 0;
			data[0] = '\0';
		}

		void append(std::string_view text) {
			for (char const character : text) {
				if (size + 1U >= sizeof(data)) {
					break;
				}
				data[size++] = character;
			}
			data[size] = '\0';
		}

		void appendUnsigned(std::uint32_t value) {
			char reversed[10]{};
			std::size_t count = 0;
			do {
				reversed[count++] = static_cast<char>('0' + (value % 10U));
				value /= 10U;
			} while (value != 0 && count < sizeof(reversed));
			while (count > 0) {
				char const character = reversed[--count];
				append(std::string_view{&character, 1});
			}
		}

		[[nodiscard]] std::string_view view() const {
			return {data, size};
		}
	};

	struct RecordingSample {
		std::uint32_t offsetMs{0};
		std::uint32_t uptimeMs{0};
		std::uint16_t mask{0};
	};

	struct RecordingStartTime {
		bool realtime{false};
		lexxos::devices::rtc::DateTime dateTime{};
		std::uint32_t uptimeMs{0};
	};

	[[nodiscard]] char hexDigit(std::uint8_t value) {
		return static_cast<char>(value < 10U ? '0' + value : 'A' + value - 10U);
	}

	template <std::size_t Capacity>
	struct FixedTextBuffer {
		char data[Capacity]{};
		std::size_t size{0};

		void clear() {
			size = 0;
			data[0] = '\0';
		}

		[[nodiscard]] bool full() const {
			return size + 1U >= Capacity;
		}

		void append(char character) {
			if (full()) {
				return;
			}
			data[size++] = character;
			data[size] = '\0';
		}

		void append(std::string_view text) {
			for (char const character : text) {
				append(character);
				if (full()) {
					break;
				}
			}
		}

		void appendUnsigned(std::uint32_t value) {
			char reversed[10]{};
			std::size_t count = 0;
			do {
				reversed[count++] = static_cast<char>('0' + (value % 10U));
				value /= 10U;
			} while (value != 0 && count < sizeof(reversed));
			while (count > 0) {
				append(reversed[--count]);
			}
		}

		void appendUnsigned64(std::uint64_t value) {
			char reversed[20]{};
			std::size_t count = 0;
			do {
				reversed[count++] = static_cast<char>('0' + (value % 10U));
				value /= 10U;
			} while (value != 0 && count < sizeof(reversed));
			while (count > 0) {
				append(reversed[--count]);
			}
		}

		void appendPadded2(std::uint8_t value) {
			append(static_cast<char>('0' + ((value / 10U) % 10U)));
			append(static_cast<char>('0' + (value % 10U)));
		}

		void appendPadded4(std::uint16_t value) {
			append(static_cast<char>('0' + ((value / 1000U) % 10U)));
			append(static_cast<char>('0' + ((value / 100U) % 10U)));
			append(static_cast<char>('0' + ((value / 10U) % 10U)));
			append(static_cast<char>('0' + (value % 10U)));
		}

		void appendHex4(std::uint16_t value) {
			append(hexDigit(static_cast<std::uint8_t>((value >> 12U) & 0x0FU)));
			append(hexDigit(static_cast<std::uint8_t>((value >> 8U) & 0x0FU)));
			append(hexDigit(static_cast<std::uint8_t>((value >> 4U) & 0x0FU)));
			append(hexDigit(static_cast<std::uint8_t>(value & 0x0FU)));
		}

		[[nodiscard]] std::string_view view() const {
			return {data, size};
		}

		[[nodiscard]] char const* c_str() const {
			return data;
		}
	};

	template <std::size_t Capacity>
	void appendDateTime(FixedTextBuffer<Capacity>& buffer, lexxos::devices::rtc::DateTime const& value, bool filenameSafe) {
		buffer.appendPadded4(value.year);
		buffer.append(filenameSafe ? "" : "-");
		buffer.appendPadded2(value.month);
		buffer.append(filenameSafe ? "" : "-");
		buffer.appendPadded2(value.day);
		buffer.append(filenameSafe ? "_" : " ");
		buffer.appendPadded2(value.hour);
		buffer.append(filenameSafe ? "" : ":");
		buffer.appendPadded2(value.minute);
		buffer.append(filenameSafe ? "" : ":");
		buffer.appendPadded2(value.second);
	}

	[[nodiscard]] bool tryReadPicoCalcDateTime(lexxos::os::AppContext& context, lexxos::devices::rtc::DateTime& output) {
#if FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
		auto* service = lexxos::devices::PicoCalcSouthbridgeResourcePolicy::readyService(context.devices());
		if (service == nullptr) {
			return false;
		}

		auto lock = lexxos::devices::PicoCalcSouthbridgeResourcePolicy::lock(
		    context.devices(),
		    "gpio-analyzer",
		    lexxos::devices::DeviceResourcePriority::Interactive);
		if (!lock) {
			return false;
		}
		return service->readJcsRtcDateTime(output) && lexxos::devices::rtc::isValidDate(output);
#else
		(void) context;
		(void) output;
		return false;
#endif
	}

	[[nodiscard]] bool tryReadDs3231DateTime(lexxos::os::AppContext& context, lexxos::devices::rtc::DateTime& output) {
#if FEATURE_ENABLED(DS3231_RTC)
		constexpr std::string_view busName{"debug"};
		auto* bus = context.devices().i2cBus(busName);
		if (bus == nullptr || !bus->isReady()) {
			return false;
		}

		auto lock = context.devices().lockI2cBus(busName, lexxos::devices::DeviceResourcePriority::Interactive, "gpio-analyzer");
		if (!lock) {
			return false;
		}

		lexxos::devices::rtc::Ds3231Rtc rtc{*bus};
		return rtc.probe() && rtc.readDateTime(output) && lexxos::devices::rtc::isValidDate(output);
#else
		(void) context;
		(void) output;
		return false;
#endif
	}

	[[nodiscard]] bool readRecordingDateTime(lexxos::os::AppContext& context, lexxos::devices::rtc::DateTime& output) {
		return tryReadPicoCalcDateTime(context, output) || tryReadDs3231DateTime(context, output);
	}

	struct RecordingBuffer {
		RecordingSample samples[internalRecordingSampleCapacity]{};
		std::size_t count{0};
		std::size_t capacity{internalRecordingSampleCapacity};
		bool psram{false};
		bool active{false};
		bool overflowed{false};
		RecordingStartTime start{};
		lexxos::platform::PsramPins const* psramPins{nullptr};

		[[nodiscard]] bool initialize(lexxos::os::AppContext& context, lexxos::platform::BoardDescriptor const& board) {
			psram = false;
			psramPins = nullptr;
			capacity = internalRecordingSampleCapacity;
			clear();

#if FEATURE_ENABLED(PSRAM) && FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PLATFORM_PICO)
			if (board.psram.spi.isValid()) {
				psramPins = &board.psram;
				RecordingSample probe{.offsetMs = 0x12345678U, .uptimeMs = 0x87654321U, .mask = 0xA55AU};
				RecordingSample actual{};
				if (writePsramSample(0, probe) && readPsramSample(0, actual) && actual.offsetMs == probe.offsetMs && actual.uptimeMs == probe.uptimeMs && actual.mask == probe.mask) {
					psram = true;
					capacity = psramSampleCapacity;
					clear();
					context.writeLine("gpio-analyzer: recording storage=psram samples=4096");
					return true;
				}
				psramPins = nullptr;
				context.writeLine("gpio-analyzer: PSRAM recording unavailable, using internal recording buffer");
			}
#else
			(void) board;
#endif

			context.writeLine("gpio-analyzer: recording storage=internal samples=32");
			clear();
			return true;
		}

		void clear() {
			if (!psram) {
				for (auto& sample : samples) {
					sample = {};
				}
			}
			count = 0;
			active = false;
			overflowed = false;
			start = {};
		}

		[[nodiscard]] bool startCapture(lexxos::os::AppContext& context, std::uint32_t nowMs) {
			clear();
			start.uptimeMs = nowMs;
			if (lexxos::devices::rtc::DateTime value{}; readRecordingDateTime(context, value)) {
				start.realtime = true;
				start.dateTime = value;
			}
			active = true;
			return true;
		}

		void stopCapture() {
			active = false;
		}

#if FEATURE_ENABLED(PSRAM) && FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PLATFORM_PICO)
		[[nodiscard]] bool writePsramSample(std::size_t index, RecordingSample const& sample) const {
			if (psramPins == nullptr || index >= psramSampleCapacity) {
				return false;
			}
			std::uint8_t bytes[psramRecordingSampleBytes]{
			    static_cast<std::uint8_t>(sample.offsetMs & 0xFFU),
			    static_cast<std::uint8_t>((sample.offsetMs >> 8U) & 0xFFU),
			    static_cast<std::uint8_t>((sample.offsetMs >> 16U) & 0xFFU),
			    static_cast<std::uint8_t>((sample.offsetMs >> 24U) & 0xFFU),
			    static_cast<std::uint8_t>(sample.uptimeMs & 0xFFU),
			    static_cast<std::uint8_t>((sample.uptimeMs >> 8U) & 0xFFU),
			    static_cast<std::uint8_t>((sample.uptimeMs >> 16U) & 0xFFU),
			    static_cast<std::uint8_t>((sample.uptimeMs >> 24U) & 0xFFU),
			    static_cast<std::uint8_t>(sample.mask & 0xFFU),
			    static_cast<std::uint8_t>((sample.mask >> 8U) & 0xFFU),
			};
			return lexxos::apps::picocalc_programs::detail::writePsramMemory(
			    *psramPins,
			    gpioAnalyzerRecordingPsramBaseAddress + static_cast<std::uint32_t>(index * psramRecordingSampleBytes),
			    bytes,
			    sizeof(bytes));
		}

		[[nodiscard]] bool readPsramSample(std::size_t index, RecordingSample& sample) const {
			if (psramPins == nullptr || index >= psramSampleCapacity) {
				return false;
			}
			std::uint8_t bytes[psramRecordingSampleBytes]{};
			if (!lexxos::apps::picocalc_programs::detail::readPsramMemory(
			        *psramPins,
			        gpioAnalyzerRecordingPsramBaseAddress + static_cast<std::uint32_t>(index * psramRecordingSampleBytes),
			        bytes,
			        sizeof(bytes))) {
				return false;
			}
			sample.offsetMs = static_cast<std::uint32_t>(bytes[0])
			    | (static_cast<std::uint32_t>(bytes[1]) << 8U)
			    | (static_cast<std::uint32_t>(bytes[2]) << 16U)
			    | (static_cast<std::uint32_t>(bytes[3]) << 24U);
			sample.uptimeMs = static_cast<std::uint32_t>(bytes[4])
			    | (static_cast<std::uint32_t>(bytes[5]) << 8U)
			    | (static_cast<std::uint32_t>(bytes[6]) << 16U)
			    | (static_cast<std::uint32_t>(bytes[7]) << 24U);
			sample.mask = static_cast<std::uint16_t>(bytes[8] | (static_cast<std::uint16_t>(bytes[9]) << 8U));
			return true;
		}
#endif

		[[nodiscard]] bool storeSample(std::size_t index, RecordingSample const& sample) {
			if (index >= capacity) {
				return false;
			}
			if (psram) {
#if FEATURE_ENABLED(PSRAM) && FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PLATFORM_PICO)
				return writePsramSample(index, sample);
#else
				return false;
#endif
			}
			samples[index] = sample;
			return true;
		}

		[[nodiscard]] bool loadSample(std::size_t index, RecordingSample& sample) const {
			if (index >= count || index >= capacity) {
				return false;
			}
			if (psram) {
#if FEATURE_ENABLED(PSRAM) && FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PLATFORM_PICO)
				return readPsramSample(index, sample);
#else
				return false;
#endif
			}
			sample = samples[index];
			return true;
		}

		[[nodiscard]] bool push(std::uint32_t nowMs, std::uint16_t mask) {
			if (!active) {
				return true;
			}
			if (count >= capacity) {
				overflowed = true;
				active = false;
				return false;
			}
			RecordingSample sample{
			    .offsetMs = nowMs - start.uptimeMs,
			    .uptimeMs = nowMs,
			    .mask = mask,
			};
			if (!storeSample(count, sample)) {
				overflowed = true;
				active = false;
				return false;
			}
			++count;
			return true;
		}
	};

	[[nodiscard]] char asciiLower(char value) {
		return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
	}

	[[nodiscard]] bool equalsIgnoreCase(std::string_view left, std::string_view right) {
		if (left.size() != right.size()) {
			return false;
		}
		for (std::size_t index = 0; index < left.size(); ++index) {
			if (asciiLower(left[index]) != asciiLower(right[index])) {
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] std::string_view trim(std::string_view value) {
		while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
			value.remove_prefix(1);
		}
		while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
			value.remove_suffix(1);
		}
		return value;
	}

	[[nodiscard]] bool parseUnsignedPinNumber(std::string_view text, int& number) {
		text = trim(text);
		if (text.size() >= 4 && asciiLower(text[0]) == 'g' && asciiLower(text[1]) == 'p' && asciiLower(text[2]) == 'i' && asciiLower(text[3]) == 'o') {
			text.remove_prefix(4);
		} else if (text.size() >= 2 && asciiLower(text[0]) == 'g' && asciiLower(text[1]) == 'p') {
			text.remove_prefix(2);
		}
		std::uint32_t value = 0;
		if (!parseUnsigned32(text, value) || value > 255U) {
			return false;
		}
		number = static_cast<int>(value);
		return true;
	}

	[[nodiscard]] lexxos::platform::BoardUserGpioPin const* findUserGpioPin(
	    lexxos::platform::BoardDescriptor const& board,
	    std::string_view text) {
		text = trim(text);
		if (text.empty()) {
			return nullptr;
		}

		for (std::uint8_t index = 0; index < board.userGpio.count && index < lexxos::platform::maxBoardUserGpioPins; ++index) {
			auto const& pin = board.userGpio.pins[index];
			if (pin.isValid() && pin.label != nullptr && equalsIgnoreCase(text, pin.label)) {
				return &pin;
			}
		}

		int number = -1;
		if (!parseUnsignedPinNumber(text, number)) {
			return nullptr;
		}
		for (std::uint8_t index = 0; index < board.userGpio.count && index < lexxos::platform::maxBoardUserGpioPins; ++index) {
			auto const& pin = board.userGpio.pins[index];
			if (pin.isValid() && pin.pin.number == number) {
				return &pin;
			}
		}
		return nullptr;
	}

	[[nodiscard]] bool channelAlreadySelected(
	    InputChannel const* channels,
	    std::size_t count,
	    lexxos::platform::BoardUserGpioPin const* pin) {
		for (std::size_t index = 0; index < count; ++index) {
			if (channels[index].descriptor == pin) {
				return true;
			}
		}
		return false;
	}

	[[nodiscard]] bool outputAlreadySelected(
	    OutputChannel const* channels,
	    std::size_t count,
	    lexxos::platform::BoardUserGpioPin const* pin) {
		for (std::size_t index = 0; index < count; ++index) {
			if (channels[index].descriptor == pin) {
				return true;
			}
		}
		return false;
	}

	struct PullSelection {
		bool useBoardDefault{true};
		lexxos::platform::GpioPull overridePull{lexxos::platform::GpioPull::None};

		[[nodiscard]] lexxos::platform::GpioPull forPin(lexxos::platform::BoardUserGpioPin const& pin) const {
			return useBoardDefault ? pin.defaultPull : overridePull;
		}
	};

	[[nodiscard]] bool parsePullSelection(std::string_view text, PullSelection& selection) {
		text = trim(text);
		if (equalsIgnoreCase(text, "board") || equalsIgnoreCase(text, "default")) {
			selection = PullSelection{};
			return true;
		}
		if (equalsIgnoreCase(text, "none")) {
			selection.useBoardDefault = false;
			selection.overridePull = lexxos::platform::GpioPull::None;
			return true;
		}
		if (equalsIgnoreCase(text, "up")) {
			selection.useBoardDefault = false;
			selection.overridePull = lexxos::platform::GpioPull::Up;
			return true;
		}
		if (equalsIgnoreCase(text, "down")) {
			selection.useBoardDefault = false;
			selection.overridePull = lexxos::platform::GpioPull::Down;
			return true;
		}
		return false;
	}

	[[nodiscard]] std::string_view nextCommaToken(std::string_view list, std::size_t& offset) {
		if (offset >= list.size()) {
			return {};
		}
		auto const start = offset;
		while (offset < list.size() && list[offset] != ',') {
			++offset;
		}
		auto token = list.substr(start, offset - start);
		if (offset < list.size() && list[offset] == ',') {
			++offset;
		}
		return trim(token);
	}

	[[nodiscard]] bool parseInputChannels(
	    lexxos::os::AppContext& context,
	    lexxos::platform::BoardDescriptor const& board,
	    std::string_view text,
	    InputChannel* channels,
	    std::size_t& channelCount) {
		channelCount = 0;
		if (text.empty()) {
			for (std::uint8_t index = 0; index < board.userGpio.count && index < lexxos::platform::maxBoardUserGpioPins; ++index) {
				auto const& pin = board.userGpio.pins[index];
				if (!pin.isValid() || !pin.inputAllowed || !pin.defaultVisible) {
					continue;
				}
				channels[channelCount] = {.descriptor = &pin, .color = palette[channelCount % std::size(palette)], .visible = true};
				++channelCount;
				if (channelCount >= maxChannels) {
					break;
				}
			}
			return channelCount > 0;
		}

		std::size_t offset = 0;
		while (offset < text.size()) {
			auto const token = nextCommaToken(text, offset);
			if (token.empty()) {
				continue;
			}
			auto const* pin = findUserGpioPin(board, token);
			if (pin == nullptr) {
				context.writeError("gpio-analyzer: unknown user gpio pin: ");
				context.writeLine(token);
				return false;
			}
			if (!pin->inputAllowed) {
				context.writeError("gpio-analyzer: pin is not input-capable: ");
				context.writeLine(pin->label);
				return false;
			}
			if (channelAlreadySelected(channels, channelCount, pin)) {
				continue;
			}
			if (channelCount >= maxChannels) {
				context.writeErrorLine("gpio-analyzer: too many input channels");
				return false;
			}
			channels[channelCount] = {.descriptor = pin, .color = palette[channelCount % std::size(palette)], .visible = true};
			++channelCount;
		}
		return channelCount > 0;
	}

	[[nodiscard]] bool parseOutputChannels(
	    lexxos::os::AppContext& context,
	    lexxos::platform::BoardDescriptor const& board,
	    std::string_view text,
	    OutputChannel* channels,
	    std::size_t& channelCount) {
		channelCount = 0;
		if (text.empty()) {
			return true;
		}

		std::size_t offset = 0;
		while (offset < text.size()) {
			auto const token = nextCommaToken(text, offset);
			if (token.empty()) {
				continue;
			}
			auto const separator = token.find('=');
			if (separator == std::string_view::npos) {
				context.writeError("gpio-analyzer: output assignment must use pin=value: ");
				context.writeLine(token);
				return false;
			}

			auto const pinName = trim(token.substr(0, separator));
			auto const valueText = trim(token.substr(separator + 1));
			auto const* pin = findUserGpioPin(board, pinName);
			if (pin == nullptr) {
				context.writeError("gpio-analyzer: unknown output pin: ");
				context.writeLine(pinName);
				return false;
			}
			if (!pin->outputAllowed) {
				context.writeError("gpio-analyzer: pin is not output-capable: ");
				context.writeLine(pin->label);
				return false;
			}
			if (outputAlreadySelected(channels, channelCount, pin)) {
				context.writeError("gpio-analyzer: duplicate output pin: ");
				context.writeLine(pin->label);
				return false;
			}
			if (channelCount >= maxChannels) {
				context.writeErrorLine("gpio-analyzer: too many output channels");
				return false;
			}

			bool value = false;
			if (equalsIgnoreCase(valueText, "1") || equalsIgnoreCase(valueText, "high") || equalsIgnoreCase(valueText, "on") || equalsIgnoreCase(valueText, "true")) {
				value = true;
			} else if (equalsIgnoreCase(valueText, "0") || equalsIgnoreCase(valueText, "low") || equalsIgnoreCase(valueText, "off") || equalsIgnoreCase(valueText, "false")) {
				value = false;
			} else {
				context.writeError("gpio-analyzer: invalid output value: ");
				context.writeLine(valueText);
				return false;
			}

			channels[channelCount] = {.descriptor = pin, .value = value};
			++channelCount;
		}
		return true;
	}

	[[nodiscard]] bool inputOutputOverlap(InputChannel const* inputs, std::size_t inputCount, OutputChannel const* outputs, std::size_t outputCount) {
		for (std::size_t inputIndex = 0; inputIndex < inputCount; ++inputIndex) {
			for (std::size_t outputIndex = 0; outputIndex < outputCount; ++outputIndex) {
				if (inputs[inputIndex].descriptor == outputs[outputIndex].descriptor) {
					return true;
				}
			}
		}
		return false;
	}

	[[nodiscard]] bool glyphRows(char character, std::uint8_t (&rows)[7]) {
		if (character >= 'a' && character <= 'z') {
			character = static_cast<char>(character - 'a' + 'A');
		}
		auto set = [&](std::uint8_t r0, std::uint8_t r1, std::uint8_t r2, std::uint8_t r3, std::uint8_t r4, std::uint8_t r5, std::uint8_t r6) {
			rows[0] = r0;
			rows[1] = r1;
			rows[2] = r2;
			rows[3] = r3;
			rows[4] = r4;
			rows[5] = r5;
			rows[6] = r6;
		};
		switch (character) {
			case ' ': set(0, 0, 0, 0, 0, 0, 0); return true;
			case '0': set(0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E); return true;
			case '1': set(0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E); return true;
			case '2': set(0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F); return true;
			case '3': set(0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E); return true;
			case '4': set(0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02); return true;
			case '5': set(0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E); return true;
			case '6': set(0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E); return true;
			case '7': set(0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08); return true;
			case '8': set(0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E); return true;
			case '9': set(0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C); return true;
			case 'A': set(0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11); return true;
			case 'B': set(0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E); return true;
			case 'C': set(0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E); return true;
			case 'D': set(0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E); return true;
			case 'E': set(0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F); return true;
			case 'F': set(0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10); return true;
			case 'G': set(0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F); return true;
			case 'H': set(0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11); return true;
			case 'I': set(0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E); return true;
			case 'J': set(0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C); return true;
			case 'K': set(0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11); return true;
			case 'L': set(0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F); return true;
			case 'M': set(0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11); return true;
			case 'N': set(0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11); return true;
			case 'O': set(0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E); return true;
			case 'P': set(0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10); return true;
			case 'Q': set(0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D); return true;
			case 'R': set(0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11); return true;
			case 'S': set(0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E); return true;
			case 'T': set(0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04); return true;
			case 'U': set(0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E); return true;
			case 'V': set(0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04); return true;
			case 'W': set(0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A); return true;
			case 'X': set(0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11); return true;
			case 'Y': set(0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04); return true;
			case 'Z': set(0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F); return true;
			case '-': set(0, 0, 0, 0x1F, 0, 0, 0); return true;
			case '_': set(0, 0, 0, 0, 0, 0, 0x1F); return true;
			case '.': set(0, 0, 0, 0, 0, 0x0C, 0x0C); return true;
			case ':': set(0, 0x0C, 0x0C, 0, 0x0C, 0x0C, 0); return true;
			case '/': set(0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10); return true;
			case '=': set(0, 0x1F, 0, 0x1F, 0, 0, 0); return true;
			case '+': set(0, 0x04, 0x04, 0x1F, 0x04, 0x04, 0); return true;
			case '[': set(0x07, 0x04, 0x04, 0x04, 0x04, 0x04, 0x07); return true;
			case ']': set(0x1C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1C); return true;
			case '*': set(0, 0x15, 0x0E, 0x1F, 0x0E, 0x15, 0); return true;
			default : set(0x1F, 0x11, 0x05, 0x02, 0x04, 0, 0x04); return false;
		}
	}

	void drawTextOnLine(
	    std::uint16_t* line,
	    int lineWidth,
	    int y,
	    int textX,
	    int textY,
	    int scale,
	    std::uint16_t color,
	    std::string_view text) {
		if (scale <= 0 || y < textY || y >= textY + 7 * scale || textX >= lineWidth) {
			return;
		}
		auto const sourceRow = (y - textY) / scale;
		int x = textX;
		for (char const character : text) {
			std::uint8_t rows[7]{};
			(void) glyphRows(character, rows);
			auto const row = rows[sourceRow];
			for (int column = 0; column < 5; ++column) {
				auto const lit = (row & (1U << (4 - column))) != 0;
				for (int sx = 0; sx < scale; ++sx) {
					if (lit && x >= 0 && x < lineWidth) {
						line[x] = color;
					}
					++x;
				}
			}
			x += scale;
			if (x >= lineWidth) {
				break;
			}
		}
	}

	[[nodiscard]] std::size_t visibleInputCount(InputChannel const* channels, std::size_t channelCount) {
		std::size_t count = 0;
		for (std::size_t index = 0; index < channelCount; ++index) {
			if (channels[index].visible) {
				++count;
			}
		}
		return count;
	}

	struct GraphSampleCache {
		std::uint16_t masks[detail::displayScratchLinePixels]{};
		bool valid[detail::displayScratchLinePixels]{};
		std::size_t width{0};

		void clear(std::size_t requestedWidth) {
			width = std::min(requestedWidth, static_cast<std::size_t>(detail::displayScratchLinePixels));
			for (std::size_t index = 0; index < width; ++index) {
				masks[index] = 0;
				valid[index] = false;
			}
		}

		[[nodiscard]] bool value(std::size_t channelIndex, std::size_t x) const {
			if (x >= width || !valid[x] || channelIndex >= 16U) {
				return false;
			}
			return (masks[x] & static_cast<std::uint16_t>(1U << channelIndex)) != 0;
		}
	};

	[[nodiscard]] std::size_t sampleWindowSize(std::uint32_t windowMs, std::uint32_t sampleMs, std::size_t capacity) {
		auto samples = static_cast<std::size_t>((windowMs + sampleMs - 1U) / sampleMs);
		if (samples < 2U) {
			samples = 2U;
		}

		if (samples > capacity) {
			samples = capacity;
		}
		return samples;
	}

	[[nodiscard]] bool prepareGraphSampleCache(
	    SampleBuffer const& samples,
	    std::uint32_t windowMs,
	    std::uint32_t sampleMs,
	    std::size_t graphWidth,
	    GraphSampleCache& cache) {
		cache.clear(graphWidth);
		if (cache.width == 0 || samples.count == 0) {
			return true;
		}

		auto const windowSamples = sampleWindowSize(windowMs, sampleMs, samples.sampleCapacity());
		auto const availableSamples = std::min(windowSamples, samples.count);
		auto const missingLeadingSamples = windowSamples - availableSamples;
		for (std::size_t x = 0; x < cache.width; ++x) {
			auto const sampleInWindow = static_cast<std::size_t>(
			    (static_cast<std::uint32_t>(x) * static_cast<std::uint32_t>(windowSamples)) / static_cast<std::uint32_t>(cache.width));
			if (sampleInWindow < missingLeadingSamples) {
				continue;
			}

			auto offsetInAvailable = sampleInWindow - missingLeadingSamples;
			if (offsetInAvailable >= availableSamples) {
				offsetInAvailable = availableSamples - 1U;
			}
			auto const sampleOffsetFromOldest = samples.count - availableSamples + offsetInAvailable;
			if (!samples.maskAt(sampleOffsetFromOldest, cache.masks[x])) {
				return false;
			}
			cache.valid[x] = true;
		}
		return true;
	}

	[[nodiscard]] bool renderFrame(
	    lexxos::devices::DisplayDevice& display,
	    InputChannel const* inputs,
	    std::size_t inputCount,
	    OutputChannel const* outputs,
	    std::size_t outputCount,
	    std::size_t selectedOutput,
	    SampleBuffer const& samples,
	    std::uint32_t sampleMs,
	    std::uint32_t windowMs,
	    bool paused,
	    RecordingBuffer const& recording) {
		auto const width = display.width();
		auto const height = display.height();
		if (width <= 0 || height <= 0 || width > detail::displayScratchLinePixels) {
			return false;
		}

		auto* line = detail::displayScratchLine();
		constexpr std::uint16_t background = 0x0000;
		constexpr std::uint16_t dim = 0x3186;
		constexpr std::uint16_t text = 0xFFFF;
		constexpr int headerHeight = 16;
		constexpr int footerHeight = 24;
		constexpr int leftMargin = 54;
		auto const graphTop = headerHeight + 2;
		auto const graphBottom = height - footerHeight - 2;
		auto const graphHeight = std::max(1, graphBottom - graphTop);
		auto const graphWidth = std::max(1, width - leftMargin - 2);
		auto const visibleCount = visibleInputCount(inputs, inputCount);
		GraphSampleCache graphCache;

		if (!prepareGraphSampleCache(samples, windowMs, sampleMs, static_cast<std::size_t>(graphWidth), graphCache)) {
			return false;
		}

		TextBuffer status;
		status.append("GPIO S=");
		status.appendUnsigned(sampleMs);
		status.append(" W=");
		status.appendUnsigned(windowMs / 1000U);
		status.append("s ");
		status.append(paused ? "PAUSE" : "RUN");
		status.append(samples.usesPsram() ? " PS" : " RAM");

		if (recording.active) {
			status.append(" REC=");
			status.appendUnsigned(static_cast<std::uint32_t>(recording.count));
		} else if (recording.count > 0) {
			status.append(recording.overflowed ? " FULL=" : " SAVED=");
			status.appendUnsigned(static_cast<std::uint32_t>(recording.count));
		}

		TextBuffer footer;
		footer.append("SPACE PAUSE +/-S []W R REC 1-F IN Q EXIT");

		TextBuffer outputText;
		if (outputCount > 0) {
			outputText.append("OUT ");
			auto const& selected = outputs[selectedOutput % outputCount];
			outputText.append(selected.descriptor != nullptr && selected.descriptor->label != nullptr ? selected.descriptor->label : "?");
			outputText.append("=");
			outputText.append(selected.value ? "1" : "0");
			outputText.append(" T TOGGLE N NEXT");
		}

		if (!display.beginFrame()) {
			return false;
		}

		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				line[x] = background;
			}

			if (y == headerHeight || y == graphBottom || y == height - footerHeight) {
				for (int x = 0; x < width; ++x) {
					line[x] = dim;
				}
			}

			drawTextOnLine(line, width, y, 2, 4, 1, text, status.view());
			drawTextOnLine(line, width, y, 2, height - 20, 1, text, footer.view());
			drawTextOnLine(line, width, y, 2, height - 10, 1, text, outputText.view());

			if (visibleCount > 0 && y >= graphTop && y < graphBottom) {
				auto const laneHeight = std::max(8, graphHeight / static_cast<int>(visibleCount));
				std::size_t visibleIndex = 0;
				for (std::size_t channelIndex = 0; channelIndex < inputCount; ++channelIndex) {
					auto const& channel = inputs[channelIndex];
					if (!channel.visible) {
						continue;
					}
					auto const laneTop = graphTop + static_cast<int>(visibleIndex) * laneHeight;
					auto const laneBottom = visibleIndex + 1U == visibleCount ? graphBottom : std::min(graphBottom, laneTop + laneHeight);
					if (y == laneTop) {
						for (int x = 0; x < width; ++x) {
							line[x] = dim;
						}
					}

					auto const labelY = laneTop + std::max(0, (laneBottom - laneTop - 7) / 2);
					drawTextOnLine(line, width, y, 2, labelY, 1, channel.color,
					    channel.descriptor != nullptr && channel.descriptor->label != nullptr ? channel.descriptor->label : "?");

					if (graphCache.width > 0) {
						auto const highY = laneTop + 3;
						auto const lowY = laneBottom - 4;
						for (int x = 0; x < graphWidth; ++x) {
							auto const cacheX = static_cast<std::size_t>(x);
							if (cacheX >= graphCache.width || !graphCache.valid[cacheX]) {
								continue;
							}
							auto const value = graphCache.value(channelIndex, cacheX);
							auto const sampleY = value ? highY : lowY;
							if (y == sampleY) {
								line[leftMargin + x] = channel.color;
							}
							if (cacheX > 0 && graphCache.valid[cacheX - 1U]) {
								auto const previousValue = graphCache.value(channelIndex, cacheX - 1U);
								if (previousValue != value) {
									auto const previousY = previousValue ? highY : lowY;
									auto const minY = std::min(previousY, sampleY);
									auto const maxY = std::max(previousY, sampleY);
									if (y >= minY && y <= maxY) {
										line[leftMargin + x] = channel.color;
									}
								}
							}
						}
					}
					++visibleIndex;
				}
			}

			if (!display.blitRgb565({.x = 0, .y = y, .width = width, .height = 1}, line, width)) {
				return false;
			}
		}

		return display.endFrame();
	}

	void writeConfiguredPins(lexxos::os::AppContext& context, InputChannel const* inputs, std::size_t inputCount,
	    PullSelection const& pullSelection, OutputChannel const* outputs, std::size_t outputCount) {
		context.write("gpio-analyzer: inputs=");
		for (std::size_t index = 0; index < inputCount; ++index) {
			if (index != 0) {
				context.write(",");
			}

			auto const* descriptor = inputs[index].descriptor;
			context.write(descriptor != nullptr && descriptor->label != nullptr ? descriptor->label : "?");
			if (descriptor != nullptr) {
				context.write("(pull=");
				context.write(lexxos::platform::toString(pullSelection.forPin(*descriptor)));
				context.write(")");
			}
		}
		context.write(" outputs=");
		if (outputCount == 0) {
			context.write("none");
		} else {
			for (std::size_t index = 0; index < outputCount; ++index) {
				if (index != 0) {
					context.write(",");
				}
				context.write(outputs[index].descriptor != nullptr && outputs[index].descriptor->label != nullptr ? outputs[index].descriptor->label : "?");
				context.write(outputs[index].value ? "=1" : "=0");
			}
		}
		context.writeLine("");
	}

	[[nodiscard]] bool createDirectoryIfNeeded(std::string_view path) {
		auto const status = lexxos::vfs::createDirectory(path);
		return status == lexxos::vfs::WriteStatus::Ok || status == lexxos::vfs::WriteStatus::IsDirectory;
	}

	[[nodiscard]] bool ensureRecordingDirectory(lexxos::os::AppContext& context) {
		if (!createDirectoryIfNeeded("/mnt/sd/apps") || !createDirectoryIfNeeded(gpioAnalyzerRecordingDirectory)) {
			context.writeError("gpio-analyzer: failed to create recording directory: ");
			context.writeErrorLine(gpioAnalyzerRecordingDirectory);
			return false;
		}
		return true;
	}

	[[nodiscard]] bool buildRecordingPath(RecordingBuffer const& recording, FixedTextBuffer<lexxos::vfs::maxPathLength>& path) {
		path.clear();
		path.append(gpioAnalyzerRecordingDirectory);
		path.append("/gpio_");
		if (recording.start.realtime) {
			appendDateTime(path, recording.start.dateTime, true);
			path.append("_u");
			path.appendUnsigned(recording.start.uptimeMs);
		} else {
			path.append("uptime_");
			path.appendUnsigned(recording.start.uptimeMs);
			path.append("ms");
		}
		path.append(".txt");
		return !path.full();
	}

	[[nodiscard]] bool writeFileChunk(
	    lexxos::os::AppContext& context,
	    std::string_view path,
	    lexxos::vfs::FileHandle& file,
	    std::string_view text) {
		auto const status = file.write(text);
		if (status == lexxos::vfs::WriteStatus::Ok) {
			return true;
		}
		context.writeError("gpio-analyzer: ");
		context.writeError(path);
		context.writeError(": ");
		context.writeErrorLine(lexxos::vfs::toString(status));
		return false;
	}

	[[nodiscard]] bool writeRecordingHeader(
	    lexxos::os::AppContext& context,
	    std::string_view path,
	    lexxos::vfs::FileHandle& file,
	    RecordingBuffer const& recording,
	    InputChannel const* inputs,
	    std::size_t inputCount,
	    std::uint32_t sampleMs) {
		FixedTextBuffer<256> line;

		if (!writeFileChunk(context, path, file, "# LexxOS gpio-analyzer capture\n")) {
			return false;
		}

		line.clear();
		line.append("# start: ");
		if (recording.start.realtime) {
			appendDateTime(line, recording.start.dateTime, false);
			line.append(" rtc");
		} else {
			line.append("uptime ");
			line.appendUnsigned(recording.start.uptimeMs);
			line.append(" ms");
		}
		line.append("\n");
		if (!writeFileChunk(context, path, file, line.view())) {
			return false;
		}

		line.clear();
		line.append("# sample_period_ms: ");
		line.appendUnsigned(sampleMs);
		line.append("\n# samples: ");
		line.appendUnsigned(static_cast<std::uint32_t>(recording.count));
		line.append("\n# overflow: ");
		line.append(recording.overflowed ? "yes" : "no");
		line.append("\n# storage: ");
		line.append(recording.psram ? "psram" : "internal");
		line.append("\n");
		if (!writeFileChunk(context, path, file, line.view())) {
			return false;
		}

		line.clear();
		line.append("offset_ms,uptime_ms,mask");
		for (std::size_t index = 0; index < inputCount; ++index) {
			line.append(",");
			line.append(inputs[index].descriptor != nullptr && inputs[index].descriptor->label != nullptr ? inputs[index].descriptor->label : "?");
		}
		line.append("\n");
		return writeFileChunk(context, path, file, line.view());
	}

	[[nodiscard]] bool saveRecording(
	    lexxos::os::AppContext& context,
	    RecordingBuffer const& recording,
	    InputChannel const* inputs,
	    std::size_t inputCount,
	    std::uint32_t sampleMs) {
		if (!ensureRecordingDirectory(context)) {
			return false;
		}

		FixedTextBuffer<lexxos::vfs::maxPathLength> path;
		if (!buildRecordingPath(recording, path)) {
			context.writeErrorLine("gpio-analyzer: recording path is too long");
			return false;
		}

		lexxos::vfs::FileHandle file;
		auto const openStatus = lexxos::vfs::open(path.c_str(), lexxos::vfs::OpenMode::CreateTruncateWrite, file);
		if (openStatus != lexxos::vfs::OpenStatus::Ok) {
			context.writeError("gpio-analyzer: ");
			context.writeError(path.view());
			context.writeError(": ");
			context.writeErrorLine(lexxos::vfs::toString(openStatus));
			return false;
		}

		if (!writeRecordingHeader(context, path.view(), file, recording, inputs, inputCount, sampleMs)) {
			return false;
		}

		FixedTextBuffer<320> line;
		for (std::size_t sampleIndex = 0; sampleIndex < recording.count; ++sampleIndex) {
			RecordingSample sample{};
			if (!recording.loadSample(sampleIndex, sample)) {
				context.writeErrorLine("gpio-analyzer: failed to read recorded sample");
				return false;
			}

			line.clear();
			line.appendUnsigned(sample.offsetMs);
			line.append(",");
			line.appendUnsigned(sample.uptimeMs);
			line.append(",0x");
			line.appendHex4(sample.mask);
			for (std::size_t channelIndex = 0; channelIndex < inputCount; ++channelIndex) {
				line.append(",");
				line.append((sample.mask & static_cast<std::uint16_t>(1U << channelIndex)) != 0 ? "1" : "0");
			}
			line.append("\n");
			if (!writeFileChunk(context, path.view(), file, line.view())) {
				return false;
			}
		}

		file.close();
		(void) lexxos::vfs::sync();
		context.writeLine("gpio-analyzer: recording saved");
		context.writeLine(path.view());
		return true;
	}

	[[nodiscard]] char controlCharForInputEvent(lexxos::devices::InputEvent const& event) {
		if (event.type != lexxos::devices::InputEventType::KeyDown) {
			return controlNone;
		}
		if (event.character != 0) {
			return static_cast<char>(event.character);
		}

		switch (event.key) {
			case lexxos::devices::KeyCode::Escape:
				return controlEscape;
			case lexxos::devices::KeyCode::Break:
				return controlCancel;
			case lexxos::devices::KeyCode::Space:
				return controlPause;
			case lexxos::devices::KeyCode::Q:
				return controlExit;
			case lexxos::devices::KeyCode::N:
				return 'n';
			case lexxos::devices::KeyCode::R:
				return controlToggleRecording;
			case lexxos::devices::KeyCode::T:
				return controlToggleOutput;
			case lexxos::devices::KeyCode::Minus:
				return '-';
			case lexxos::devices::KeyCode::Equals:
				return '=';
			case lexxos::devices::KeyCode::LeftBracket:
				return controlWindowSmaller;
			case lexxos::devices::KeyCode::RightBracket:
				return controlWindowLarger;
			case lexxos::devices::KeyCode::Num1:
				return '1';
			case lexxos::devices::KeyCode::Num2:
				return '2';
			case lexxos::devices::KeyCode::Num3:
				return '3';
			case lexxos::devices::KeyCode::Num4:
				return '4';
			case lexxos::devices::KeyCode::Num5:
				return '5';
			case lexxos::devices::KeyCode::Num6:
				return '6';
			case lexxos::devices::KeyCode::Num7:
				return '7';
			case lexxos::devices::KeyCode::Num8:
				return '8';
			case lexxos::devices::KeyCode::Num9:
				return '9';
			case lexxos::devices::KeyCode::A:
				return 'a';
			case lexxos::devices::KeyCode::B:
				return 'b';
			case lexxos::devices::KeyCode::C:
				return 'c';
			case lexxos::devices::KeyCode::D:
				return 'd';
			case lexxos::devices::KeyCode::E:
				return 'e';
			case lexxos::devices::KeyCode::F:
				return 'f';
			default:
				return controlNone;
		}
	}

	enum class ControlDispatchResult : std::uint8_t {
		Continue,
		Exit,
		Cancel,
		ToggleRecording,
	};

	[[nodiscard]] ControlDispatchResult dispatchControlChar(
	    char key,
	    InputChannel* inputs,
	    std::size_t inputCount,
	    OutputChannel* outputs,
	    std::size_t outputCount,
	    std::size_t& selectedOutput,
	    std::uint32_t& sampleMs,
	    std::uint32_t& windowMs,
	    bool& paused,
	    bool& redraw) {
		if (key == controlNone) {
			return ControlDispatchResult::Continue;
		}
		if (key == controlExit || key == 'Q' || key == controlEscape) {
			return ControlDispatchResult::Exit;
		}
		if (key == controlCancel) {
			return ControlDispatchResult::Cancel;
		}
		if (key == controlToggleRecording || key == 'R') {
			return ControlDispatchResult::ToggleRecording;
		}
		if (key == controlPause) {
			paused = !paused;
			redraw = true;
		} else if (key == '+' || key == '=') {
			sampleMs = std::max(minimumSamplePeriodMs, sampleMs / 2U);
			redraw = true;
		} else if (key == '-' || key == '_') {
			sampleMs = std::min(maximumSamplePeriodMs, sampleMs * 2U);
			redraw = true;
		} else if (key == controlWindowSmaller) {
			windowMs = std::max(minimumWindowMs, windowMs / 2U);
			redraw = true;
		} else if (key == controlWindowLarger) {
			windowMs = std::min(maximumWindowMs, windowMs * 2U);
			redraw = true;
		} else if ((key >= '1' && key <= '9') || (key >= 'a' && key <= 'f') || (key >= 'A' && key <= 'F')) {
			std::size_t index = 0;
			if (key >= '1' && key <= '9') {
				index = static_cast<std::size_t>(key - '1');
			} else {
				index = 9U + static_cast<std::size_t>(asciiLower(key) - 'a');
			}
			if (index < inputCount) {
				inputs[index].visible = !inputs[index].visible;
				redraw = true;
			}
		} else if ((key == 'n' || key == 'N') && outputCount > 0) {
			selectedOutput = (selectedOutput + 1U) % outputCount;
			redraw = true;
		} else if ((key == controlToggleOutput || key == 'T') && outputCount > 0) {
			auto& output = outputs[selectedOutput % outputCount];
			output.value = !output.value;
			if (output.descriptor != nullptr) {
				(void) lexxos::platform::writeGpio(output.descriptor->pin, output.value);
			}
			redraw = true;
		}
		return ControlDispatchResult::Continue;
	}

} // namespace

lexxos::program::ProgramDescriptor GpioAnalyzerProgram::descriptor() {
	using lexxos::program::ProgramInterfaceKind;
	using lexxos::program::ProgramLaunchMode;
	using lexxos::program::ProgramMemoryClass;

	return {
	    .name = "gpio-analyzer",
	    .help = "show and control user GPIO pins",
	    .description = "Configures board-approved digital GPIO pins as inputs and outputs, then shows selected inputs as a live logic graph on the display.",
	    .usage = "gpio-analyzer [-i pins] [-o pin=value,...] [-s milliseconds] [-w milliseconds] [-p board|none|up|down] [display]",
	    .arguments = argumentList(gpioAnalyzerArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 6144,
	                .priority = 2,
	                .memoryClass = ProgramMemoryClass::ExternalPreferred,
	                .externalMemoryQuotaBytes = static_cast<std::uint32_t>(psramSampleCapacity * (sizeof(std::uint16_t) + psramRecordingSampleBytes)),
	                .cancellable = true,
	                .allowBackground = false,
	                .interfaceKind = ProgramInterfaceKind::Graphical,
	                },
	    .legacy = false,
	    .enabled = true,
	};
}

int GpioAnalyzerProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto const& board = activeBoard(context);
	if (board.userGpio.count == 0) {
		context.writeErrorLine("gpio-analyzer: board does not expose user GPIO pins");
		return 1;
	}

	std::uint32_t sampleMs = 20;
	std::uint32_t windowMs = 5000;
	if (!parseUnsigned32(arguments.valueOr("sample", "20"), sampleMs) || sampleMs < minimumSamplePeriodMs || sampleMs > maximumSamplePeriodMs) {
		context.writeErrorLine("gpio-analyzer: invalid sample period");
		return 1;
	}
	if (!parseUnsigned32(arguments.valueOr("window", "5000"), windowMs) || windowMs < minimumWindowMs || windowMs > maximumWindowMs) {
		context.writeErrorLine("gpio-analyzer: invalid time window");
		return 1;
	}

	PullSelection pullSelection;
	if (!parsePullSelection(arguments.valueOr("pull", "board"), pullSelection)) {
		context.writeErrorLine("gpio-analyzer: invalid pull mode");
		return 1;
	}

	InputChannel inputs[maxChannels]{};
	OutputChannel outputs[maxChannels]{};
	std::size_t inputCount = 0;
	std::size_t outputCount = 0;
	if (!parseInputChannels(context, board, arguments.valueOr("in", ""), inputs, inputCount)) {
		context.writeErrorLine("gpio-analyzer: no input channels selected");
		return 1;
	}
	if (!parseOutputChannels(context, board, arguments.valueOr("out", ""), outputs, outputCount)) {
		return 1;
	}
	if (inputOutputOverlap(inputs, inputCount, outputs, outputCount)) {
		context.writeErrorLine("gpio-analyzer: the same pin cannot be input and output in one session");
		return 1;
	}

	for (std::size_t index = 0; index < outputCount; ++index) {
		if (outputs[index].descriptor == nullptr || !lexxos::platform::configureGpioOutput(outputs[index].descriptor->pin, outputs[index].value)) {
			context.writeErrorLine("gpio-analyzer: failed to configure output");
			return 1;
		}
	}
	for (std::size_t index = 0; index < inputCount; ++index) {
		auto const* descriptor = inputs[index].descriptor;
		auto const pull = descriptor != nullptr ? pullSelection.forPin(*descriptor) : lexxos::platform::GpioPull::None;
		if (descriptor == nullptr || !lexxos::platform::configureGpioInput(descriptor->pin, pull)) {
			context.writeErrorLine("gpio-analyzer: failed to configure input");
			return 1;
		}
	}

	auto* display = detail::selectedDisplay(context, arguments.positional("display"));
	if (display == nullptr || !display->isReady()) {
		context.writeErrorLine(display == nullptr
		        ? "gpio-analyzer: display device is not registered"
		        : "gpio-analyzer: display device is not ready");
		return 1;
	}

	writeConfiguredPins(context, inputs, inputCount, pullSelection, outputs, outputCount);
	context.writeLine("gpio-analyzer: controls: space pause, +/- sample, [] window, r record, 1-9/a-f toggle inputs, t toggle output, n next output, q exit");

	lexxos::app::program_input::EventCursor inputCursor;
	lexxos::app::program_input::reset(inputCursor);
	context.writeLine("gpio-analyzer: input=program-input event queue");

	static SampleBuffer samples;
	if (!samples.initialize(context, board)) {
		return 1;
	}

	static RecordingBuffer recording;
	if (!recording.initialize(context, board)) {
		return 1;
	}

	std::size_t selectedOutput = 0;
	bool paused = false;
	bool redraw = true;
	auto nextSampleMs = uptimeMs(context);
	lexxos::devices::DeviceResourceLock displayLock;

	{
		auto graphicsMode = detail::enterGraphicsMode();
		(void) graphicsMode;

		while (true) {
			if (context.pollCancellationRequest()) {
				return 130;
			}

			char key = 0;

			lexxos::devices::InputEvent inputEvent;
			while (lexxos::app::program_input::pollEvent(inputCursor, inputEvent)) {
				auto const result = dispatchControlChar(
				    controlCharForInputEvent(inputEvent), inputs, inputCount, outputs, outputCount, selectedOutput, sampleMs, windowMs, paused, redraw);

				if (result == ControlDispatchResult::Exit) {
					return 0;
				}

				if (result == ControlDispatchResult::Cancel) {
					return 130;
				}

				if (result == ControlDispatchResult::ToggleRecording) {
					auto const controlNowMs = uptimeMs(context);
					if (recording.active) {
						recording.stopCapture();
						redraw = true;
						if (!saveRecording(context, recording, inputs, inputCount, sampleMs)) {
							return 1;
						}
					} else if (recording.startCapture(context, controlNowMs)) {
						context.writeLine("gpio-analyzer: recording started");
						redraw = true;
					}
				}
			}

			auto const nowMs = uptimeMs(context);
			if (!paused && nowMs >= nextSampleMs) {
				std::uint16_t sampleMask = 0;
				if (!samples.push(inputs, inputCount, &sampleMask)) {
					context.writeErrorLine("gpio-analyzer: failed to store sample");
					return 1;
				}
				if (recording.active && !recording.push(nowMs, sampleMask)) {
					context.writeErrorLine("gpio-analyzer: recording buffer full, saving capture");
					if (!saveRecording(context, recording, inputs, inputCount, sampleMs)) {
						return 1;
					}
				}
				nextSampleMs = nowMs + sampleMs;
				redraw = true;
			}

			auto const focused = context.hasProgramFocus();
			if (!focused) {
				displayLock.release();
			} else if (!displayLock) {
				displayLock = detail::lockDisplay(context, *display, "gpio-analyzer");
				if (!displayLock) {
					// Another focused owner is still redrawing the display.  Keep
					// sampling GPIOs and retry on the next iteration instead of
					// terminating the analyzer session.
					lexxos::kernel::sleepMs(5);
					continue;
				}
				redraw = true;
			}

			if (focused && redraw) {
				if (!renderFrame(*display, inputs, inputCount, outputs, outputCount, selectedOutput, samples, sampleMs, windowMs, paused, recording)) {
					context.writeErrorLine("gpio-analyzer: failed to render frame");
					return 1;
				}
				redraw = false;
			}

			lexxos::kernel::sleepMs(5);
		}
	}
}

} // namespace lexxos::apps::device_programs
