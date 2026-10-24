#include <lexxos/platform/platform.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_partition.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <lexxos/platform/espidf/detail/espidf_console.h>
#include <lexxos/platform/espidf/detail/espidf_gpio.h>

#include <lexxos/devices/espidf/espidf_ethernet.h>

namespace {

TickType_t millisecondsToTicks(std::uint32_t milliseconds) {
	auto const ticks = pdMS_TO_TICKS(milliseconds);
	if (ticks == 0) {
		return 1;
	}

	return ticks;
}

constexpr int cardputerBatteryAdcGpio = 10;
constexpr int cardputerBatterySamples = 8;
constexpr float cardputerBatteryAdcRatio = 2.0F;
constexpr int cardputerBatteryEmptyMv = 3300;
constexpr int cardputerBatteryFullMv = 4150;
constexpr adc_atten_t cardputerBatteryAdcAtten = ADC_ATTEN_DB_12;
constexpr adc_bitwidth_t cardputerBatteryAdcBitwidth = ADC_BITWIDTH_12;
constexpr char gameBoyRomCachePartitionLabel[] = "storage";
constexpr std::uint32_t gameBoyRomCacheEraseBytes = 4096;
constexpr std::uint32_t gameBoyRomCacheProgramBytes = 4;

struct BatteryAdcRuntime {
	bool initialized{false};
	adc_unit_t unit{ADC_UNIT_1};
	adc_channel_t channel{ADC_CHANNEL_0};
	adc_oneshot_unit_handle_t handle{nullptr};
	adc_cali_handle_t calibration{nullptr};
	bool calibrated{false};
};

BatteryAdcRuntime g_batteryAdc{};

struct GameBoyRomCachePartitionRuntime {
	esp_partition_t const* partition{nullptr};
	esp_partition_mmap_handle_t mmapHandle{0};
	std::uint8_t const* mappedBase{nullptr};
};

GameBoyRomCachePartitionRuntime g_gameBoyRomCachePartition{};

bool isCardputerBatteryAdcBoard() {
	auto const boardId = lexxos::platform::activeBoardDescriptor().boardId;
	return std::strcmp(boardId, "m5stack_cardputer") == 0
	    || std::strcmp(boardId, "m5stack_cardputer_adv") == 0;
}

std::uint8_t estimateBatteryPercent(std::uint16_t voltageMv) {
	if (voltageMv <= cardputerBatteryEmptyMv) {
		return 0;
	}
	if (voltageMv >= cardputerBatteryFullMv) {
		return 100;
	}

	auto const percent =
	    (static_cast<int>(voltageMv) - cardputerBatteryEmptyMv) * 100 / (cardputerBatteryFullMv - cardputerBatteryEmptyMv);
	return static_cast<std::uint8_t>(std::clamp(percent, 0, 100));
}

bool initializeBatteryAdc() {
	if (g_batteryAdc.initialized) {
		return g_batteryAdc.handle != nullptr;
	}
	g_batteryAdc.initialized = true;

	adc_unit_t unit{};
	adc_channel_t channel{};
	if (adc_oneshot_io_to_channel(cardputerBatteryAdcGpio, &unit, &channel) != ESP_OK) {
		return false;
	}

	adc_oneshot_unit_init_cfg_t initConfig{};
	initConfig.unit_id = unit;
	initConfig.ulp_mode = ADC_ULP_MODE_DISABLE;
	if (adc_oneshot_new_unit(&initConfig, &g_batteryAdc.handle) != ESP_OK) {
		g_batteryAdc.handle = nullptr;
		return false;
	}

	adc_oneshot_chan_cfg_t channelConfig{};
	channelConfig.atten = cardputerBatteryAdcAtten;
	channelConfig.bitwidth = cardputerBatteryAdcBitwidth;
	if (adc_oneshot_config_channel(g_batteryAdc.handle, channel, &channelConfig) != ESP_OK) {
		g_batteryAdc.handle = nullptr;
		return false;
	}

	g_batteryAdc.unit = unit;
	g_batteryAdc.channel = channel;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
	adc_cali_curve_fitting_config_t calibrationConfig{};
	calibrationConfig.unit_id = unit;
	calibrationConfig.chan = channel;
	calibrationConfig.atten = cardputerBatteryAdcAtten;
	calibrationConfig.bitwidth = cardputerBatteryAdcBitwidth;
	g_batteryAdc.calibrated =
	    adc_cali_create_scheme_curve_fitting(&calibrationConfig, &g_batteryAdc.calibration) == ESP_OK;
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
	adc_cali_line_fitting_config_t calibrationConfig{};
	calibrationConfig.unit_id = unit;
	calibrationConfig.atten = cardputerBatteryAdcAtten;
	calibrationConfig.bitwidth = cardputerBatteryAdcBitwidth;
	g_batteryAdc.calibrated =
	    adc_cali_create_scheme_line_fitting(&calibrationConfig, &g_batteryAdc.calibration) == ESP_OK;
#endif

	return true;
}

bool readCardputerBatteryVoltage(std::uint16_t& voltageMv) {
	if (!isCardputerBatteryAdcBoard() || !initializeBatteryAdc()) {
		return false;
	}

	int sumMv = 0;
	int validSamples = 0;
	for (int sample = 0; sample < cardputerBatterySamples; ++sample) {
		int padMv = 0;
		esp_err_t result = ESP_FAIL;
		if (g_batteryAdc.calibrated && g_batteryAdc.calibration != nullptr) {
			result = adc_oneshot_get_calibrated_result(
			    g_batteryAdc.handle,
			    g_batteryAdc.calibration,
			    g_batteryAdc.channel,
			    &padMv);
		} else {
			int raw = 0;
			result = adc_oneshot_read(g_batteryAdc.handle, g_batteryAdc.channel, &raw);
			if (result == ESP_OK) {
				padMv = raw * 2450 / 4095;
			}
		}

		if (result == ESP_OK) {
			sumMv += static_cast<int>(std::lround(static_cast<float>(padMv) * cardputerBatteryAdcRatio));
			++validSamples;
		}
	}

	if (validSamples == 0) {
		return false;
	}

	auto const averagedMv = sumMv / validSamples;
	voltageMv = static_cast<std::uint16_t>(std::clamp(averagedMv, 0, 65535));
	return true;
}

esp_partition_t const* gameBoyRomCachePartition() {
	return esp_partition_find_first(
	    ESP_PARTITION_TYPE_DATA,
	    ESP_PARTITION_SUBTYPE_DATA_FAT,
	    gameBoyRomCachePartitionLabel);
}

bool ensureGameBoyRomCacheMapping(esp_partition_t const* partition, std::uint8_t const*& mappedBase) {
	mappedBase = nullptr;
	if (partition == nullptr || partition->size == 0) {
		return false;
	}

	if (g_gameBoyRomCachePartition.partition == partition && g_gameBoyRomCachePartition.mappedBase != nullptr) {
		mappedBase = g_gameBoyRomCachePartition.mappedBase;
		return true;
	}

	if (g_gameBoyRomCachePartition.mappedBase != nullptr) {
		esp_partition_munmap(g_gameBoyRomCachePartition.mmapHandle);
		g_gameBoyRomCachePartition = {};
	}

	void const* mapped = nullptr;
	esp_partition_mmap_handle_t handle = 0;
	if (esp_partition_mmap(partition, 0, partition->size, ESP_PARTITION_MMAP_DATA, &mapped, &handle) != ESP_OK || mapped == nullptr) {
		return false;
	}

	g_gameBoyRomCachePartition.partition = partition;
	g_gameBoyRomCachePartition.mmapHandle = handle;
	g_gameBoyRomCachePartition.mappedBase = static_cast<std::uint8_t const*>(mapped);
	mappedBase = g_gameBoyRomCachePartition.mappedBase;
	return true;
}

} // namespace

void lexxos::platform::initialize() {
	espidf::detail::initializeConsole();
	espidf::detail::initializeStatusLed();
	auto const& activeBoard = activeBoardDescriptor();
	if (activeBoard.ethernet.isValid()) {
		(void) lexxos::devices::espidf_ethernet::start(activeBoard);
	}
}

lexxos::platform::BoardDescriptor const& lexxos::platform::board() {
	return activeBoardDescriptor();
}

void lexxos::platform::sleepMs(std::uint32_t milliseconds) {
	if (milliseconds == 0) {
		taskYIELD();
		return;
	}

	vTaskDelay(millisecondsToTicks(milliseconds));
}

std::uint32_t lexxos::platform::uptimeMs() {
	return static_cast<std::uint32_t>(esp_timer_get_time() / 1000);
}

lexxos::platform::PlatformPerformanceSnapshot lexxos::platform::performanceSnapshot() {
	return {};
}

bool lexxos::platform::readBatteryStatus(PlatformBatteryStatus& output) {
	output = {};

	std::uint16_t voltageMv = 0;
	if (!readCardputerBatteryVoltage(voltageMv)) {
		return false;
	}

	output.available = true;
	output.voltageMvValid = true;
	output.voltageMv = voltageMv;
	output.percentValid = true;
	output.percent = estimateBatteryPercent(voltageMv);
	output.chargingValid = false;
	output.source = "cardputer-adc-gpio10";
	return true;
}

void lexxos::platform::reboot() {
	esp_restart();
}

bool lexxos::platform::gameBoyRomFlashRegion(GameBoyRomFlashRegion& output) {
	output = GameBoyRomFlashRegion{};
	auto const* partition = gameBoyRomCachePartition();
	std::uint8_t const* mappedBase = nullptr;
	if (!ensureGameBoyRomCacheMapping(partition, mappedBase)) {
		return false;
	}

	output.available = true;
	output.offset = partition->address;
	output.capacity = partition->size;
	output.eraseSize = gameBoyRomCacheEraseBytes;
	output.programSize = gameBoyRomCacheProgramBytes;
	output.xipBase = mappedBase;
	return true;
}

bool lexxos::platform::eraseAndProgramGameBoyRomFlash(std::uint32_t offset, void const* data, std::size_t size) {
	GameBoyRomFlashRegion region{};
	auto const* partition = gameBoyRomCachePartition();
	if (partition == nullptr || !gameBoyRomFlashRegion(region) || data == nullptr || size == 0 || size > UINT32_MAX) {
		return false;
	}

	auto const writeSize = static_cast<std::uint32_t>(size);
	if (writeSize > region.capacity
	    || offset < region.offset
	    || offset - region.offset > region.capacity - writeSize
	    || (offset % region.eraseSize) != 0U
	    || (writeSize % region.eraseSize) != 0U) {
		return false;
	}

	auto const relativeOffset = offset - region.offset;
	return esp_partition_erase_range(partition, relativeOffset, writeSize) == ESP_OK
	    && esp_partition_write(partition, relativeOffset, data, size) == ESP_OK;
}
