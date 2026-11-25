#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include <lexxos/devices/bus/spi_clock_mode.h>
#include <lexxos/platform/board_identity.h>
#include <lexxos/platform/processor.h>

namespace lexxos::platform {

inline constexpr int invalidPin = -1;

struct GpioPin {
	int number{invalidPin};

	[[nodiscard]] bool isValid() const {
		return number >= 0;
	}
};

enum class GpioActiveLevel : std::uint8_t {
	Low,
	High,
};

enum class AnalogAttenuation : std::uint8_t {
	Db0,
	Db2_5,
	Db6,
	Db12,
};

struct AnalogInputConfig {
	std::string_view name;
	GpioPin pin;
	std::uint8_t resolutionBits{12};
	AnalogAttenuation attenuation{AnalogAttenuation::Db12};
};

struct AnalogOutputConfig {
	std::string_view name;
	GpioPin pin;
	std::uint8_t resolutionBits{8};
	std::uint32_t initialCode{0};
};

struct PwmOutputConfig {
	std::string_view name;
	GpioPin pin;
	std::uint8_t channel{0};
	std::uint8_t timer{0};
	std::uint32_t frequencyHz{1000};
	std::uint8_t resolutionBits{10};
	GpioActiveLevel activeLevel{GpioActiveLevel::High};
	std::uint16_t initialDutyPermille{0};
};

enum class GpioPull : std::uint8_t {
	None,
	Up,
	Down,
};

enum class SdInterface : std::uint8_t {
	None,
	Spi,
	Sdio1Bit,
	Sdio4Bit,
};

[[nodiscard]] inline std::string_view toString(SdInterface interfaceType) {
	switch (interfaceType) {
		case SdInterface::None:
			return "none";
		case SdInterface::Spi:
			return "spi";
		case SdInterface::Sdio1Bit:
			return "sdio-1bit";
		case SdInterface::Sdio4Bit:
			return "sdio-4bit";
	}

	return "unknown";
}

[[nodiscard]] inline bool isSdioInterface(SdInterface interfaceType) {
	return interfaceType == SdInterface::Sdio1Bit || interfaceType == SdInterface::Sdio4Bit;
}

[[nodiscard]] inline std::string_view toString(GpioActiveLevel level) {
	switch (level) {
		case GpioActiveLevel::Low:
			return "low";
		case GpioActiveLevel::High:
			return "high";
	}

	return "unknown";
}

[[nodiscard]] inline std::string_view toString(GpioPull pull) {
	switch (pull) {
		case GpioPull::None:
			return "none";
		case GpioPull::Up:
			return "up";
		case GpioPull::Down:
			return "down";
	}

	return "unknown";
}

struct GpioCardDetect {
	GpioPin detect;
	GpioActiveLevel insertedLevel{GpioActiveLevel::Low};

	[[nodiscard]] bool isValid() const {
		return detect.isValid();
	}
};

// Optional description for boards with several independent SD media.
// storageSlotCount == 0 retains the existing single-sd0 behavior of all
// legacy boards; board-specific drivers still own initialization/mounting.
enum class SdMediaRole : std::uint8_t {
	Removable,
	System,
};

struct BoardStorageSlot {
	char const* deviceName{nullptr}; // VFS /dev/<deviceName>, e.g. sd0, sd1.
	char const* mountPath{nullptr}; // nullptr when raw media is not auto-mounted.
	SdMediaRole role{SdMediaRole::Removable};
	bool hotSwappable{false};
	bool autoMount{false};
	bool readOnlyByDefault{true};
};

// Up to four independent devices, matching the default VFS device limit.
inline constexpr std::uint8_t maxBoardStorageSlots = 4;

inline constexpr std::uint8_t maxBoardUserGpioPins = 16;

enum class GpioDriver : std::uint8_t {
	None,
	DigitalInput,
	DigitalOutput
};

struct GpioInputTiming {
	std::uint32_t debounceMs{25};
	std::uint32_t clickGapMs{300};
	std::uint32_t holdMs{1500};
	std::uint32_t longHoldMs{10000};
};

struct BoardUserGpioPin {
	GpioPin pin{-1};
	char const* label{nullptr};
	bool inputAllowed{true};
	bool outputAllowed{true};
	GpioPull defaultPull{GpioPull::None};
	bool defaultVisible{false};
	GpioDriver driver{GpioDriver::None};
	GpioActiveLevel activeLevel{GpioActiveLevel::Low}; // Для DigitalInput.
	bool initialHigh{false}; // Для DigitalOutput.
	GpioInputTiming const* inputTiming{nullptr}; // Необязательные статические настройки платы.

	static constexpr BoardUserGpioPin digitalInput(GpioPin pin, char const* label,
	    GpioPull pull = GpioPull::None, GpioActiveLevel activeLevel = GpioActiveLevel::Low,
	    GpioInputTiming const* timing = nullptr) {
		return {.pin = pin, .label = label, .outputAllowed = false, .defaultPull = pull, .driver = GpioDriver::DigitalInput, .activeLevel = activeLevel, .inputTiming = timing};
	}

	static constexpr BoardUserGpioPin digitalOutput(GpioPin pin, char const* label, bool initialHigh = false) {
		return {.pin = pin, .label = label, .inputAllowed = false, .driver = GpioDriver::DigitalOutput, .initialHigh = initialHigh};
	}

	[[nodiscard]] bool isValid() const {
		return pin.isValid() && label != nullptr && (inputAllowed || outputAllowed);
	}
};

struct BoardUserGpio {
	BoardUserGpioPin pins[maxBoardUserGpioPins]{};
	std::uint8_t count{0};

	[[nodiscard]] bool isValid() const {
		return count > 0;
	}
};

struct UartPins {
	int index{0};
	GpioPin tx{-1};
	GpioPin rx{-1};
	std::uint32_t baudRate{115200};

	[[nodiscard]] bool isValid() const {
		return tx.isValid() && rx.isValid();
	}
};

struct I2cPins {
	int index{0};
	GpioPin sda{-1};
	GpioPin scl{-1};
	std::uint32_t baudRate{400000};

	[[nodiscard]] bool isValid() const {
		return sda.isValid() && scl.isValid();
	}
};

struct NamedI2cBus {
	std::string_view name;
	I2cPins pins;
};

struct SpiPins {
	int index{0};
	GpioPin sck{-1};
	GpioPin mosi{-1};
	GpioPin miso{-1};
	GpioPin cs{-1};
	std::uint32_t baudRate{2000000};

	[[nodiscard]] bool isValid() const {
		return sck.isValid() && mosi.isValid() && miso.isValid() && cs.isValid();
	}
};

struct SpiInterfacePins {
	int index{-1};
	GpioPin sck{-1};
	GpioPin mosi{-1};
	GpioPin miso{-1};
	std::uint32_t baudRate{2000000};

	[[nodiscard]] bool isTxValid() const {
		return index >= 0 && sck.isValid() && mosi.isValid();
	}

	[[nodiscard]] bool isFullDuplexValid() const {
		return isTxValid() && miso.isValid();
	}
};

// Имена устройств уникальны среди всех SPI-шин платы.
struct SpiDeviceConfig {
	std::string_view name;
	GpioPin cs{-1};
	std::uint32_t baudRate{2000000};
	devices::SpiClockMode mode{devices::SpiClockMode::Mode0};
};

struct NamedSpiBus {
	std::string_view name;
	SpiInterfacePins pins;
	std::span<SpiDeviceConfig const> devices;
};

struct SdioPins {
	int index{0};
	GpioPin clock{-1};
	GpioPin command{-1};
	GpioPin data0{-1};
	GpioPin data1{-1};
	GpioPin data2{-1};
	GpioPin data3{-1};
	std::uint32_t initializationClockHz{400000};
	std::uint32_t transferClockHz{24000000};
	int powerLdoChannel{-1};

	[[nodiscard]] bool isValid() const {
		return clock.isValid() && command.isValid() && data0.isValid();
	}

	[[nodiscard]] bool supportsWideBus() const {
		return data0.isValid() && data1.isValid() && data2.isValid() && data3.isValid();
	}
};

enum class SpiTftController : std::uint8_t {
	Ili934x,
	St7789,
};

struct SpiTftDisplayPins {
	bool enabled{false};
	int spiIndex{-1};
	std::uint32_t spiBaudRate{16000000};
	GpioPin sck{-1};
	GpioPin mosi{-1};
	GpioPin cs{-1};
	GpioPin dc{-1};
	GpioPin reset{-1};
	GpioPin backlight{-1};
	int width{128};
	int height{160};
	int columnOffset{0};
	int rowOffset{0};
	bool invertColors{false};
	SpiTftController controller{SpiTftController::Ili934x};
	std::uint8_t madctl{0x28};
	bool addressWindowSwapsAxes{true};

	[[nodiscard]] bool isValid() const {
		return enabled && sck.isValid() && mosi.isValid() && cs.isValid() && dc.isValid() && reset.isValid();
	}
};

struct ParallelTftDisplayPins {
	bool enabled{false};
	char const* name{"parallel-tft"};
	int width{320};
	int height{240};
	GpioPin chipSelect{-1};
	GpioPin registerSelect{-1};
	GpioPin read{-1};
	GpioPin write{-1};
	GpioPin reset{-1};
	GpioPin backlight{-1};
	GpioPin data[16]{};

	[[nodiscard]] bool isValid() const {
		if (!enabled || name == nullptr || width <= 0 || height <= 0 || !chipSelect.isValid() || !registerSelect.isValid() || !read.isValid() || !write.isValid()) {
			return false;
		}

		for (auto const pin : data) {
			if (!pin.isValid()) {
				return false;
			}
		}

		return true;
	}
};

enum class MipiDsiPanelKind : std::uint8_t {
	GenericDpi,
	RaspberryPiTouch7InchV1,
};

[[nodiscard]] inline std::string_view toString(MipiDsiPanelKind kind) {
	switch (kind) {
		case MipiDsiPanelKind::GenericDpi:
			return "generic-dpi";
		case MipiDsiPanelKind::RaspberryPiTouch7InchV1:
			return "rpi-touch-7-v1";
	}

	return "unknown";
}

enum class MipiDsiColorFormat : std::uint8_t {
	Rgb565,
	Rgb888,
};

[[nodiscard]] inline std::string_view toString(MipiDsiColorFormat format) {
	switch (format) {
		case MipiDsiColorFormat::Rgb565:
			return "rgb565";
		case MipiDsiColorFormat::Rgb888:
			return "rgb888";
	}

	return "unknown";
}

enum class MipiDsiRgbElementOrder : std::uint8_t {
	Rgb,
	Bgr,
};

[[nodiscard]] inline std::string_view toString(MipiDsiRgbElementOrder order) {
	switch (order) {
		case MipiDsiRgbElementOrder::Rgb:
			return "rgb";
		case MipiDsiRgbElementOrder::Bgr:
			return "bgr";
	}

	return "unknown";
}

struct MipiDsiDisplayConfig {
	bool enabled{false};
	char const* name{"mipi-dsi"};
	MipiDsiPanelKind panelKind{MipiDsiPanelKind::GenericDpi};
	int width{800};
	int height{480};
	std::uint8_t virtualChannel{0};
	std::uint8_t dataLanes{1};
	std::uint32_t laneBitRateMbps{600};
	std::uint32_t dpiClockKhz{25979};
	MipiDsiColorFormat inputColor{MipiDsiColorFormat::Rgb565};
	MipiDsiColorFormat outputColor{MipiDsiColorFormat::Rgb888};
	MipiDsiRgbElementOrder rgbElementOrder{MipiDsiRgbElementOrder::Rgb};
	std::uint8_t frameBuffers{1};
	std::uint16_t hsyncPulseWidth{2};
	std::uint16_t hsyncBackPorch{46};
	std::uint16_t hsyncFrontPorch{1};
	std::uint16_t vsyncPulseWidth{2};
	std::uint16_t vsyncBackPorch{21};
	std::uint16_t vsyncFrontPorch{7};
	int dsiPhyPowerLdoChannel{-1};
	std::uint32_t dsiPhyPowerMillivolts{2500};
	bool powerControlEnabled{false};
	bool powerControlRequired{false};
	int powerControlI2cIndex{0};
	std::uint8_t powerControlAddress{0x45};
	std::uint8_t backlightPwmRegister{0x86};
	std::uint8_t defaultBrightness{255};
	bool tc358762BridgeInit{false};
	bool nonBurstSyncPulses{false};
	bool forceClockLaneHs{false};
	bool disableLowPower{false};
	bool frameAck{false};
	bool commandAck{false};

	[[nodiscard]] bool isValid() const {
		return enabled && name != nullptr && width > 0 && height > 0 && virtualChannel < 4 && dataLanes > 0 && dataLanes <= 2 && laneBitRateMbps > 0 && dpiClockKhz > 0 && frameBuffers > 0 && frameBuffers <= 3 && hsyncPulseWidth > 0 && vsyncPulseWidth > 0;
	}
};

enum class TouchPanelKind : std::uint8_t {
	None,
	Gt911,
	FocalTechFt5x06,
};

[[nodiscard]] inline std::string_view toString(TouchPanelKind kind) {
	switch (kind) {
		case TouchPanelKind::None:
			return "none";
		case TouchPanelKind::Gt911:
			return "gt911";
		case TouchPanelKind::FocalTechFt5x06:
			return "focaltech-ft5x06";
	}

	return "unknown";
}

struct TouchPanelConfig {
	bool enabled{false};
	TouchPanelKind controller{TouchPanelKind::None};
	char const* name{"touch"};
	int i2cIndex{0};
	std::uint8_t address{0x5D};
	GpioPin interrupt{-1};
	GpioPin reset{-1};
	GpioActiveLevel interruptActiveLevel{GpioActiveLevel::Low};
	std::uint16_t rawWidth{800};
	std::uint16_t rawHeight{480};
	bool swapXY{false};
	bool invertX{false};
	bool invertY{false};

	[[nodiscard]] bool isValid() const {
		return enabled && controller != TouchPanelKind::None && name != nullptr && address != 0 && rawWidth > 0 && rawHeight > 0;
	}
};

struct Pcf8575Pins {
	bool enabled{false};
	char const* name{"pcf8575"};
	int i2cIndex{0};
	std::uint8_t address{0x20};
	GpioPin interrupt;
	GpioActiveLevel interruptActiveLevel{GpioActiveLevel::Low};

	[[nodiscard]] bool isValid() const {
		return enabled && name != nullptr;
	}
};

struct Pcf8575MatrixKeyboardPins {
	bool enabled{false};
	char const* name{"matrix-keyboard"};
	char const* expanderName{"pcf8575"};
	std::uint8_t rows[8]{};
	std::uint8_t rowCount{0};
	std::uint8_t columns[8]{};
	std::uint8_t columnCount{0};
	GpioActiveLevel rowActiveLevel{GpioActiveLevel::Low};
	GpioActiveLevel columnActiveLevel{GpioActiveLevel::Low};
	std::uint32_t scanPeriodMs{5};
	std::uint32_t debounceMs{20};

	[[nodiscard]] bool isValid() const {
		return enabled && name != nullptr && expanderName != nullptr && rowCount > 0 && columnCount > 0;
	}
};

struct SegaGenesisGamepadPins {
	bool enabled{false};
	char const* name{"sega-gamepad"};
	char const* expanderName{"pcf8575"};
	std::uint8_t up{0};
	std::uint8_t down{1};
	std::uint8_t left{2};
	std::uint8_t right{3};
	std::uint8_t buttonBa{4};
	std::uint8_t buttonCStart{5};
	GpioPin select;

	[[nodiscard]] bool isValid() const {
		return enabled && name != nullptr && expanderName != nullptr && up < 16 && down < 16 && left < 16 && right < 16 && buttonBa < 16 && buttonCStart < 16 && select.isValid();
	}
};

struct BoardInterfaces {
	UartPins consoleUart{};
	I2cPins systemI2c{};
	SpiInterfacePins storageSpi{};
	SpiInterfacePins displaySpi{};
};

struct PsramPins {
	SpiPins spi;
	GpioPin sio2{-1};
	GpioPin sio3{-1};

	[[nodiscard]] bool isQuadSpiLike() const {
		return spi.isValid() && sio2.isValid() && sio3.isValid();
	}
};

struct AudioBeepPins {
	bool enabled{false};
	GpioPin left{-1};
	GpioPin right{-1};
	std::uint32_t defaultFrequencyHz{880};
	std::uint32_t defaultDurationMs{120};

	[[nodiscard]] bool hasLeft() const {
		return enabled && left.isValid();
	}

	[[nodiscard]] bool hasRight() const {
		return enabled && right.isValid();
	}

	[[nodiscard]] bool isValid() const {
		return hasLeft() || hasRight();
	}
};

enum class AudioCodecKind : std::uint8_t {
	None,
	Es8311,
};

[[nodiscard]] inline std::string_view toString(AudioCodecKind codec) {
	switch (codec) {
		case AudioCodecKind::None:
			return "none";
		case AudioCodecKind::Es8311:
			return "es8311";
	}

	return "unknown";
}

struct AudioI2sCodecPins {
	bool enabled{false};
	char const* name{"espidf-es8311-audio"};
	char const* microphoneName{"espidf-es8311-mic"};
	AudioCodecKind codec{AudioCodecKind::None};
	int i2sIndex{0};
	GpioPin mclk{-1};
	GpioPin bclk{-1};
	GpioPin wordSelect{-1};
	GpioPin dataOut{-1};
	GpioPin dataIn{-1};
	GpioPin amplifierEnable{-1};
	GpioActiveLevel amplifierEnableLevel{GpioActiveLevel::High};
	std::uint8_t i2cAddress{0};
	std::uint32_t sampleRateHz{48000};
	std::uint32_t defaultFrequencyHz{880};
	std::uint32_t defaultDurationMs{120};

	[[nodiscard]] bool hasOutput() const {
		return enabled && codec != AudioCodecKind::None && bclk.isValid() && wordSelect.isValid() && dataOut.isValid();
	}

	[[nodiscard]] bool hasInput() const {
		return enabled && codec != AudioCodecKind::None && bclk.isValid() && wordSelect.isValid() && dataIn.isValid();
	}

	[[nodiscard]] bool isValid() const {
		return hasOutput() && i2cAddress != 0 && sampleRateHz != 0 && i2sIndex >= 0 && i2sIndex <= 1;
	}
};

enum class MotionSensorKind : std::uint8_t {
	None,
	Bmi270,
};

[[nodiscard]] inline std::string_view toString(MotionSensorKind sensor) {
	switch (sensor) {
		case MotionSensorKind::None:
			return "none";
		case MotionSensorKind::Bmi270:
			return "bmi270";
	}

	return "unknown";
}

struct MotionSensorPins {
	bool enabled{false};
	MotionSensorKind sensor{MotionSensorKind::None};
	char const* name{"motion-sensor"};
	std::uint8_t i2cAddress{0};

	[[nodiscard]] bool isValid() const {
		return enabled && sensor != MotionSensorKind::None && name != nullptr && i2cAddress != 0;
	}
};

struct InfraredTransmitterPins {
	bool enabled{false};
	char const* name{"ir"};
	GpioPin output{-1};
	std::uint32_t carrierFrequencyHz{38000};

	[[nodiscard]] bool isValid() const {
		return enabled && name != nullptr && output.isValid() && carrierFrequencyHz != 0;
	}
};

struct WirelessCapabilities {
	bool wifi{false};
	bool bluetooth{false};
	char const* radio{"none"};
	char const* interfaceName{"wifi0"};
	char const* backend{"none"};
	bool autoConnect{false};
	bool scan{false};
	SdioPins hostedSdio{};
	GpioPin hostedReset{-1};
};

enum class EthernetPhyKind : std::uint8_t {
	None,
	Ip101,
	GenericRmii,
};

[[nodiscard]] inline std::string_view toString(EthernetPhyKind phy) {
	switch (phy) {
		case EthernetPhyKind::None:
			return "none";
		case EthernetPhyKind::Ip101:
			return "ip101";
		case EthernetPhyKind::GenericRmii:
			return "generic-rmii";
	}

	return "unknown";
}

struct EthernetRmiiPins {
	bool enabled{false};
	EthernetPhyKind phy{EthernetPhyKind::None};
	char const* name{"eth0"};
	GpioPin mdc{-1};
	GpioPin mdio{-1};
	GpioPin reset{-1};
	GpioPin clock{-1};
	GpioPin txd0{-1};
	GpioPin txd1{-1};
	GpioPin txEnable{-1};
	GpioPin rxd0{-1};
	GpioPin rxd1{-1};
	GpioPin crsDv{-1};
	std::uint8_t phyAddress{0};

	[[nodiscard]] bool isValid() const {
		return enabled && phy != EthernetPhyKind::None && name != nullptr && mdc.isValid() && mdio.isValid() && clock.isValid() && txd0.isValid() && txd1.isValid() && txEnable.isValid() && rxd0.isValid() && rxd1.isValid() && crsDv.isValid();
	}
};

struct SoftwareRtcConfig {
	bool enabled{false};
	bool preferAsDefault{false};
	bool autoNetworkSync{false};
	char const* networkSyncServer{"pool.ntp.org"};
	std::uint32_t networkSyncTimeoutMs{8000};
	std::uint32_t networkSyncIntervalMs{60U * 60U * 1000U};
	std::uint32_t networkSyncRetryIntervalMs{60U * 1000U};
	std::uint32_t startupDelayMs{3000};
	std::int32_t timezoneOffsetSeconds{0};

	[[nodiscard]] bool isValid() const {
		return enabled;
	}
};

struct LegacyBoardDescriptor {
	char const* boardId{"unknown"};
	char const* boardName{"Unknown board"};
	char const* vendor{"unknown"};
	ProcessorFamily processorFamily{ProcessorFamily::Unknown};
	char const* processorName{"unknown"};

	GpioPin statusLed{};
	GpioActiveLevel statusLedActiveLevel{GpioActiveLevel::High};
	BoardUserGpio userGpio{};
	UartPins debugUart{};
	I2cPins debugOledI2c{};
	BoardInterfaces interfaces{};
	SdInterface sdInterface{SdInterface::None};
	SpiPins sdSpi{};
	SdioPins sdSdio{};
	GpioCardDetect sdCardDetect{};
	PsramPins psram{};
	AudioBeepPins audioBeep{};
	AudioI2sCodecPins audioCodec{};
	MotionSensorPins motionSensor{};
	InfraredTransmitterPins infraredTransmitter{};
	WirelessCapabilities wireless{};
	EthernetRmiiPins ethernet{};
	SoftwareRtcConfig softwareRtc{};
	SpiTftDisplayPins spiTftDisplay{};
	MipiDsiDisplayConfig mipiDsiDisplay{};
	TouchPanelConfig touchPanel{};
	Pcf8575Pins keyboardExpander{};
	ParallelTftDisplayPins parallelTftDisplay{};
	Pcf8575MatrixKeyboardPins keyboard{};
	SegaGenesisGamepadPins segaGamepad{};
	BoardStorageSlot storageSlots[maxBoardStorageSlots]{};
	std::uint8_t storageSlotCount{0}; // Zero = legacy sd0 default; no behavior change.
	std::span<NamedI2cBus const> i2cBuses{}; // CE: таблица платы со статическим временем жизни.
	std::span<NamedSpiBus const> spiBuses{}; // Таблицы и имена должны пережить объекты DeviceManager.
	std::span<AnalogInputConfig const> analogInputs{};
	std::span<PwmOutputConfig const> pwmOutputs{};
	std::span<AnalogOutputConfig const> analogOutputs{};
};

// Identity adapter for consumers migrating away from the legacy projection.
[[nodiscard]] constexpr BoardDescriptor boardIdentity(LegacyBoardDescriptor const& legacy) {
	return {
	    .boardId = legacy.boardId,
	    .boardName = legacy.boardName,
	    .vendor = legacy.vendor,
	    .processorFamily = legacy.processorFamily,
	    .processorName = legacy.processorName,
	};
}

// Storage backends interpret this policy; other boards with no explicit slots
// continue using their existing sd0 logic without any behavior change.
[[nodiscard]] inline BoardStorageSlot const* findBoardStorageSlot(
    LegacyBoardDescriptor const& descriptor, std::string_view deviceName) {
	for (std::uint8_t i = 0; i < descriptor.storageSlotCount && i < maxBoardStorageSlots; ++i) {
		auto const& slot = descriptor.storageSlots[i];
		if (slot.deviceName != nullptr && std::string_view{slot.deviceName} == deviceName) {
			return &slot;
		}
	}

	return nullptr;
}

[[nodiscard]] inline UartPins effectiveDebugUart(LegacyBoardDescriptor const& descriptor) {
	if (descriptor.debugUart.isValid()) {
		return descriptor.debugUart;
	}
	return descriptor.interfaces.consoleUart;
}

[[nodiscard]] inline I2cPins effectiveDebugI2c(LegacyBoardDescriptor const& descriptor) {
	if (descriptor.debugOledI2c.isValid()) {
		return descriptor.debugOledI2c;
	}
	return descriptor.interfaces.systemI2c;
}

[[nodiscard]] inline SpiPins effectiveSdSpi(LegacyBoardDescriptor const& descriptor) {
	auto pins = descriptor.sdSpi;
	auto const& interface = descriptor.interfaces.storageSpi;
	if (interface.isFullDuplexValid()) {
		pins.index = interface.index;
		pins.sck = interface.sck;
		pins.mosi = interface.mosi;
		pins.miso = interface.miso;
		pins.baudRate = interface.baudRate;
	}

	return pins;
}

[[nodiscard]] inline SpiTftDisplayPins effectiveSpiTftDisplay(LegacyBoardDescriptor const& descriptor) {
	auto pins = descriptor.spiTftDisplay;
	auto const& interface = descriptor.interfaces.displaySpi;
	if (interface.isTxValid()) {
		pins.spiIndex = interface.index;
		pins.sck = interface.sck;
		pins.mosi = interface.mosi;
		pins.spiBaudRate = interface.baudRate;
	}

	return pins;
}

[[nodiscard]] inline SdInterface effectiveSdInterface(LegacyBoardDescriptor const& descriptor) {
	if (descriptor.sdInterface != SdInterface::None) {
		return descriptor.sdInterface;
	}

	if (effectiveSdSpi(descriptor).isValid()) {
		return SdInterface::Spi;
	}

	if (descriptor.sdSdio.isValid()) {
		return descriptor.sdSdio.supportsWideBus() ? SdInterface::Sdio4Bit : SdInterface::Sdio1Bit;
	}

	if (descriptor.sdSpi.isValid()) {
		return SdInterface::Spi;
	}

	return SdInterface::None;
}

LegacyBoardDescriptor const& activeBoardDescriptor();

} // namespace lexxos::platform
