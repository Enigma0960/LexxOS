#include <lexxos/devices/espidf/mipi_dsi_display_espidf.h>

#include <algorithm>
#include <cinttypes>
#include <cstring>

#include <esp_check.h>
#include <esp_lcd_mipi_dsi.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_ldo_regulator.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <hal/mipi_dsi_hal.h>
#include <hal/mipi_dsi_host_ll.h>
#include <hal/mipi_dsi_ll.h>
#include <hal/mipi_dsi_types.h>

namespace {

constexpr char const* tag = "lexxos.dsi";

constexpr std::uint8_t rpiRegPortA = 0x81;
constexpr std::uint8_t rpiRegPortB = 0x82;
constexpr std::uint8_t rpiRegPortC = 0x83;
constexpr std::uint8_t rpiRegAddrL = 0x8C;
constexpr std::uint8_t rpiRegAddrH = 0x8D;
constexpr std::uint8_t rpiRegWriteDataH = 0x90;
constexpr std::uint8_t rpiRegWriteDataL = 0x91;

constexpr std::uint8_t paLcdLr = 1U << 2U;
constexpr std::uint8_t pbLcdMain = 1U << 7U;
constexpr std::uint8_t pcLedEn = 1U << 0U;
constexpr std::uint8_t pcResetLcdN = 1U << 2U;
constexpr std::uint8_t pcResetBridgeN = 1U << 3U;

constexpr std::uint16_t tcDsiLaneEnable = 0x0210;
constexpr std::uint16_t tcPpiD0sClrSipoCount = 0x0164;
constexpr std::uint16_t tcPpiD1sClrSipoCount = 0x0168;
constexpr std::uint16_t tcPpiD0sAtmr = 0x0144;
constexpr std::uint16_t tcPpiD1sAtmr = 0x0148;
constexpr std::uint16_t tcPpiLpTxTimeCnt = 0x0114;
constexpr std::uint16_t tcSpiCmr = 0x0450;
constexpr std::uint16_t tcLcdCtrl = 0x0420;
constexpr std::uint16_t tcSysCtrl = 0x0464;
constexpr std::uint16_t tcLcdHsHbp = 0x0424;
constexpr std::uint16_t tcLcdHdispHfp = 0x0428;
constexpr std::uint16_t tcLcdVsVbp = 0x042C;
constexpr std::uint16_t tcLcdVdispVfp = 0x0430;
constexpr std::uint16_t tcPpiStartPpi = 0x0104;
constexpr std::uint16_t tcDsiStartDsi = 0x0204;

constexpr std::uint32_t bit(std::uint32_t value) {
	return 1UL << value;
}

std::uint8_t expand5To8(std::uint16_t value) {
	return static_cast<std::uint8_t>((value << 3U) | (value >> 2U));
}

std::uint8_t expand6To8(std::uint16_t value) {
	return static_cast<std::uint8_t>((value << 2U) | (value >> 4U));
}

std::uint16_t rgb565FromBigEndianBytes(std::uint8_t const* bytes) {
	return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[0]) << 8U) | bytes[1]);
}

lcd_color_format_t toLcdColorFormat(lexxos::platform::MipiDsiColorFormat format) {
	switch (format) {
		case lexxos::platform::MipiDsiColorFormat::Rgb888:
			return LCD_COLOR_FMT_RGB888;
		case lexxos::platform::MipiDsiColorFormat::Rgb565:
			return LCD_COLOR_FMT_RGB565;
	}
	return LCD_COLOR_FMT_RGB565;
}

lexxos::devices::DisplayWireFormat toWireFormat(lexxos::platform::MipiDsiColorFormat format) {
	switch (format) {
		case lexxos::platform::MipiDsiColorFormat::Rgb888:
			return lexxos::devices::DisplayWireFormat::Rgb888;
		case lexxos::platform::MipiDsiColorFormat::Rgb565:
			return lexxos::devices::DisplayWireFormat::Rgb565;
	}
	return lexxos::devices::DisplayWireFormat::Unknown;
}

int bytesPerPixel(lexxos::platform::MipiDsiColorFormat format) {
	return format == lexxos::platform::MipiDsiColorFormat::Rgb888 ? 3 : 2;
}

std::uint16_t rgb565ForElementOrder(std::uint16_t rgb565, lexxos::platform::MipiDsiRgbElementOrder order) {
	if (order == lexxos::platform::MipiDsiRgbElementOrder::Rgb) {
		return rgb565;
	}
	auto const red = static_cast<std::uint16_t>((rgb565 >> 11U) & 0x001FU);
	auto const green = static_cast<std::uint16_t>(rgb565 & 0x07E0U);
	auto const blue = static_cast<std::uint16_t>(rgb565 & 0x001FU);
	return static_cast<std::uint16_t>((blue << 11U) | green | red);
}

struct DsiBusPrivatePrefix {
	int busId;
	mipi_dsi_hal_context_t hal;
};

} // namespace

namespace lexxos::devices {

EspIdfMipiDsiDisplay::EspIdfMipiDsiDisplay(I2cBus& controlBus, lexxos::platform::MipiDsiDisplayConfig config)
    : _controlBus(controlBus)
    , _config(config) {
}

bool EspIdfMipiDsiDisplay::initialize() {
	_ready = false;
	_frameActive = false;
	_streamActive = false;
	_streamOffsetPixels = 0;
	_frameBuffer = nullptr;
	_powerMode = DisplayPowerMode::On;
	_backlight = 0;

	if (!_config.isValid()) {
		ESP_LOGE(tag, "invalid MIPI DSI display config");
		return false;
	}

	if (!_initializePowerControl()) {
		return false;
	}

	if (!_initializeDsi()) {
		return false;
	}

	_ready = true;
	if (!clear(0x0000)) {
		_ready = false;
		return false;
	}

	if (!setBacklight(_config.defaultBrightness)) {
		ESP_LOGW(tag, "MIPI DSI backlight command failed");
	}

	ESP_LOGI(tag, "display initialized: %dx%d lanes=%u lane=%" PRIu32 "Mbps dpi=%" PRIu32 "kHz",
	    _config.width,
	    _config.height,
	    _config.dataLanes,
	    _config.laneBitRateMbps,
	    _config.dpiClockKhz);
	return true;
}

std::string_view EspIdfMipiDsiDisplay::name() const {
	return _config.name != nullptr ? _config.name : "mipi-dsi";
}

bool EspIdfMipiDsiDisplay::isReady() const {
	return _ready;
}

int EspIdfMipiDsiDisplay::width() const {
	return _config.width;
}

int EspIdfMipiDsiDisplay::height() const {
	return _config.height;
}

PixelFormat EspIdfMipiDsiDisplay::nativeFormat() const {
	return PixelFormat::Rgb565;
}

DisplayWireFormat EspIdfMipiDsiDisplay::wireFormat() const {
	return toWireFormat(_config.outputColor);
}

int EspIdfMipiDsiDisplay::wireBytesPerPixel() const {
	return bytesPerPixel(_config.outputColor);
}

std::uint32_t EspIdfMipiDsiDisplay::busSpeedHz() const {
	return _config.dpiClockKhz * 1000U;
}

bool EspIdfMipiDsiDisplay::supportsPowerMode() const {
	return supportsBacklight();
}

DisplayPowerMode EspIdfMipiDsiDisplay::powerMode() const {
	return _powerMode;
}

bool EspIdfMipiDsiDisplay::setPowerMode(DisplayPowerMode mode) {
	if (!_ready || _frameActive || _streamActive) {
		return false;
	}
	if (mode == _powerMode) {
		return true;
	}

	switch (mode) {
		case DisplayPowerMode::On:
			if (_backlight == 0 && !setBacklight(_config.defaultBrightness)) {
				return false;
			}
			_powerMode = DisplayPowerMode::On;
			return true;
		case DisplayPowerMode::Off:
		case DisplayPowerMode::Sleep:
			if (!setBacklight(0)) {
				return false;
			}
			_powerMode = mode;
			return true;
	}
	return false;
}

bool EspIdfMipiDsiDisplay::supportsBacklight() const {
	return _config.powerControlEnabled && _powerControlReady;
}

std::uint8_t EspIdfMipiDsiDisplay::backlight() const {
	return _backlight;
}

bool EspIdfMipiDsiDisplay::setBacklight(std::uint8_t value) {
	if (!_ready && !_powerControlReady) {
		return false;
	}
	if (!_setBrightness(value)) {
		return false;
	}
	_backlight = value;
	return true;
}

bool EspIdfMipiDsiDisplay::clear(std::uint16_t rgb565) {
	if (!_ready || _powerMode != DisplayPowerMode::On || _frameBuffer == nullptr) {
		return false;
	}
	if (_config.inputColor == lexxos::platform::MipiDsiColorFormat::Rgb565) {
		auto const storedColor = rgb565ForElementOrder(rgb565, _config.rgbElementOrder);
		auto* fb565 = reinterpret_cast<std::uint16_t*>(_frameBuffer);
		std::fill(fb565, fb565 + static_cast<std::size_t>(_config.width) * static_cast<std::size_t>(_config.height), storedColor);
	} else {
		for (int y = 0; y < _config.height; ++y) {
			for (int x = 0; x < _config.width; ++x) {
				_writeFrameBufferPixel(x, y, rgb565);
			}
		}
	}
	return _flush({.x = 0, .y = 0, .width = _config.width, .height = _config.height});
}

bool EspIdfMipiDsiDisplay::beginFrame() {
	if (!_ready || _powerMode != DisplayPowerMode::On || _frameActive) {
		return false;
	}
	_frameActive = true;
	return true;
}

bool EspIdfMipiDsiDisplay::blitRgb565(Rect const& rect, std::uint16_t const* pixels, int stridePixels) {
	if (!_ready || !_frameActive || pixels == nullptr || stridePixels < rect.width || !_copyToFrameBuffer(rect, pixels, stridePixels)) {
		return false;
	}
	return _flush(rect);
}

bool EspIdfMipiDsiDisplay::fillRgb565(Rect const& rect, std::uint16_t rgb565) {
	if (!_ready || _powerMode != DisplayPowerMode::On || !_isRectValid(rect) || _frameBuffer == nullptr) {
		return false;
	}
	if (_config.inputColor == lexxos::platform::MipiDsiColorFormat::Rgb565) {
		auto const storedColor = rgb565ForElementOrder(rgb565, _config.rgbElementOrder);
		auto* fb565 = reinterpret_cast<std::uint16_t*>(_frameBuffer);
		for (int y = 0; y < rect.height; ++y) {
			auto* row = fb565 + static_cast<std::size_t>(rect.y + y) * static_cast<std::size_t>(_config.width) + rect.x;
			std::fill(row, row + rect.width, storedColor);
		}
	} else {
		for (int y = 0; y < rect.height; ++y) {
			for (int x = 0; x < rect.width; ++x) {
				_writeFrameBufferPixel(rect.x + x, rect.y + y, rgb565);
			}
		}
	}
	return _flush(rect);
}

bool EspIdfMipiDsiDisplay::beginRgb565Stream(Rect const& rect) {
	if (!_ready || _powerMode != DisplayPowerMode::On || !_frameActive || _streamActive || !_isRectValid(rect) || _frameBuffer == nullptr) {
		return false;
	}
	_streamRect = rect;
	_streamOffsetPixels = 0;
	_streamActive = true;
	return true;
}

bool EspIdfMipiDsiDisplay::writeRgb565Stream(std::uint16_t const* pixels, int count) {
	if (!_ready || _powerMode != DisplayPowerMode::On || !_streamActive || pixels == nullptr || count < 0 || _streamOffsetPixels + count > _streamRect.width * _streamRect.height) {
		return false;
	}

	int consumed = 0;
	while (consumed < count) {
		auto const row = _streamOffsetPixels / _streamRect.width;
		auto const column = _streamOffsetPixels % _streamRect.width;
		auto const chunk = std::min(count - consumed, _streamRect.width - column);
		Rect const chunkRect{.x = _streamRect.x + column, .y = _streamRect.y + row, .width = chunk, .height = 1};
		if (!_copyToFrameBuffer(chunkRect, pixels + consumed, chunk)) {
			return false;
		}
		_streamOffsetPixels += chunk;
		consumed += chunk;
	}
	return true;
}

bool EspIdfMipiDsiDisplay::writeRgb565BytesStream(std::uint8_t const* bytes, int pixelCount) {
	if (!_ready || _powerMode != DisplayPowerMode::On || !_streamActive || bytes == nullptr || pixelCount < 0 || _streamOffsetPixels + pixelCount > _streamRect.width * _streamRect.height) {
		return false;
	}

	int consumed = 0;
	while (consumed < pixelCount) {
		auto const row = _streamOffsetPixels / _streamRect.width;
		auto const column = _streamOffsetPixels % _streamRect.width;
		auto const chunk = std::min(pixelCount - consumed, _streamRect.width - column);
		Rect const chunkRect{.x = _streamRect.x + column, .y = _streamRect.y + row, .width = chunk, .height = 1};
		if (!_copyBytesToFrameBuffer(chunkRect, bytes + static_cast<std::size_t>(consumed) * 2U, chunk)) {
			return false;
		}
		_streamOffsetPixels += chunk;
		consumed += chunk;
	}
	return true;
}

bool EspIdfMipiDsiDisplay::endRgb565Stream() {
	if (!_ready || _powerMode != DisplayPowerMode::On || !_streamActive) {
		return false;
	}
	auto const complete = _streamOffsetPixels == _streamRect.width * _streamRect.height;
	auto const flushed = complete && _flush(_streamRect);
	_streamActive = false;
	_streamOffsetPixels = 0;
	return flushed;
}

bool EspIdfMipiDsiDisplay::endFrame() {
	if (!_ready || _powerMode != DisplayPowerMode::On || !_frameActive) {
		return false;
	}
	if (_streamActive && !endRgb565Stream()) {
		_frameActive = false;
		return false;
	}
	_frameActive = false;
	return true;
}

bool EspIdfMipiDsiDisplay::_initializeDsi() {
	if (!_enablePhyPower()) {
		return false;
	}

	esp_lcd_dsi_bus_handle_t dsiBus = nullptr;
	esp_lcd_dsi_bus_config_t busConfig{};
	busConfig.bus_id = 0;
	busConfig.num_data_lanes = _config.dataLanes;
	busConfig.phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT;
	busConfig.lane_bit_rate_mbps = static_cast<float>(_config.laneBitRateMbps);
	if (esp_lcd_new_dsi_bus(&busConfig, &dsiBus) != ESP_OK) {
		ESP_LOGE(tag, "failed to create MIPI DSI bus");
		return false;
	}
	_dsiBus = dsiBus;

	esp_lcd_panel_io_handle_t dbiIo = nullptr;
	esp_lcd_dbi_io_config_t dbiConfig{};
	dbiConfig.virtual_channel = _config.virtualChannel;
	dbiConfig.lcd_cmd_bits = 8;
	dbiConfig.lcd_param_bits = 8;
	if (esp_lcd_new_panel_io_dbi(dsiBus, &dbiConfig, &dbiIo) != ESP_OK) {
		ESP_LOGE(tag, "failed to create MIPI DBI control IO");
		return false;
	}
	_dbiIo = dbiIo;
	(void) esp_lcd_panel_io_del(dbiIo);
	_dbiIo = nullptr;

	esp_lcd_dpi_panel_config_t dpiConfig{};
	dpiConfig.virtual_channel = _config.virtualChannel;
	dpiConfig.dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT;
	dpiConfig.dpi_clock_freq_mhz = static_cast<float>(_config.dpiClockKhz) / 1000.0F;
	dpiConfig.in_color_format = toLcdColorFormat(_config.inputColor);
	dpiConfig.out_color_format = toLcdColorFormat(_config.outputColor);
	dpiConfig.num_fbs = _config.frameBuffers;
	dpiConfig.video_timing.h_size = _config.width;
	dpiConfig.video_timing.v_size = _config.height;
	dpiConfig.video_timing.hsync_back_porch = _config.hsyncBackPorch;
	dpiConfig.video_timing.hsync_pulse_width = _config.hsyncPulseWidth;
	dpiConfig.video_timing.hsync_front_porch = _config.hsyncFrontPorch;
	dpiConfig.video_timing.vsync_back_porch = _config.vsyncBackPorch;
	dpiConfig.video_timing.vsync_pulse_width = _config.vsyncPulseWidth;
	dpiConfig.video_timing.vsync_front_porch = _config.vsyncFrontPorch;
	dpiConfig.flags.disable_lp = _config.disableLowPower ? 1 : 0;

	esp_lcd_panel_handle_t panel = nullptr;
	if (esp_lcd_new_panel_dpi(dsiBus, &dpiConfig, &panel) != ESP_OK) {
		ESP_LOGE(tag, "failed to create MIPI DPI panel");
		return false;
	}
	_panel = panel;

	auto* privateBus = static_cast<DsiBusPrivatePrefix*>(_dsiBus);
	if (_config.nonBurstSyncPulses) {
		mipi_dsi_host_ll_dpi_set_video_burst_type(privateBus->hal.host, MIPI_DSI_LL_VIDEO_NON_BURST_WITH_SYNC_PULSES);
	}
	mipi_dsi_host_ll_dpi_enable_frame_ack(privateBus->hal.host, _config.frameAck);

	if (esp_lcd_panel_init(panel) != ESP_OK) {
		ESP_LOGE(tag, "failed to initialize MIPI DPI panel");
		return false;
	}

	if (_config.forceClockLaneHs) {
		mipi_dsi_host_ll_set_clock_lane_state(privateBus->hal.host, MIPI_DSI_LL_CLOCK_LANE_STATE_HS);
	}
	mipi_dsi_host_ll_enable_cmd_ack(privateBus->hal.host, _config.commandAck);

	if (!_releaseBridgeResetAndWake()) {
		return false;
	}
	if (!_initializeTc358762Bridge()) {
		return false;
	}

	void* fb0 = nullptr;
	if (esp_lcd_dpi_panel_get_frame_buffer(panel, 1, &fb0) != ESP_OK || fb0 == nullptr) {
		ESP_LOGE(tag, "failed to get MIPI DPI frame buffer");
		return false;
	}
	_frameBuffer = static_cast<std::uint8_t*>(fb0);
	return true;
}

bool EspIdfMipiDsiDisplay::_enablePhyPower() {
	if (_config.dsiPhyPowerLdoChannel < 0 || _phyLdo != nullptr) {
		return true;
	}

	esp_ldo_channel_handle_t ldo = nullptr;
	esp_ldo_channel_config_t ldoConfig{};
	ldoConfig.chan_id = _config.dsiPhyPowerLdoChannel;
	ldoConfig.voltage_mv = static_cast<int>(_config.dsiPhyPowerMillivolts);
	auto const result = esp_ldo_acquire_channel(&ldoConfig, &ldo);
	if (result != ESP_OK) {
		ESP_LOGE(tag, "failed to enable DSI PHY LDO%d: %s", _config.dsiPhyPowerLdoChannel, esp_err_to_name(result));
		return false;
	}

	_phyLdo = ldo;
	ESP_LOGI(tag, "MIPI DSI PHY powered by LDO%d at %" PRIu32 " mV",
	    _config.dsiPhyPowerLdoChannel,
	    _config.dsiPhyPowerMillivolts);
	return true;
}

bool EspIdfMipiDsiDisplay::_initializePowerControl() {
	if (!_config.powerControlEnabled) {
		_powerControlReady = false;
		return true;
	}
	if (!_controlBus.isReady()) {
		ESP_LOGW(tag, "MIPI DSI power-control I2C bus is not ready");
		return !_config.powerControlRequired;
	}

	if (!_controlBus.probe(_config.powerControlAddress)) {
		ESP_LOGW(tag, "MIPI DSI power-control address 0x%02x did not ACK", _config.powerControlAddress);
	}

	bool ok = true;
	ok = _writePowerRegister(rpiRegPortC, 0x00) && ok;
	_sleepMs(10);
	ok = _writePowerRegister(rpiRegPortA, paLcdLr) && ok;
	_sleepMs(10);
	ok = _writePowerRegister(rpiRegPortB, pbLcdMain) && ok;
	_sleepMs(10);
	ok = _writePowerRegister(rpiRegPortC, pcLedEn) && ok;
	_sleepMs(80);

	_powerControlReady = ok;
	if (!ok) {
		ESP_LOGW(tag, "MIPI DSI power-control init failed");
	}
	return ok || !_config.powerControlRequired;
}

bool EspIdfMipiDsiDisplay::_writePowerRegister(std::uint8_t reg, std::uint8_t value) {
	std::uint8_t data[2]{reg, value};
	return _controlBus.write(_config.powerControlAddress, data, sizeof(data));
}

bool EspIdfMipiDsiDisplay::_releaseBridgeResetAndWake() {
	if (!_config.powerControlEnabled || !_powerControlReady) {
		return !_config.powerControlRequired;
	}

	bool ok = true;
	ok = _writePowerRegister(rpiRegPortC, pcLedEn | pcResetLcdN | pcResetBridgeN) && ok;
	_sleepMs(10);
	ok = _writePowerRegister(rpiRegAddrH, 0x04) && ok;
	_sleepMs(8);
	ok = _writePowerRegister(rpiRegAddrL, 0x7C) && ok;
	_sleepMs(8);
	ok = _writePowerRegister(rpiRegWriteDataH, 0x00) && ok;
	_sleepMs(8);
	ok = _writePowerRegister(rpiRegWriteDataL, 0x00) && ok;
	_sleepMs(100);
	if (!ok) {
		ESP_LOGW(tag, "MIPI DSI bridge reset/wake command failed");
	}
	return ok || !_config.powerControlRequired;
}

bool EspIdfMipiDsiDisplay::_setBrightness(std::uint8_t brightness) {
	if (!_config.powerControlEnabled || !_powerControlReady) {
		return !_config.powerControlRequired;
	}
	return _writePowerRegister(_config.backlightPwmRegister, brightness) || !_config.powerControlRequired;
}

bool EspIdfMipiDsiDisplay::_initializeTc358762Bridge() {
	if (!_config.tc358762BridgeInit) {
		return true;
	}
	if (_dsiBus == nullptr) {
		return false;
	}

	auto const laneEnable = _config.dataLanes > 1 ? (bit(0) | bit(1) | bit(2)) : (bit(0) | bit(1));
	_writeTc358762(tcDsiLaneEnable, laneEnable);
	_writeTc358762(tcPpiD0sClrSipoCount, 0x05);
	_writeTc358762(tcPpiD1sClrSipoCount, 0x05);
	_writeTc358762(tcPpiD0sAtmr, 0x00);
	_writeTc358762(tcPpiD1sAtmr, 0x00);
	_writeTc358762(tcPpiLpTxTimeCnt, 0x03);
	_writeTc358762(tcSpiCmr, 0x00);
	_writeTc358762(tcLcdCtrl, 0x00100150);
	_writeTc358762(tcSysCtrl, 0x040F);
	_writeTc358762(tcLcdHsHbp, (static_cast<std::uint32_t>(_config.hsyncBackPorch) << 16U) | _config.hsyncPulseWidth);
	_writeTc358762(tcLcdHdispHfp, (static_cast<std::uint32_t>(_config.hsyncFrontPorch) << 16U) | static_cast<std::uint32_t>(_config.width));
	_writeTc358762(tcLcdVsVbp, (static_cast<std::uint32_t>(_config.vsyncBackPorch) << 16U) | _config.vsyncPulseWidth);
	_writeTc358762(tcLcdVdispVfp, (static_cast<std::uint32_t>(_config.vsyncFrontPorch) << 16U) | static_cast<std::uint32_t>(_config.height));
	_sleepMs(100);
	_writeTc358762(tcPpiStartPpi, 0x01);
	_writeTc358762(tcDsiStartDsi, 0x01);
	_sleepMs(100);
	ESP_LOGI(tag, "TC358762-compatible bridge init sequence sent");
	return true;
}

void EspIdfMipiDsiDisplay::_writeTc358762(std::uint16_t reg, std::uint32_t value) {
	auto* privateBus = static_cast<DsiBusPrivatePrefix*>(_dsiBus);
	std::uint8_t payload[6]{
	    static_cast<std::uint8_t>(reg & 0xFFU),
	    static_cast<std::uint8_t>((reg >> 8U) & 0xFFU),
	    static_cast<std::uint8_t>(value & 0xFFU),
	    static_cast<std::uint8_t>((value >> 8U) & 0xFFU),
	    static_cast<std::uint8_t>((value >> 16U) & 0xFFU),
	    static_cast<std::uint8_t>((value >> 24U) & 0xFFU),
	};
	mipi_dsi_hal_host_gen_write_long_packet(&privateBus->hal, _config.virtualChannel, MIPI_DSI_DT_GENERIC_LONG_WRITE, payload, sizeof(payload));
}

bool EspIdfMipiDsiDisplay::_flush(Rect const& rect) {
	if (_panel == nullptr || _frameBuffer == nullptr || !_isRectValid(rect)) {
		return false;
	}
	auto const result = esp_lcd_panel_draw_bitmap_2d(static_cast<esp_lcd_panel_handle_t>(_panel),
	    rect.x,
	    rect.y,
	    rect.x + rect.width,
	    rect.y + rect.height,
	    _frameBuffer,
	    static_cast<std::size_t>(_config.width),
	    static_cast<std::size_t>(_config.height),
	    rect.x,
	    rect.y,
	    rect.x + rect.width,
	    rect.y + rect.height);
	return result == ESP_OK;
}

bool EspIdfMipiDsiDisplay::_copyToFrameBuffer(Rect const& rect, std::uint16_t const* pixels, int stridePixels) {
	if (!_isRectValid(rect) || pixels == nullptr || stridePixels < rect.width || _frameBuffer == nullptr) {
		return false;
	}
	if (_config.inputColor == lexxos::platform::MipiDsiColorFormat::Rgb565) {
		auto* fb565 = reinterpret_cast<std::uint16_t*>(_frameBuffer);
		for (int y = 0; y < rect.height; ++y) {
			auto* destination = fb565 + static_cast<std::size_t>(rect.y + y) * static_cast<std::size_t>(_config.width) + rect.x;
			auto const* source = pixels + static_cast<std::size_t>(y) * static_cast<std::size_t>(stridePixels);
			if (_config.rgbElementOrder == lexxos::platform::MipiDsiRgbElementOrder::Rgb) {
				std::memcpy(destination, source, static_cast<std::size_t>(rect.width) * sizeof(std::uint16_t));
			} else {
				for (int x = 0; x < rect.width; ++x) {
					destination[x] = rgb565ForElementOrder(source[x], _config.rgbElementOrder);
				}
			}
		}
	} else {
		for (int y = 0; y < rect.height; ++y) {
			auto const* source = pixels + static_cast<std::size_t>(y) * static_cast<std::size_t>(stridePixels);
			for (int x = 0; x < rect.width; ++x) {
				_writeFrameBufferPixel(rect.x + x, rect.y + y, source[x]);
			}
		}
	}
	return true;
}

bool EspIdfMipiDsiDisplay::_copyBytesToFrameBuffer(Rect const& rect, std::uint8_t const* bytes, int stridePixels) {
	if (!_isRectValid(rect) || bytes == nullptr || stridePixels < rect.width || _frameBuffer == nullptr) {
		return false;
	}
	if (_config.inputColor == lexxos::platform::MipiDsiColorFormat::Rgb565) {
		auto* fb565 = reinterpret_cast<std::uint16_t*>(_frameBuffer);
		for (int y = 0; y < rect.height; ++y) {
			auto* destination = fb565 + static_cast<std::size_t>(rect.y + y) * static_cast<std::size_t>(_config.width) + rect.x;
			auto const* source = bytes + static_cast<std::size_t>(y) * static_cast<std::size_t>(stridePixels) * sizeof(std::uint16_t);
			for (int x = 0; x < rect.width; ++x) {
				auto const sourceColor = rgb565FromBigEndianBytes(source + static_cast<std::size_t>(x) * sizeof(std::uint16_t));
				destination[x] = rgb565ForElementOrder(sourceColor, _config.rgbElementOrder);
			}
		}
	} else {
		for (int y = 0; y < rect.height; ++y) {
			auto const* source = bytes + static_cast<std::size_t>(y) * static_cast<std::size_t>(stridePixels) * sizeof(std::uint16_t);
			for (int x = 0; x < rect.width; ++x) {
				_writeFrameBufferPixel(rect.x + x,
				    rect.y + y,
				    rgb565FromBigEndianBytes(source + static_cast<std::size_t>(x) * sizeof(std::uint16_t)));
			}
		}
	}
	return true;
}

int EspIdfMipiDsiDisplay::_frameBufferBytesPerPixel() const {
	return bytesPerPixel(_config.inputColor);
}

std::uint8_t* EspIdfMipiDsiDisplay::_frameBufferPixel(int x, int y) {
	if (_frameBuffer == nullptr) {
		return nullptr;
	}
	auto const offset = (static_cast<std::size_t>(y) * static_cast<std::size_t>(_config.width) + static_cast<std::size_t>(x))
	    * static_cast<std::size_t>(_frameBufferBytesPerPixel());
	return _frameBuffer + offset;
}

void EspIdfMipiDsiDisplay::_writeFrameBufferPixel(int x, int y, std::uint16_t rgb565) {
	auto* pixel = _frameBufferPixel(x, y);
	if (pixel == nullptr) {
		return;
	}
	if (_config.inputColor == lexxos::platform::MipiDsiColorFormat::Rgb565) {
		auto* destination = reinterpret_cast<std::uint16_t*>(pixel);
		*destination = rgb565ForElementOrder(rgb565, _config.rgbElementOrder);
		return;
	}

	auto const red = expand5To8(static_cast<std::uint16_t>((rgb565 >> 11U) & 0x1FU));
	auto const green = expand6To8(static_cast<std::uint16_t>((rgb565 >> 5U) & 0x3FU));
	auto const blue = expand5To8(static_cast<std::uint16_t>(rgb565 & 0x1FU));
	if (_config.rgbElementOrder == lexxos::platform::MipiDsiRgbElementOrder::Bgr) {
		pixel[0] = blue;
		pixel[1] = green;
		pixel[2] = red;
	} else {
		pixel[0] = red;
		pixel[1] = green;
		pixel[2] = blue;
	}
}

bool EspIdfMipiDsiDisplay::_isRectValid(Rect const& rect) const {
	return rect.x >= 0 && rect.y >= 0 && rect.width > 0 && rect.height > 0 && rect.x + rect.width <= _config.width && rect.y + rect.height <= _config.height;
}

void EspIdfMipiDsiDisplay::_sleepMs(std::uint32_t milliseconds) const {
	if (milliseconds == 0) {
		return;
	}
	vTaskDelay(pdMS_TO_TICKS(milliseconds));
}

} // namespace lexxos::devices
