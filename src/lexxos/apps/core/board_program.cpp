#include <lexxos/apps/core/board_program.h>

#include <cstdint>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/os/app_context.h>

#if defined(LEXXOS_PLATFORM_ESPIDF) && defined(LEXXOS_ESPIDF_ENABLE_HOSTED)
	#include <lexxos/devices/espidf/espidf_hosted.h>
#endif

namespace lexxos::apps::system_programs {

namespace {

	void writeIndex(lexxos::os::AppContext& context, int index) {
		if (index < 0) {
			context.write("-");
			return;
		}
		writeUnsigned(context, static_cast<std::uint32_t>(index));
	}

	void writeUserGpioSummary(lexxos::os::AppContext& context, lexxos::platform::BoardDescriptor const& board) {
		context.write("user-gpio: count=");
		writeUnsigned(context, board.userGpio.count);
		context.writeLine("");

		for (std::uint8_t index = 0; index < board.userGpio.count && index < lexxos::platform::maxBoardUserGpioPins; ++index) {
			auto const& userPin = board.userGpio.pins[index];
			if (!userPin.isValid()) {
				continue;
			}

			context.write("user-gpio[");
			writeUnsigned(context, index);
			context.write("]: label=");
			context.write(userPin.label);
			context.write(" pin=");
			writePin(context, userPin.pin);
			context.write(" input=");
			context.write(userPin.inputAllowed ? "yes" : "no");
			context.write(" output=");
			context.write(userPin.outputAllowed ? "yes" : "no");
			context.write(" default-pull=");
			context.write(lexxos::platform::toString(userPin.defaultPull));
			context.write(" default-visible=");
			context.writeLine(userPin.defaultVisible ? "yes" : "no");
		}
	}

	void writeRuntimeCapabilities(lexxos::os::AppContext& context) {
		auto const capabilities = context.capabilities();
		context.write("capabilities:");
		bool wroteAny = false;
		for (auto const& descriptor : lexxos::runtime::knownCapabilities) {
			if (!capabilities.has(descriptor.capability)) {
				continue;
			}
			context.write(" ");
			context.write(descriptor.name);
			wroteAny = true;
		}
		if (!wroteAny) {
			context.write(" none");
		}
		context.writeLine("");
	}

#if defined(LEXXOS_PLATFORM_ESPIDF) && defined(LEXXOS_ESPIDF_ENABLE_HOSTED)
	void writeHostedVersion(lexxos::os::AppContext& context, lexxos::devices::espidf_hosted::Version version) {
		if (!version.valid) {
			context.write("-");
			return;
		}
		writeUnsigned(context, version.major);
		context.write(".");
		writeUnsigned(context, version.minor);
		context.write(".");
		writeUnsigned(context, version.patch);
	}

	void writeHostedSummary(lexxos::os::AppContext& context, lexxos::platform::BoardDescriptor const& board) {
		auto const info = lexxos::devices::espidf_hosted::readInfo(board, false);
		if (!info.supported) {
			return;
		}
		context.write("hosted: host-version=");
		writeHostedVersion(context, info.hostVersion);
		context.write(" coprocessor-version=");
		writeHostedVersion(context, info.coprocessorVersion);
		context.write(" version-error=");
		context.write(info.versionErrorName);
		context.write(" compatible=");
		context.write(info.compatibilityKnown ? (info.majorMinorCompatible ? "yes" : "no") : "unknown");
		context.write(" app-desc=");
		context.write(info.app.valid ? "yes" : "no");
		context.write(" app-error=");
		context.writeLine(info.appErrorName);
		if (info.app.valid) {
			context.write("hosted/app: name=");
			context.write(info.app.projectName[0] != '\0' ? info.app.projectName : "-");
			context.write(" version=");
			context.write(info.app.version[0] != '\0' ? info.app.version : "-");
			context.write(" idf=");
			context.write(info.app.idfVersion[0] != '\0' ? info.app.idfVersion : "-");
			context.write(" build=");
			context.write(info.app.buildDate[0] != '\0' ? info.app.buildDate : "-");
			context.write(" ");
			context.writeLine(info.app.buildTime[0] != '\0' ? info.app.buildTime : "-");
		}
	}
#endif

} // namespace

lexxos::program::ProgramDescriptor BoardProgram::descriptor() {
	return {.name = "board", .help = "show active board descriptor", .handler = run, .legacy = false, .enabled = true};
}

int BoardProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	auto const& board = activeBoard(context);
	auto const debugUart = lexxos::platform::effectiveDebugUart(board);
	auto const debugI2c = lexxos::platform::effectiveDebugI2c(board);
	auto const systemI2c = board.interfaces.systemI2c;
	auto const sdSpi = lexxos::platform::effectiveSdSpi(board);
	auto const display = lexxos::platform::effectiveSpiTftDisplay(board);
	auto const& dsiDisplay = board.mipiDsiDisplay;

	// Board identity and processor.
	context.write("id: ");
	context.writeLine(board.boardId);
	context.write("name: ");
	context.writeLine(board.boardName);
	context.write("vendor: ");
	context.writeLine(board.vendor);
	context.write("processor: ");
	context.write(board.processorName);
	context.write(" / ");
	context.writeLine(lexxos::platform::toString(board.processorFamily));
	context.write("status-led: pin=");
	writePin(context, board.statusLed);
	context.write(" active=");
	context.writeLine(lexxos::platform::toString(board.statusLedActiveLevel));
	writeUserGpioSummary(context, board);

	// Effective console UART and board I2C buses.
	context.write("uart: index=");
	writeIndex(context, debugUart.index);
	context.write(" tx=");
	writePin(context, debugUart.tx);
	context.write(" rx=");
	writePin(context, debugUart.rx);
	context.write(" baud=");
	writeUnsigned(context, debugUart.baudRate);
	context.writeLine("");
	context.write("i2c/debug: index=");
	writeIndex(context, debugI2c.index);
	context.write(" sda=");
	writePin(context, debugI2c.sda);
	context.write(" scl=");
	writePin(context, debugI2c.scl);
	context.write(" baud=");
	writeUnsigned(context, debugI2c.baudRate);
	context.writeLine("");
	context.write("i2c/system: index=");
	writeIndex(context, systemI2c.index);
	context.write(" sda=");
	writePin(context, systemI2c.sda);
	context.write(" scl=");
	writePin(context, systemI2c.scl);
	context.write(" baud=");
	writeUnsigned(context, systemI2c.baudRate);
	context.writeLine("");

	// SD card SPI and SDIO wiring.
	context.write("sd/spi: index=");
	writeIndex(context, sdSpi.index);
	context.write(" sck=");
	writePin(context, sdSpi.sck);
	context.write(" mosi=");
	writePin(context, sdSpi.mosi);
	context.write(" miso=");
	writePin(context, sdSpi.miso);
	context.write(" cs=");
	writePin(context, sdSpi.cs);
	context.write(" cd=");
	writePin(context, board.sdCardDetect.detect);
	context.write(" cd-active=");
	context.write(lexxos::platform::toString(board.sdCardDetect.insertedLevel));
	context.write(" baud=");
	writeUnsigned(context, sdSpi.baudRate);
	context.writeLine("");
	context.write("sd/sdio: index=");
	writeIndex(context, board.sdSdio.index);
	context.write(" clk=");
	writePin(context, board.sdSdio.clock);
	context.write(" cmd=");
	writePin(context, board.sdSdio.command);
	context.write(" d0=");
	writePin(context, board.sdSdio.data0);
	context.write(" d1=");
	writePin(context, board.sdSdio.data1);
	context.write(" d2=");
	writePin(context, board.sdSdio.data2);
	context.write(" d3=");
	writePin(context, board.sdSdio.data3);
	context.write(" init-hz=");
	writeUnsigned(context, board.sdSdio.initializationClockHz);
	context.write(" xfer-hz=");
	writeUnsigned(context, board.sdSdio.transferClockHz);
	context.write(" pwr-ldo=");
	writeIndex(context, board.sdSdio.powerLdoChannel);
	context.writeLine("");
	context.write("display/spi: enabled=");
	context.write(display.enabled ? "yes" : "no");
	context.write(" index=");
	writeIndex(context, display.spiIndex);
	context.write(" sck=");
	writePin(context, display.sck);
	context.write(" mosi=");
	writePin(context, display.mosi);
	context.write(" cs=");
	writePin(context, display.cs);
	context.write(" dc=");
	writePin(context, display.dc);
	context.write(" reset=");
	writePin(context, display.reset);
	context.write(" baud=");
	writeUnsigned(context, display.spiBaudRate);
	context.writeLine("");
	context.write("display/dsi: enabled=");
	context.write(dsiDisplay.enabled ? "yes" : "no");
	context.write(" name=");
	context.write(dsiDisplay.name != nullptr ? dsiDisplay.name : "-");
	context.write(" panel=");
	context.write(lexxos::platform::toString(dsiDisplay.panelKind));
	context.write(" size=");
	writeUnsigned(context, static_cast<std::uint32_t>(dsiDisplay.width));
	context.write("x");
	writeUnsigned(context, static_cast<std::uint32_t>(dsiDisplay.height));
	context.write(" lanes=");
	writeUnsigned(context, dsiDisplay.dataLanes);
	context.write(" lane-mbps=");
	writeUnsigned(context, dsiDisplay.laneBitRateMbps);
	context.write(" dpi-khz=");
	writeUnsigned(context, dsiDisplay.dpiClockKhz);
	context.write(" fb=");
	context.write(lexxos::platform::toString(dsiDisplay.inputColor));
	context.write(" wire=");
	context.write(lexxos::platform::toString(dsiDisplay.outputColor));
	context.write(" order=");
	context.write(lexxos::platform::toString(dsiDisplay.rgbElementOrder));
	context.write(" ldo=");
	writeIndex(context, dsiDisplay.dsiPhyPowerLdoChannel);
	context.writeLine("");
	context.write("display/dsi-timing: hsw=");
	writeUnsigned(context, dsiDisplay.hsyncPulseWidth);
	context.write(" hbp=");
	writeUnsigned(context, dsiDisplay.hsyncBackPorch);
	context.write(" hfp=");
	writeUnsigned(context, dsiDisplay.hsyncFrontPorch);
	context.write(" vsw=");
	writeUnsigned(context, dsiDisplay.vsyncPulseWidth);
	context.write(" vbp=");
	writeUnsigned(context, dsiDisplay.vsyncBackPorch);
	context.write(" vfp=");
	writeUnsigned(context, dsiDisplay.vsyncFrontPorch);
	context.write(" pwr-i2c=");
	writeIndex(context, dsiDisplay.powerControlI2cIndex);
	context.write(" pwr-addr=");
	writeHex8(context, dsiDisplay.powerControlAddress);
	context.write(" pwm-reg=");
	writeHex8(context, dsiDisplay.backlightPwmRegister);
	context.write(" bridge=");
	context.writeLine(dsiDisplay.tc358762BridgeInit ? "tc358762" : "none");
	context.write("touch: enabled=");
	context.write(board.touchPanel.enabled ? "yes" : "no");
	context.write(" controller=");
	context.write(lexxos::platform::toString(board.touchPanel.controller));
	context.write(" name=");
	context.write(board.touchPanel.name != nullptr ? board.touchPanel.name : "-");
	context.write(" i2c=");
	writeIndex(context, board.touchPanel.i2cIndex);
	context.write(" addr=");
	writeHex8(context, board.touchPanel.address);
	context.write(" int=");
	writePin(context, board.touchPanel.interrupt);
	context.write(" reset=");
	writePin(context, board.touchPanel.reset);
	context.write(" raw=");
	writeUnsigned(context, board.touchPanel.rawWidth);
	context.write("x");
	writeUnsigned(context, board.touchPanel.rawHeight);
	context.write(" swap-xy=");
	context.write(board.touchPanel.swapXY ? "yes" : "no");
	context.write(" invert-x=");
	context.write(board.touchPanel.invertX ? "yes" : "no");
	context.write(" invert-y=");
	context.writeLine(board.touchPanel.invertY ? "yes" : "no");

	// External PSRAM wiring.
	context.write("psram: cs=");
	writePin(context, board.psram.spi.cs);
	context.write(" sck=");
	writePin(context, board.psram.spi.sck);
	context.write(" mosi=");
	writePin(context, board.psram.spi.mosi);
	context.write(" miso=");
	writePin(context, board.psram.spi.miso);
	context.write(" sio2=");
	writePin(context, board.psram.sio2);
	context.write(" sio3=");
	writePin(context, board.psram.sio3);
	context.write(" baud=");
	writeUnsigned(context, board.psram.spi.baudRate);
	context.writeLine("");
	context.write("audio/beep: enabled=");
	context.write(board.audioBeep.enabled ? "yes" : "no");
	context.write(" left=");
	writePin(context, board.audioBeep.left);
	context.write(" right=");
	writePin(context, board.audioBeep.right);
	context.write(" freq=");
	writeUnsigned(context, board.audioBeep.defaultFrequencyHz);
	context.write(" duration-ms=");
	writeUnsigned(context, board.audioBeep.defaultDurationMs);
	context.writeLine("");
	context.write("audio/codec: enabled=");
	context.write(board.audioCodec.enabled ? "yes" : "no");
	context.write(" codec=");
	context.write(lexxos::platform::toString(board.audioCodec.codec));
	context.write(" i2s=");
	writeIndex(context, board.audioCodec.i2sIndex);
	context.write(" mclk=");
	writePin(context, board.audioCodec.mclk);
	context.write(" bclk=");
	writePin(context, board.audioCodec.bclk);
	context.write(" ws=");
	writePin(context, board.audioCodec.wordSelect);
	context.write(" dout=");
	writePin(context, board.audioCodec.dataOut);
	context.write(" din=");
	writePin(context, board.audioCodec.dataIn);
	context.write(" amp=");
	writePin(context, board.audioCodec.amplifierEnable);
	context.write(" amp-level=");
	context.write(lexxos::platform::toString(board.audioCodec.amplifierEnableLevel));
	context.write(" addr=");
	writeHex8(context, board.audioCodec.i2cAddress);
	context.write(" sample-rate=");
	writeUnsigned(context, board.audioCodec.sampleRateHz);
	context.writeLine("");
	context.write("motion-sensor: enabled=");
	context.write(board.motionSensor.enabled ? "yes" : "no");
	context.write(" kind=");
	context.write(lexxos::platform::toString(board.motionSensor.sensor));
	context.write(" name=");
	context.write(board.motionSensor.name != nullptr ? board.motionSensor.name : "-");
	context.write(" addr=");
	writeHex8(context, board.motionSensor.i2cAddress);
	context.writeLine("");
	context.write("infrared: enabled=");
	context.write(board.infraredTransmitter.enabled ? "yes" : "no");
	context.write(" name=");
	context.write(board.infraredTransmitter.name != nullptr ? board.infraredTransmitter.name : "-");
	context.write(" pin=");
	writePin(context, board.infraredTransmitter.output);
	context.write(" carrier=");
	writeUnsigned(context, board.infraredTransmitter.carrierFrequencyHz);
	context.writeLine(" Hz");
	context.write("wireless: radio=");
	context.write(board.wireless.radio != nullptr ? board.wireless.radio : "-");
	context.write(" wifi=");
	context.write(board.wireless.wifi ? "yes" : "no");
	context.write(" bluetooth=");
	context.write(board.wireless.bluetooth ? "yes" : "no");
	context.write(" if=");
	context.write(board.wireless.interfaceName != nullptr ? board.wireless.interfaceName : "-");
	context.write(" backend=");
	context.write(board.wireless.backend != nullptr ? board.wireless.backend : "-");
	context.write(" autoconnect=");
	context.write(board.wireless.autoConnect ? "yes" : "no");
	context.write(" scan=");
	context.write(board.wireless.scan ? "yes" : "no");
	context.writeLine("");
	if (board.wireless.hostedSdio.isValid()) {
		context.write("wifi/hosted-sdio: index=");
		writeIndex(context, board.wireless.hostedSdio.index);
		context.write(" clk=");
		writePin(context, board.wireless.hostedSdio.clock);
		context.write(" cmd=");
		writePin(context, board.wireless.hostedSdio.command);
		context.write(" d0=");
		writePin(context, board.wireless.hostedSdio.data0);
		context.write(" d1=");
		writePin(context, board.wireless.hostedSdio.data1);
		context.write(" d2=");
		writePin(context, board.wireless.hostedSdio.data2);
		context.write(" d3=");
		writePin(context, board.wireless.hostedSdio.data3);
		context.write(" reset=");
		writePin(context, board.wireless.hostedReset);
		context.write(" init-hz=");
		writeUnsigned(context, board.wireless.hostedSdio.initializationClockHz);
		context.write(" xfer-hz=");
		writeUnsigned(context, board.wireless.hostedSdio.transferClockHz);
		context.writeLine("");
	}
#if defined(LEXXOS_PLATFORM_ESPIDF) && defined(LEXXOS_ESPIDF_ENABLE_HOSTED)
	writeHostedSummary(context, board);
#endif
	context.write("ethernet: enabled=");
	context.write(board.ethernet.enabled ? "yes" : "no");
	context.write(" phy=");
	context.write(lexxos::platform::toString(board.ethernet.phy));
	context.write(" name=");
	context.write(board.ethernet.name != nullptr ? board.ethernet.name : "-");
	context.write(" addr=");
	writeUnsigned(context, board.ethernet.phyAddress);
	context.write(" mdc=");
	writePin(context, board.ethernet.mdc);
	context.write(" mdio=");
	writePin(context, board.ethernet.mdio);
	context.write(" rst=");
	writePin(context, board.ethernet.reset);
	context.write(" clk=");
	writePin(context, board.ethernet.clock);
	context.write(" txd0=");
	writePin(context, board.ethernet.txd0);
	context.write(" txd1=");
	writePin(context, board.ethernet.txd1);
	context.write(" txen=");
	writePin(context, board.ethernet.txEnable);
	context.write(" rxd0=");
	writePin(context, board.ethernet.rxd0);
	context.write(" rxd1=");
	writePin(context, board.ethernet.rxd1);
	context.write(" crs-dv=");
	writePin(context, board.ethernet.crsDv);
	context.writeLine("");
	context.write("rtc/software: enabled=");
	context.write(board.softwareRtc.enabled ? "yes" : "no");
	context.write(" default=");
	context.write(board.softwareRtc.preferAsDefault ? "yes" : "no");
	context.write(" auto-sync=");
	context.write(board.softwareRtc.autoNetworkSync ? "yes" : "no");
	context.write(" server=");
	context.write(board.softwareRtc.networkSyncServer != nullptr ? board.softwareRtc.networkSyncServer : "-");
	context.write(" timeout-ms=");
	writeUnsigned(context, board.softwareRtc.networkSyncTimeoutMs);
	context.write(" interval-ms=");
	writeUnsigned(context, board.softwareRtc.networkSyncIntervalMs);
	context.write(" retry-ms=");
	writeUnsigned(context, board.softwareRtc.networkSyncRetryIntervalMs);
	context.writeLine("");
	writeRuntimeCapabilities(context);
	return 0;
}

} // namespace lexxos::apps::system_programs
