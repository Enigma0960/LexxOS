#include <lexxos/apps/core/esp32_info_program.h>

#include <cstdint>

#include <esp_chip_info.h>
#include <esp_err.h>
#include <esp_flash.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/os/app_context.h>

namespace {

std::string_view chipModelName(esp_chip_model_t model) {
	switch (model) {
		case CHIP_ESP32:
			return "esp32";
		case CHIP_ESP32S2:
			return "esp32s2";
		case CHIP_ESP32S3:
			return "esp32s3";
		case CHIP_ESP32C2:
			return "esp32c2";
		case CHIP_ESP32C3:
			return "esp32c3";
		case CHIP_ESP32C5:
			return "esp32c5";
		case CHIP_ESP32C6:
			return "esp32c6";
		case CHIP_ESP32C61:
			return "esp32c61";
		case CHIP_ESP32H2:
			return "esp32h2";
		case CHIP_ESP32H21:
			return "esp32h21";
		case CHIP_ESP32H4:
			return "esp32h4";
		case CHIP_ESP32P4:
			return "esp32p4";
		case CHIP_POSIX_LINUX:
			return "posix-linux";
		default:
			return "unknown";
	}
}

std::string_view resetReasonName(esp_reset_reason_t reason) {
	switch (reason) {
		case ESP_RST_UNKNOWN:
			return "unknown";
		case ESP_RST_POWERON:
			return "poweron";
		case ESP_RST_EXT:
			return "external";
		case ESP_RST_SW:
			return "software";
		case ESP_RST_PANIC:
			return "panic";
		case ESP_RST_INT_WDT:
			return "interrupt-watchdog";
		case ESP_RST_TASK_WDT:
			return "task-watchdog";
		case ESP_RST_WDT:
			return "watchdog";
		case ESP_RST_DEEPSLEEP:
			return "deepsleep";
		case ESP_RST_BROWNOUT:
			return "brownout";
		case ESP_RST_SDIO:
			return "sdio";
		default:
			return "unknown";
	}
}

void writeBytesAsKiB(lexxos::os::AppContext& context, std::uint32_t bytes) {
	lexxos::apps::writeUnsigned(context, bytes / 1024U);
	context.writeLine(" KiB");
}

void writeChipFeatures(lexxos::os::AppContext& context, std::uint32_t features) {
	context.write("features: ");
	lexxos::apps::writeHex32(context, features);
	context.write(" [");

	bool first = true;
	auto writeFeature = [&](char const* name) {
		context.write(first ? "" : ",");
		context.write(name);
		first = false;
	};

	if ((features & CHIP_FEATURE_EMB_FLASH) != 0) {
		writeFeature("embedded-flash");
	}
	if ((features & CHIP_FEATURE_WIFI_BGN) != 0) {
		writeFeature("wifi");
	}
	if ((features & CHIP_FEATURE_BT) != 0) {
		writeFeature("bt");
	}
	if ((features & CHIP_FEATURE_BLE) != 0) {
		writeFeature("ble");
	}
	if ((features & CHIP_FEATURE_IEEE802154) != 0) {
		writeFeature("ieee802154");
	}
	if ((features & CHIP_FEATURE_EMB_PSRAM) != 0) {
		writeFeature("embedded-psram");
	}
	if (first) {
		context.write("none");
	}
	context.writeLine("]");
}

void writeHeapLine(lexxos::os::AppContext& context, char const* label, std::uint32_t caps) {
	context.write(label);
	writeBytesAsKiB(context, static_cast<std::uint32_t>(heap_caps_get_free_size(caps)));
}

void writeLargestHeapBlockLine(lexxos::os::AppContext& context, char const* label, std::uint32_t caps) {
	context.write(label);
	writeBytesAsKiB(context, static_cast<std::uint32_t>(heap_caps_get_largest_free_block(caps)));
}

} // namespace

namespace lexxos::apps::system_programs {

lexxos::program::ProgramDescriptor Esp32InfoProgram::descriptor() {
	return {
	    .name = "esp32-info",
	    .help = "show ESP32 diagnostic information",
	    .description = "Shows ESP-IDF version, chip, flash, reset and heap diagnostic information.",
	    .usage = "esp32-info",
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int Esp32InfoProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	esp_chip_info_t chip{};
	esp_chip_info(&chip);

	context.write("idf-version: ");
	context.writeLine(esp_get_idf_version());

	context.write("chip: ");
	context.write(chipModelName(chip.model));
	context.write(" revision=");
	lexxos::apps::writeUnsigned(context, static_cast<std::uint32_t>(chip.revision));
	context.write(" cores=");
	lexxos::apps::writeUnsigned(context, static_cast<std::uint32_t>(chip.cores));
	context.writeLine("");
	writeChipFeatures(context, chip.features);

	std::uint32_t flashSize = 0;
	if (esp_flash_get_size(nullptr, &flashSize) == ESP_OK) {
		context.write("flash-size: ");
		writeBytesAsKiB(context, flashSize);
	} else {
		context.writeLine("flash-size: unknown");
	}

	auto const reason = esp_reset_reason();
	context.write("reset-reason: ");
	context.write(resetReasonName(reason));
	context.write(" (");
	lexxos::apps::writeUnsigned(context, static_cast<std::uint32_t>(reason));
	context.writeLine(")");

	context.write("heap-free: ");
	writeBytesAsKiB(context, static_cast<std::uint32_t>(heap_caps_get_free_size(MALLOC_CAP_8BIT)));
	context.write("heap-min-ever-free: ");
	writeBytesAsKiB(context, static_cast<std::uint32_t>(heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT)));
	context.write("heap-largest-free-block: ");
	writeBytesAsKiB(context, static_cast<std::uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
	writeHeapLine(context, "heap-internal-free: ", MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
	writeLargestHeapBlockLine(context, "heap-internal-largest-free-block: ", MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
	writeHeapLine(context, "heap-psram-free: ", MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	writeLargestHeapBlockLine(context, "heap-psram-largest-free-block: ", MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

	return 0;
}

} // namespace lexxos::apps::system_programs
