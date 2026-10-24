#include <lexxos/app/console_mirror.h>
#include <lexxos/app/program_input.h>
#include <lexxos/apps/gameboy/detail/gameboy_pcm_output_guard.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_arguments.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_support.h>
#include <lexxos/apps/gameboy/detail/gameboy_settings.h>
#include <lexxos/devices/audio_device.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/kernel/system.h>

namespace lexxos::apps::gameboy_programs {

namespace {

	inline constexpr std::size_t gbAudioPcmChunkFrames = 128;

	struct ExitComboInputState {
		bool start{false};
		bool select{false};
	};

	bool flushGameBoyAudioFrames(
	    lexxos::os::AppContext& context,
	    lexxos::devices::AudioDevice& audio,
	    lexxos::gameboy::Bus& bus) {
		std::array<lexxos::devices::AudioPcmFrame, gbAudioPcmChunkFrames> frames{};
		while (bus.audioFrameCount() != 0) {
			auto const frameCount = bus.readAudioFrames(frames.data(), frames.size());
			if (frameCount == 0) {
				break;
			}
			if (!audio.playPcmBuffer(lexxos::devices::AudioPcmBufferRequest{
			        .channel = lexxos::devices::AudioChannel::Stereo,
			        .frames = frames.data(),
			        .frameCount = frameCount,
			        .sampleRateHz = lexxos::gameboy::Apu::sampleRateHz,
			    })) {
				lexxos::app::console_mirror::setMode(lexxos::app::ConsoleMirrorMode::Text);
				context.writeErrorLine("gbemu: PCM audio output failed");
				return false;
			}
		}
		return true;
	}

	bool isExitComboStartKey(lexxos::devices::KeyCode key) {
		return key == lexxos::devices::KeyCode::Tab;
	}

	bool isExitComboSelectKey(lexxos::devices::KeyCode key) {
		return key == lexxos::devices::KeyCode::Backspace || key == lexxos::devices::KeyCode::Enter;
	}

	void updateExitComboInputState(
	    lexxos::app::program_input::EventCursor& cursor,
	    ExitComboInputState& state) {
		lexxos::devices::InputEvent event{};
		while (lexxos::app::program_input::pollEvent(cursor, event)) {
			if (event.type != lexxos::devices::InputEventType::KeyDown
			    && event.type != lexxos::devices::InputEventType::KeyUp) {
				continue;
			}

			auto const pressed = event.type == lexxos::devices::InputEventType::KeyDown;
			if (isExitComboStartKey(event.key)) {
				state.start = pressed;
			}
			if (isExitComboSelectKey(event.key)) {
				state.select = pressed;
			}
		}
	}

	bool exitComboPressed(lexxos::devices::GamepadState const& gamepad, ExitComboInputState const& input) {
		return (gamepad.start || input.start) && (gamepad.select || input.select);
	}

	void writeSettingsWarning(lexxos::os::AppContext& context, detail::GbEmuSettingsLoadResult const& settings) {
		switch (settings.status) {
			case detail::GbEmuSettingsStatus::Loaded:
			case detail::GbEmuSettingsStatus::StorageUnavailable:
				return;
			case detail::GbEmuSettingsStatus::CreatedDefault:
				context.write("gbemu: created default settings ");
				context.writeLine(detail::gbEmuSettingsPath);
				return;
			case detail::GbEmuSettingsStatus::InvalidFile:
				context.write("gbemu: warning: invalid settings file, using defaults: ");
				context.writeLine(detail::gbEmuSettingsPath);
				return;
			case detail::GbEmuSettingsStatus::ReadFailed:
				context.write("gbemu: warning: cannot read settings, using defaults: ");
				context.write(detail::gbEmuSettingsPath);
				context.write(" open=");
				context.write(lexxos::vfs::toString(settings.openStatus));
				context.write(" read=");
				context.writeLine(lexxos::vfs::toString(settings.readStatus));
				return;
			case detail::GbEmuSettingsStatus::WriteFailed:
				context.write("gbemu: warning: cannot create default settings: ");
				context.write(detail::gbEmuSettingsPath);
				context.write(": ");
				context.writeLine(lexxos::vfs::toString(settings.writeStatus));
				return;
			case detail::GbEmuSettingsStatus::SyncFailed:
				context.write("gbemu: warning: cannot sync default settings: ");
				context.write(detail::gbEmuSettingsPath);
				context.write(": ");
				context.writeLine(lexxos::vfs::toString(settings.syncStatus));
				return;
		}
	}

} // namespace

int runGameBoyEmulator(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments) {
	lexxos::app::console_mirror::setMode(lexxos::app::ConsoleMirrorMode::Text);

	std::array<char, lexxos::vfs::maxPathLength> path{};
	if (!resolvePath(context, arguments.positional("rom"), path)) {
		return 1;
	}

	auto* display = context.devices().primaryDisplayDevice();
	if (display == nullptr) {
		context.writeErrorLine("gbemu: display device is not registered");
		return 1;
	}
	if (!display->isReady()) {
		context.writeErrorLine("gbemu: display device is not ready");
		return 1;
	}
	if (display->nativeFormat() != lexxos::devices::PixelFormat::Rgb565) {
		context.writeErrorLine("gbemu: only rgb565 displays are supported by the skeleton renderer");
		return 1;
	}

	auto* gamepad = context.devices().primaryGamepadDevice();
	if (gamepad == nullptr) {
		context.writeErrorLine("gbemu: gamepad device is not registered");
		return 1;
	}
	if (!gamepad->isReady()) {
		context.writeErrorLine("gbemu: gamepad device is not ready");
		return 1;
	}

	lexxos::devices::AudioDevice* audio = nullptr;
#if defined(LEXXOS_PLATFORM_ESPIDF)
	constexpr bool defaultAudioRequested = false;
#else
	constexpr bool defaultAudioRequested = true;
#endif
	auto const audioRequested = !arguments.has("no-audio") && (arguments.has("audio") || defaultAudioRequested);
	if (audioRequested) {
		audio = context.devices().primaryAudioDevice(context.platform());
		if (audio == nullptr || !audio->isReady() || !audio->supportsPcmBuffer() || !audio->supportsChannel(lexxos::devices::AudioChannel::Stereo)) {
			context.writeLine("gbemu: audio disabled: PCM stereo output is unavailable");
			audio = nullptr;
		}
	} else {
		context.writeLine(arguments.has("no-audio") ? "gbemu: audio disabled by --no-audio" : "gbemu: audio disabled by target default");
	}
	detail::GameBoyPcmOutputGuard audioOutputGuard{audio};
	if (audio != nullptr) {
		audio->resetPcmOutputStats();
	}

	lexxos::devices::DeviceResourceLock displayLock;
	lexxos::devices::DeviceResourceLock gamepadLock;
	auto releaseInteractiveLocks = [&]() {
		displayLock.release();
		gamepadLock.release();
	};
	auto waitForInteractiveFocus = [&]() -> bool {
		while (context.processId() != 0 && !context.hasProgramFocus()) {
			releaseInteractiveLocks();
			if (lexxos::kernel::isShutdownRequested()) {
				return false;
			}
			sleepMs(context, 10);
		}
		return !lexxos::kernel::isShutdownRequested();
	};
	auto ensureInteractiveLocks = [&]() -> bool {
		if (!waitForInteractiveFocus()) {
			return false;
		}
		if (!displayLock) {
			displayLock = context.devices().lockDisplayDevice(
			    display->name(),
			    lexxos::devices::DeviceResourcePriority::Interactive,
			    "gbemu");
			if (!displayLock) {
				return false;
			}
		}
		if (!gamepadLock) {
			gamepadLock = context.devices().lockGamepadDevice(
			    gamepad->name(),
			    lexxos::devices::DeviceResourcePriority::Interactive,
			    "gbemu");
			if (!gamepadLock) {
				displayLock.release();
				return false;
			}
		}
		return true;
	};
	auto waitForInteractiveLocks = [&]() -> bool {
		while (!lexxos::kernel::isShutdownRequested()) {
			if (ensureInteractiveLocks()) {
				return true;
			}
			if (context.processId() != 0 && !context.hasProgramFocus()) {
				continue;
			}
			if (context.pollCancellationRequest()) {
				return false;
			}
			sleepMs(context, 5);
		}
		return false;
	};

	auto const settingsResult = detail::loadOrCreateGbEmuSettings();
	writeSettingsWarning(context, settingsResult);
	auto const& settings = settingsResult.settings;

	std::uint32_t frames = settings.frames;
	if (arguments.has("frames")) {
		if (!parseUnsigned(arguments.valueOr("frames", "1"), frames)) {
			context.writeErrorLine("gbemu: invalid frame count");
			return 2;
		}
	}
	if (frames != 0 && frames > maxGbEmuFrames) {
		frames = maxGbEmuFrames;
	}

	std::uint32_t stepsPerFrame = settings.stepsPerFrame;
	if (arguments.has("steps-per-frame")) {
		if (!parseUnsigned(arguments.valueOr("steps-per-frame", "512"), stepsPerFrame) || stepsPerFrame == 0) {
			context.writeErrorLine("gbemu: invalid steps-per-frame count");
			return 2;
		}
	}
	if (stepsPerFrame > maxGbEmuStepsPerFrame) {
		stepsPerFrame = maxGbEmuStepsPerFrame;
	}

	std::uint32_t cyclesPerFrame = settings.cyclesPerFrame;
	if (arguments.has("cycles-per-frame")) {
		if (!parseUnsigned(arguments.valueOr("cycles-per-frame", "0"), cyclesPerFrame)) {
			context.writeErrorLine("gbemu: invalid cycles-per-frame count");
			return 2;
		}
	}
	if (cyclesPerFrame > maxGbCyclesPerFrame) {
		cyclesPerFrame = maxGbCyclesPerFrame;
	}

	auto const presetFromCli = arguments.has("preset");
	auto const speedMode = settings.speedModeName();
	auto const cliPlayableFast = arguments.has("playable-fast");
	auto const cliMaxSpeed = arguments.has("max-speed");
	auto const cliPsramFast = arguments.has("psram-fast");
	auto const cliSpeedShortcut = cliPlayableFast || cliMaxSpeed || cliPsramFast;
	auto const useSettingsSpeedMode = !presetFromCli && !cliSpeedShortcut;
	auto const settingsPlayablePreset = useSettingsSpeedMode && speedMode == "playable";
	auto const presetText = presetFromCli ? arguments.valueOr("preset", "") : settingsPlayablePreset ? std::string_view{"playable"}
	                                                                                                 : std::string_view{"none"};
	auto const preset = presetText == "none" ? std::string_view{} : presetText;
	auto const playablePreset = preset == "playable";
	if (!preset.empty() && !playablePreset) {
		context.writeErrorLine("gbemu: invalid preset, expected none or playable");
		return 2;
	}

	auto const playableFast = cliPlayableFast || (useSettingsSpeedMode && speedMode == "playable-fast");
	auto const maxSpeed = cliMaxSpeed || (useSettingsSpeedMode && speedMode == "max-speed");
	auto const psramFast = cliPsramFast || (useSettingsSpeedMode && speedMode == "psram-fast");
	if (maxSpeed && psramFast) {
		context.writeErrorLine("gbemu: use either --max-speed or --psram-fast, not both");
		return 2;
	}

	auto const performanceShortcut = playableFast || playablePreset || maxSpeed || psramFast;
	auto const realFrame = arguments.has("real-frame") || performanceShortcut;
	if (cyclesPerFrame == 0 && realFrame) {
		cyclesPerFrame = defaultGbCyclesPerFrame;
	}

	auto const warmupMode = settings.warmupModeName();
	auto const cliWarmupMode = arguments.has("until-vram") || arguments.has("until-visible") || arguments.has("until-tile-data") || arguments.has("until-bg-ready");
	auto const untilVram = arguments.has("until-vram") || (!cliWarmupMode && warmupMode == "vram");
	auto const untilVisible = arguments.has("until-visible") || (!cliWarmupMode && warmupMode == "visible");
	auto const untilTileData = arguments.has("until-tile-data") || (!cliWarmupMode && warmupMode == "tile-data");
	auto const untilBgReady = arguments.has("until-bg-ready") || (!cliWarmupMode && warmupMode == "bg-ready");
	auto const forceVisible = arguments.has("force-visible");
	lexxos::gameboy::BackgroundMapMode bgMapMode = lexxos::gameboy::BackgroundMapMode::Auto;
	if (!parseBackgroundMapMode(arguments.valueOr("bg-map", "auto"), bgMapMode)) {
		context.writeErrorLine("gbemu: invalid bg-map, expected auto, 9800 or 9c00");
		return 2;
	}
	lexxos::gameboy::BackgroundTileDataMode bgTileMode = lexxos::gameboy::BackgroundTileDataMode::Auto;
	if (!parseBackgroundTileDataMode(arguments.valueOr("bg-tiles", "auto"), bgTileMode)) {
		context.writeErrorLine("gbemu: invalid bg-tiles, expected auto, 8000 or 8800");
		return 2;
	}
	std::uint32_t minTileData = settings.minTileData;
	if (arguments.has("min-tile-data")) {
		if (!parseUnsigned(arguments.valueOr("min-tile-data", "256"), minTileData)) {
			context.writeErrorLine("gbemu: invalid min-tile-data count");
			return 2;
		}
	}
	std::uint32_t maxWarmupCycles = settings.maxWarmupCycles;
	if (arguments.has("max-cycles")) {
		if (!parseUnsigned(arguments.valueOr("max-cycles", "8988672"), maxWarmupCycles)) {
			context.writeErrorLine("gbemu: invalid max-cycles count");
			return 2;
		}
	}

	if (maxWarmupCycles > maxGbEmuWarmupCycles) {
		maxWarmupCycles = maxGbEmuWarmupCycles;
	}

	auto const displayModeText = arguments.has("display") ? arguments.valueOr("display", "crop") : settings.displayName();
	lexxos::gameboy::DisplayBlitMode displayMode = lexxos::gameboy::DisplayBlitMode::Crop;
	if (!parseDisplayBlitMode(displayModeText, displayMode)) {
		context.writeErrorLine("gbemu: invalid display mode, expected crop, fit or none");
		return 2;
	}

	auto const paletteName = arguments.has("palette") ? arguments.valueOr("palette", "default") : settings.paletteName();
	lexxos::gameboy::Rgb565Palette displayPalette{};
	if (!parseRgb565Palette(paletteName, displayPalette)) {
		context.writeError("gbemu: invalid palette, expected ");
		context.writeErrorLine(rgb565PaletteNames());
		return 2;
	}

	std::uint32_t renderEvery = settings.renderEvery;
	if (arguments.has("render-every")) {
		if (!parseUnsigned(arguments.valueOr("render-every", "1"), renderEvery) || renderEvery == 0) {
			context.writeErrorLine("gbemu: invalid render-every count");
			return 2;
		}
	}

	if (performanceShortcut && !arguments.has("render-every") && (presetFromCli || playableFast || maxSpeed || psramFast)) {
		renderEvery = 4;
	}

	if (frames != 0 && renderEvery > frames) {
		renderEvery = frames;
	}

	std::uint32_t inputEvery = settings.inputEvery;
	if (arguments.has("input-every")) {
		if (!parseUnsigned(arguments.valueOr("input-every", "1"), inputEvery) || inputEvery == 0) {
			context.writeErrorLine("gbemu: invalid input-every count");
			return 2;
		}
	}

	if (performanceShortcut && !arguments.has("input-every") && (presetFromCli || playableFast || maxSpeed || psramFast)) {
		inputEvery = playablePreset ? 2 : 1;
	}

	if (frames != 0 && inputEvery > frames) {
		inputEvery = frames;
	}

	auto const explicitRomFlash = arguments.has("rom-flash") || arguments.has("rom-flash-reuse");
	auto const explicitRomPsram = arguments.has("rom-psram") || arguments.has("rom-psram-reuse");
	auto const romStorage = settings.romStorageName();
#if defined(LEXXOS_KERNEL_ESPIDF)
	auto const fileCacheSettingAllowsMappedFlashDefault = romStorage == "file-cache" && playablePreset;
#else
	auto const fileCacheSettingAllowsMappedFlashDefault = false;
#endif
	auto const settingsStorageBlocksShortcuts = romStorage != "auto" && !fileCacheSettingAllowsMappedFlashDefault;
	auto const settingsRomFlash = romStorage == "flash" || romStorage == "flash-reuse";
	auto const settingsRomFlashReuse = romStorage == "flash-reuse" || (romStorage == "auto" && settings.romFlashReuse);
	auto const settingsRomPsram = romStorage == "psram" || romStorage == "psram-reuse";
	auto const settingsRomPsramReuse = romStorage == "psram-reuse";
#if defined(LEXXOS_KERNEL_ESPIDF)
	auto const defaultRomFlashReuse = playablePreset;
#else
	auto const defaultRomFlashReuse = false;
#endif
	auto const implicitRomFlashReuse =
	    !settingsStorageBlocksShortcuts
	    && (maxSpeed || (presetFromCli && playablePreset) || defaultRomFlashReuse || settingsRomFlashReuse)
	    && !explicitRomFlash
	    && !explicitRomPsram;
	auto const implicitRomPsramReuse = !settingsStorageBlocksShortcuts && psramFast && !explicitRomFlash && !explicitRomPsram;
	auto const romFlash = explicitRomFlash || (!explicitRomPsram && (settingsRomFlash || implicitRomFlashReuse));
	auto const romFlashReuse = arguments.has("rom-flash-reuse") || (!explicitRomFlash && settingsRomFlashReuse) || implicitRomFlashReuse;
	auto const romPsram = explicitRomPsram || (!explicitRomFlash && (settingsRomPsram || implicitRomPsramReuse));
	auto const romPsramReuse = arguments.has("rom-psram-reuse") || (!explicitRomPsram && settingsRomPsramReuse) || implicitRomPsramReuse;
	auto const noPpu = arguments.has("no-ppu") || settings.noPpu;
	auto const fastTiming = arguments.has("fast-timing") || settings.fastTiming || playablePreset;
	if (romFlash && romPsram) {
		context.writeErrorLine("gbemu: use either --rom-flash/--rom-flash-reuse or --rom-psram/--rom-psram-reuse, not both");
		return 2;
	}
	auto const fastHalt = settings.fastHalt && !arguments.has("no-fast-halt");
	auto const syncRealtime = arguments.has("sync-realtime") || settings.syncRealtime || (presetFromCli && playablePreset);
	if (syncRealtime && cyclesPerFrame == 0) {
		context.writeErrorLine("gbemu: --sync-realtime requires --real-frame or --cycles-per-frame");
		return 2;
	}

	std::uint32_t breakPcValue = 0;
	std::uint16_t breakPc = 0;
	std::uint16_t const* breakPcPtr = nullptr;
	auto const breakPcText = arguments.valueOr("break-pc", "");
	if (!breakPcText.empty()) {
		if (!parseNumber(breakPcText, breakPcValue) || breakPcValue > 0xFFFFU) {
			context.writeErrorLine("gbemu: invalid break-pc address");
			return 2;
		}
		breakPc = static_cast<std::uint16_t>(breakPcValue);
		breakPcPtr = &breakPc;
	}

	std::uint32_t breakPcFromValue = 0;
	std::uint16_t breakPcFrom = 0;
	std::uint16_t const* breakPcFromPtr = nullptr;
	auto const breakPcFromText = arguments.valueOr("break-pc-from", "");
	if (!breakPcFromText.empty()) {
		if (!parseNumber(breakPcFromText, breakPcFromValue) || breakPcFromValue > 0xFFFFU) {
			context.writeErrorLine("gbemu: invalid break-pc-from address");
			return 2;
		}
		breakPcFrom = static_cast<std::uint16_t>(breakPcFromValue);
		breakPcFromPtr = &breakPcFrom;
	}

	auto const trace = arguments.has("trace");
	auto const profile = arguments.has("profile") || settings.debugProfile;
	auto const faultTrace = arguments.has("fault-trace") || settings.faultTrace;
	auto const watchBlank = arguments.has("watch-blank");

	std::uint32_t watchBlankAfter = 0;
	if (!parseUnsigned(arguments.valueOr("watch-blank-after", "0"), watchBlankAfter)) {
		context.writeErrorLine("gbemu: invalid watch-blank-after count");
		return 2;
	}

	std::uint32_t watchBlankStreakTarget = 1;
	if (!parseUnsigned(arguments.valueOr("watch-blank-streak", "1"), watchBlankStreakTarget) || watchBlankStreakTarget == 0) {
		context.writeErrorLine("gbemu: invalid watch-blank-streak count");
		return 2;
	}

	std::uint32_t breakBgpZeroAfter = 0;
	auto const breakBgpZero = arguments.has("break-bgp-zero-after");
	if (!parseUnsigned(arguments.valueOr("break-bgp-zero-after", "0"), breakBgpZeroAfter)) {
		context.writeErrorLine("gbemu: invalid break-bgp-zero-after count");
		return 2;
	}

	auto const debugTrace = trace || faultTrace || watchBlank || breakPcPtr != nullptr || breakPcFromPtr != nullptr || breakBgpZero;
	auto const verboseRunSteps = trace || profile || debugTrace;

	std::uint32_t profileEvery = settings.profileEvery;
	if (arguments.has("profile-every")) {
		if (!parseUnsigned(arguments.valueOr("profile-every", "60"), profileEvery) || profileEvery == 0) {
			context.writeErrorLine("gbemu: invalid profile-every count");
			return 2;
		}
	}
	if (frames != 0 && profileEvery > frames) {
		profileEvery = frames;
	}

	auto* workspace = gameBoyWorkspace(context);
	if (workspace == nullptr) {
		return 1;
	}

	auto& cartridge = workspace->cartridge;
	if (!openCartridge(context, "gbemu", path.data(), cartridge)) {
		return 1;
	}
	CartridgeLifetimeGuard cartridgeGuard{cartridge};

	if (!cartridge.mapperSupported()) {
		context.writeErrorLine("gbemu: cartridge mapper is not supported yet");
		return 2;
	}

	if (romPsram) {
		lexxos::gameboy::CartridgePsramLoadResult psramLoad{};
		std::uint32_t psramMs = 0;
		if (romPsramReuse) {
			psramLoad = cartridge.reusePlatformPsramRomIfMatching();
			if (psramLoad.ok) {
				context.write("gbemu: reused PicoCalc PSRAM ROM bytes=");
				writeUnsigned(context, psramLoad.bytesLoaded);
				context.write(" capacity=");
				writeUnsigned(context, psramLoad.capacity);
				context.write(" transfer=");
				writeUnsigned(context, psramLoad.transferBytes);
				context.writeLine("");
			}
		}
		if (!psramLoad.ok) {
			context.writeLine("gbemu: copying ROM to PicoCalc PSRAM staging area...");
			auto const psramStartMs = uptimeMs(context);
			psramLoad = cartridge.loadRomToPlatformPsram();
			psramMs = elapsedMs(psramStartMs, uptimeMs(context));
		}
		if (!psramLoad.ok) {
			if (psramLoad.platformUnavailable) {
				context.writeErrorLine("gbemu: ROM PSRAM staging is not available on this platform/build");
			} else if (psramLoad.tooLarge) {
				context.writeErrorLine("gbemu: ROM does not fit into PicoCalc PSRAM");
			} else if (psramLoad.seekFailed) {
				context.writeErrorLine("gbemu: failed to seek ROM before PSRAM staging");
			} else if (psramLoad.shortRead) {
				context.writeErrorLine("gbemu: short ROM read during PSRAM staging");
			} else if (psramLoad.writeFailed) {
				context.writeErrorLine("gbemu: failed to write ROM into PicoCalc PSRAM");
			} else if (psramLoad.readFailed) {
				context.writeErrorLine("gbemu: failed to read PicoCalc PSRAM staging area");
			} else if (psramLoad.verifyFailed) {
				context.writeError("gbemu: PicoCalc PSRAM ROM verify failed offset=");
				writeUnsigned(context, psramLoad.verifyFailedOffset);
				context.writeError(" expected=");
				writeHexByte(context, psramLoad.verifyExpected);
				context.writeError(" actual=");
				writeHexByte(context, psramLoad.verifyActual);
				context.writeErrorLine("");
			} else if (psramLoad.noMatchingImage) {
				context.writeErrorLine("gbemu: PicoCalc PSRAM staging area does not contain a matching image");
			} else {
				context.writeErrorLine("gbemu: failed to read ROM during PSRAM staging");
			}
			return 1;
		}
		if (!romPsramReuse || psramMs != 0) {
			context.write("gbemu: ROM PSRAM staging bytes=");
			writeUnsigned(context, psramLoad.bytesLoaded);
			context.write(" capacity=");
			writeUnsigned(context, psramLoad.capacity);
			context.write(" transfer=");
			writeUnsigned(context, psramLoad.transferBytes);
			context.write(" ms=");
			writeUnsigned(context, psramMs);
			context.writeLine("");
		}
	}

	if (romFlash) {
		lexxos::gameboy::CartridgeFlashLoadResult flashLoad{};
		std::uint32_t flashMs = 0;
		if (romFlashReuse) {
			flashLoad = cartridge.reusePlatformFlashRomIfMatching();
			if (flashLoad.ok) {
				context.write("gbemu: reused mapped flash ROM bytes=");
				writeUnsigned(context, flashLoad.bytesLoaded);
				context.write(" offset=");
				writeHex32(context, flashLoad.flashOffset);
				context.write(" capacity=");
				writeUnsigned(context, flashLoad.flashCapacity);
				context.writeLine("");
			}
		}
		if (!flashLoad.ok) {
			context.writeLine("gbemu: copying ROM to mapped flash staging area...");
			auto const flashStartMs = uptimeMs(context);
			flashLoad = cartridge.loadRomToPlatformFlash();
			flashMs = elapsedMs(flashStartMs, uptimeMs(context));
		}
		if (!flashLoad.ok) {
			if (flashLoad.platformUnavailable) {
				context.writeErrorLine("gbemu: ROM flash staging is not available on this platform/build");
			} else if (flashLoad.tooLarge) {
				context.writeErrorLine("gbemu: ROM does not fit into the configured flash staging area");
			} else if (flashLoad.seekFailed) {
				context.writeErrorLine("gbemu: failed to seek ROM before flash staging");
			} else if (flashLoad.shortRead) {
				context.writeErrorLine("gbemu: short ROM read during flash staging");
			} else if (flashLoad.programFailed) {
				context.writeErrorLine("gbemu: failed to erase/program ROM flash staging area");
			} else if (flashLoad.noMatchingImage) {
				context.writeErrorLine("gbemu: ROM flash staging area does not contain a matching image");
			} else {
				context.writeErrorLine("gbemu: failed to read ROM during flash staging");
			}
			return 1;
		}
		if (!romFlashReuse || flashMs != 0) {
			context.write("gbemu: ROM flash staging bytes=");
			writeUnsigned(context, flashLoad.bytesLoaded);
			context.write(" offset=");
			writeHex32(context, flashLoad.flashOffset);
			context.write(" capacity=");
			writeUnsigned(context, flashLoad.flashCapacity);
			context.write(" ms=");
			writeUnsigned(context, flashMs);
			context.writeLine("");
		}
	}

	auto& bus = workspace->bus;
	auto& cpu = workspace->cpu;
	auto& framebuffer = workspace->framebuffer;
	auto& ppu = workspace->ppu;

	bus.reset();
	cartridgeGuard.attachBus(bus);
	bus.setAudioOutputEnabled(audio != nullptr);
	cpu.resetToPostBootState();

	auto const& info = cartridge.headerInfo();

	context.write("gbemu: loaded ");
	context.write(info.title.data()[0] != '\0' ? info.title.data() : "<empty>");
	context.write(" from ");
	context.writeLine(path.data());
	context.write("gbemu: display ");
	writeUnsigned(context, static_cast<std::uint32_t>(display->width()));
	context.write("x");
	writeUnsigned(context, static_cast<std::uint32_t>(display->height()));
	context.write(" ");
	context.writeLine(lexxos::devices::toString(display->nativeFormat()));

	context.write("gbemu: mapper ");
	context.write(lexxos::gameboy::mapperName(cartridge.mapper()));
	context.write(" frames=");
	if (frames == 0) {
		context.write("unlimited");
	} else {
		writeUnsigned(context, frames);
	}
	context.write(" steps/frame=");
	writeUnsigned(context, stepsPerFrame);
	context.write(" cycles/frame=");
	if (cyclesPerFrame == 0) {
		context.write("off");
	} else {
		writeUnsigned(context, cyclesPerFrame);
	}
	context.write(" until-vram=");
	context.write(untilVram ? "1" : "0");
	context.write(" until-visible=");
	context.write(untilVisible ? "1" : "0");
	context.write(" until-tile-data=");
	context.write(untilTileData ? "1" : "0");
	context.write(" until-bg-ready=");
	context.write(untilBgReady ? "1" : "0");
	context.write(" min-tile-data=");
	writeUnsigned(context, minTileData);
	context.write(" force-visible=");
	context.write(forceVisible ? "1" : "0");
	context.write(" bg-map=");
	context.write(toString(bgMapMode));
	context.write(" bg-tiles=");
	context.write(toString(bgTileMode));
	context.write(" display=");
	context.write(toString(displayMode));
	context.write(" palette=");
	context.write(paletteName.empty() ? "default" : paletteName);
	context.write(" render-every=");
	writeUnsigned(context, renderEvery);
	context.write(" input-every=");
	writeUnsigned(context, inputEvery);
	context.write(" exit-hold-ms=");
	writeUnsigned(context, settings.exitHoldMs);
	if (playablePreset) {
		context.write(" preset=playable");
	}
	context.write(" playable-fast=");
	context.write(playableFast ? "1" : "0");
	context.write(" max-speed=");
	context.write(maxSpeed ? "1" : "0");
	context.write(" psram-fast=");
	context.write(psramFast ? "1" : "0");
	context.write(" rom-storage=");
	if (cartridge.usingPlatformFlashRom()) {
		context.write("flash");
	} else if (cartridge.usingPlatformPsramRom()) {
		context.write("psram-cache");
	} else {
		context.write("file-cache");
	}
	context.write(" no-ppu=");
	context.write(noPpu ? "1" : "0");
	context.write(" fast-timing=");
	context.write(fastTiming ? "1" : "0");
	context.write(" sync-realtime=");
	context.write(syncRealtime ? "1" : "0");
	context.write(" audio=");
	context.write(audio != nullptr ? "1" : "0");
	context.write(" profile=");
	context.write(profile ? "1" : "0");
	if (profile) {
		context.write(" profile-every=");
		writeUnsigned(context, profileEvery);
	}
	context.write(" fast-halt=");
	context.write(fastHalt ? "1" : "0");
	context.write(" rom-cache=");
	writeUnsigned(context, lexxos::gameboy::Cartridge::cachePageBytes() / 1024U);
	context.write("Kx");
	writeUnsigned(context, lexxos::gameboy::Cartridge::cacheSlots());
	context.write("=");
	writeUnsigned(context, lexxos::gameboy::Cartridge::cacheCapacityBytes() / 1024U);
	context.write("K");
	if (breakPcPtr != nullptr) {
		context.write(" break-pc=");
		writeHex16(context, breakPc);
	}
	if (breakPcFromPtr != nullptr) {
		context.write(" break-pc-from=");
		writeHex16(context, breakPcFrom);
	}
	if (watchBlank) {
		context.write(" watch-blank-after=");
		writeUnsigned(context, watchBlankAfter);
		context.write(" watch-blank-streak=");
		writeUnsigned(context, watchBlankStreakTarget);
	}
	if (breakBgpZero) {
		context.write(" break-bgp-zero-after=");
		writeUnsigned(context, breakBgpZeroAfter);
	}
	context.writeLine("");

	lexxos::devices::GamepadState lastState{};
	lexxos::app::program_input::EventCursor exitComboInputCursor{};
	lexxos::app::program_input::reset(exitComboInputCursor);
	ExitComboInputState exitComboInput{};
	lexxos::gameboy::DisplayRenderInfo renderInfo = lexxos::gameboy::Ppu::layoutForDisplay(*display, displayMode);
	lexxos::gameboy::CpuStepStatus lastStatus = lexxos::gameboy::CpuStepStatus::Ok;
	std::uint32_t totalSteps = 0;
	std::uint32_t totalCycles = 0;
	std::uint32_t renderedFrames = 0;
	std::uint32_t totalWallMs = 0;
	std::uint32_t totalInputMs = 0;
	std::uint32_t totalCpuMs = 0;
	std::uint32_t totalPpuMs = 0;
	std::uint32_t totalBlitMs = 0;
	GbEmuProfileWindow profileWindow{};
	GbEmuProfileWindow profileTotal{};
	std::uint32_t previousCacheFills = cartridge.cacheFillCount();
	std::uint32_t previousCacheRetries = cartridge.cacheRetryCount();
	std::uint32_t previousCacheFailures = cartridge.cacheFailureCount();
	std::uint64_t previousCacheReadBytes = cartridge.cacheReadBytes();
	std::uint32_t previousCacheFillMs = cartridge.cacheFillMs();
	bool emuBreakpointHit = false;
	RunTraceBuffer emuTrace{};

	if (!noPpu && displayMode != lexxos::gameboy::DisplayBlitMode::None) {
		if (!waitForInteractiveLocks()) {
			bus.attachCartridge(nullptr);
			context.writeLine(context.cancellationRequested() ? "gbemu: canceled" : "gbemu: stopped");
			return context.cancellationRequested() ? 130 : 1;
		}
		lexxos::app::console_mirror::flush();
		lexxos::app::console_mirror::setMode(lexxos::app::ConsoleMirrorMode::Graphics);
		if (!display->clear(gbDisplayBackgroundColor)) {
			bus.attachCartridge(nullptr);
			lexxos::app::console_mirror::setMode(lexxos::app::ConsoleMirrorMode::Text);
			context.writeErrorLine("gbemu: failed to clear display background");
			return 1;
		}
	}

	if ((untilVram || untilVisible || untilTileData || untilBgReady) && isContinuableEmulationStatus(lastStatus)) {
		RunBudgetResult warmup{};
		VramWarmupTarget warmupTarget = VramWarmupTarget::Any;
		auto const warmupStartMs = uptimeMs(context);
		if (untilBgReady) {
			warmupTarget = VramWarmupTarget::TileData;
			warmup = runUntilBgReady(cpu, bus, maxWarmupCycles, minTileData, debugTrace ? &emuTrace : nullptr, &cartridge, breakPcPtr, fastHalt);
		} else if (untilTileData) {
			warmupTarget = VramWarmupTarget::TileData;
			warmup = runUntilVramTarget(cpu, bus, maxWarmupCycles, warmupTarget, 1, debugTrace ? &emuTrace : nullptr, &cartridge, breakPcPtr, fastHalt);
		} else if (untilVisible) {
			warmupTarget = VramWarmupTarget::Any;
			warmup = runUntilVramTarget(cpu, bus, maxWarmupCycles, warmupTarget, 1, debugTrace ? &emuTrace : nullptr, &cartridge, breakPcPtr, fastHalt);
		} else if (bus.vramWriteCount() == 0) {
			warmupTarget = VramWarmupTarget::Any;
			warmup = runUntilVramWrite(cpu, bus, maxWarmupCycles, debugTrace ? &emuTrace : nullptr, &cartridge, breakPcPtr, fastHalt);
		}
		auto const warmupEndMs = uptimeMs(context);
		auto const warmupWallMs = elapsedMs(warmupStartMs, warmupEndMs);

		totalSteps += warmup.steps;
		totalCycles += warmup.cycles;
		lastStatus = warmup.status;

		if (profile) {
			GbEmuProfileWindow warmupProfile{};
			warmupProfile.steps = warmup.steps;
			warmupProfile.cycles = warmup.cycles;
			warmupProfile.wallMs = warmupWallMs;
			warmupProfile.cpuMs = warmupWallMs;
			warmupProfile.cacheFills = cartridge.cacheFillCount() - previousCacheFills;
			warmupProfile.cacheRetries = cartridge.cacheRetryCount() - previousCacheRetries;
			warmupProfile.cacheFailures = cartridge.cacheFailureCount() - previousCacheFailures;
			warmupProfile.cacheReadBytes = cartridge.cacheReadBytes() - previousCacheReadBytes;
			warmupProfile.cacheFillMs = cartridge.cacheFillMs() - previousCacheFillMs;
			writeGbEmuProfileLine(context, "warmup", 0, warmupProfile, cyclesPerFrame);
		}
		previousCacheFills = cartridge.cacheFillCount();
		previousCacheRetries = cartridge.cacheRetryCount();
		previousCacheFailures = cartridge.cacheFailureCount();
		previousCacheReadBytes = cartridge.cacheReadBytes();
		previousCacheFillMs = cartridge.cacheFillMs();

		context.write("gbemu: warmup ");
		if (untilBgReady) {
			context.write("until-bg-ready");
		} else if (untilTileData) {
			context.write("until-tile-data");
		} else {
			context.write(untilVisible ? "until-visible" : "until-vram");
		}
		context.write(" target=");
		context.write(toString(warmupTarget));
		context.write(" target-nonzero=");
		writeUnsigned(context, countNonZeroForTarget(bus, warmupTarget));
		context.write(" tile-data=");
		writeUnsigned(context, countNonZeroForTarget(bus, VramWarmupTarget::TileData));
		context.write(" map=");
		writeUnsigned(context, countNonZeroForTarget(bus, VramWarmupTarget::Map));
		context.write(" steps=");
		writeUnsigned(context, warmup.steps);
		context.write(" cycles=");
		writeUnsigned(context, warmup.cycles);
		context.write(" vram-writes=");
		writeUnsigned(context, bus.vramWriteCount());
		context.write(" vram-nonzero=");
		writeUnsigned(context, bus.nonZeroVramBytes());
		context.write(" status=");
		context.write(lexxos::gameboy::toString(lastStatus));
		context.writeLine("");
		writeRunStopStep(context, "gbemu: ", warmup);

		bus.clearAudioFrames();

		if (warmup.breakpointHit) {
			emuBreakpointHit = true;
			context.write("gbemu: breakpoint pc=");
			writeHex16(context, breakPc);
			context.writeLine(" reached during warmup");
			writeGbEmuFaultSnapshot(context, "break-pc-warmup", 0U, cpu, bus, cartridge, framebuffer, bgMapMode, bgTileMode, emuTrace);
		}

		if (untilBgReady) {
			auto const tileData = countNonZeroForTarget(bus, VramWarmupTarget::TileData);
			auto const mapData = countNonZeroForTarget(bus, VramWarmupTarget::Map);
			if (tileData < minTileData || mapData == 0) {
				context.writeLine("gbemu: warning: bg-ready target was not reached before max-cycles");
			}
		}
	}

	bool sawVisibleFrame = false;
	bool debugStop = false;
	bool canceled = false;
	bool exitComboRequested = false;
	bool exitComboActive = false;
	std::uint32_t exitComboStartMs = 0;
	auto const syncStartMs = uptimeMs(context);
	auto const syncStartCycles = totalCycles;
	bool sawNonZeroBgp = bus.read8Fast(0xFF47U) != 0;
	std::uint32_t blankFrameStreak = 0;

	for (std::uint32_t frame = 0; (frames == 0 || frame < frames) && !emuBreakpointHit && !debugStop && !canceled && !exitComboRequested && isContinuableEmulationStatus(lastStatus);
	    ++frame) {
		if (!waitForInteractiveLocks()) {
			canceled = context.cancellationRequested();
			if (canceled) {
				lexxos::app::console_mirror::setMode(lexxos::app::ConsoleMirrorMode::Text);
				context.writeLine("gbemu: canceled");
			}
			break;
		}

		if (context.pollCancellationRequest()) {
			canceled = true;
			lexxos::app::console_mirror::setMode(lexxos::app::ConsoleMirrorMode::Text);
			context.writeLine("gbemu: canceled");
			break;
		}

		auto const frameWallStartMs = profile ? uptimeMs(context) : 0U;
		std::uint32_t frameInputMs = 0;

		if ((frame % inputEvery) == 0) {
			auto const inputStartMs = profile ? uptimeMs(context) : 0U;
			lastState = gamepad->state();
			updateExitComboInputState(exitComboInputCursor, exitComboInput);
			bus.setJoypadState(toJoypadState(lastState));
			if (exitComboPressed(lastState, exitComboInput)) {
				auto const nowMs = uptimeMs(context);
				if (!exitComboActive) {
					exitComboActive = true;
					exitComboStartMs = nowMs;
				}
				if (settings.exitHoldMs == 0 || elapsedMs(exitComboStartMs, nowMs) >= settings.exitHoldMs) {
					exitComboRequested = true;
				}
			} else {
				exitComboActive = false;
			}
			if (profile) {
				frameInputMs = elapsedMs(inputStartMs, uptimeMs(context));
				totalInputMs += frameInputMs;
			}
		}

		if (exitComboRequested) {
			break;
		}

		auto const frameStepLimit = cyclesPerFrame == 0 ? stepsPerFrame : maxGbEmuStepsPerFrame;
		auto const cpuStartMs = uptimeMs(context);
		auto const frameRun = runCpuBudget(cpu, bus, frameStepLimit, cyclesPerFrame, debugTrace ? &emuTrace : nullptr, &cartridge, breakPcPtr, breakPcFromPtr, fastHalt, fastTiming);
		auto const cpuEndMs = uptimeMs(context);
		auto const frameCpuMs = elapsedMs(cpuStartMs, cpuEndMs);
		totalCpuMs += frameCpuMs;
		auto const frameSteps = frameRun.steps;
		auto const frameCycles = frameRun.cycles;
		totalCycles += frameCycles;
		totalSteps += frameSteps;
		lastStatus = frameRun.status;

		if (audio != nullptr) {
			if (!flushGameBoyAudioFrames(context, *audio, bus)) {
				bus.attachCartridge(nullptr);
				lexxos::app::console_mirror::setMode(lexxos::app::ConsoleMirrorMode::Text);
				return 1;
			}
		}

		if (trace) {
			writeGbEmuTraceLine(context, frame, frameSteps, frameCycles, cpu, bus, cartridge);
		}
		if (verboseRunSteps) {
			writeRunStopStep(context, "gbemu: ", frameRun);
		}
		if (frameRun.breakpointHit) {
			emuBreakpointHit = true;
			context.write("gbemu: breakpoint pc=");
			writeHex16(context, breakPc);
			context.writeLine(" reached");
			writeGbEmuFaultSnapshot(context, "break-pc", frame + 1U, cpu, bus, cartridge, framebuffer, bgMapMode, bgTileMode, emuTrace);
		}

		if (frameRun.pcRangeHit) {
			debugStop = true;
			context.write("gbemu-debug: pc-range pc=");
			writeHex16(context, cpu.pc());
			context.write(" from=");
			writeHex16(context, breakPcFrom);
			context.writeLine(" reached");
			writeGbEmuFaultSnapshot(context, "pc-range", frame + 1U, cpu, bus, cartridge, framebuffer, bgMapMode, bgTileMode, emuTrace);
		}

		auto const frameBgp = bus.read8Fast(0xFF47U);
		if (frameBgp != 0) {
			sawNonZeroBgp = true;
		} else if (!debugStop && breakBgpZero && sawNonZeroBgp && frame + 1U >= breakBgpZeroAfter) {
			debugStop = true;
			context.write("gbemu-debug: bgp-zero frame=");
			writeUnsigned(context, frame + 1U);
			context.writeLine(" reached");
			writeGbEmuFaultSnapshot(context, "bgp-zero", frame + 1U, cpu, bus, cartridge, framebuffer, bgMapMode, bgTileMode, emuTrace);
		}

		lexxos::gameboy::BackgroundRenderOptions renderOptions{
		    .tileDataMode = bgTileMode,
		    .mapMode = bgMapMode,
		    .forceLcdEnable = forceVisible,
		    .forceBgEnable = forceVisible,
		    .forcePalette = forceVisible,
		    .palette = 0xE4,
		};

		auto const lastRequestedFrame = frames != 0 && frame + 1U == frames;
		auto const shouldRenderFrame = !noPpu && ((frame % renderEvery) == 0 || lastRequestedFrame || frameRun.breakpointHit || !isContinuableEmulationStatus(lastStatus));

		std::uint32_t framePpuMs = 0;
		std::uint32_t frameBlitMs = 0;

		if (shouldRenderFrame) {
			auto const ppuStartMs = uptimeMs(context);
			ppu.renderFrame(bus, framebuffer, renderOptions);
			auto const ppuEndMs = uptimeMs(context);
			framePpuMs = elapsedMs(ppuStartMs, ppuEndMs);
			totalPpuMs += framePpuMs;
			if (displayMode == lexxos::gameboy::DisplayBlitMode::None) {
				renderInfo = lexxos::gameboy::Ppu::layoutForDisplay(*display, displayMode);
			} else {
				auto const blitStartMs = uptimeMs(context);
				auto const blitOk = lexxos::gameboy::Ppu::blitFrameBuffer(*display, framebuffer, &renderInfo, displayMode, false, displayPalette);
				auto const blitEndMs = uptimeMs(context);
				frameBlitMs = elapsedMs(blitStartMs, blitEndMs);
				totalBlitMs += frameBlitMs;
				if (!blitOk) {
					bus.attachCartridge(nullptr);
					lexxos::app::console_mirror::setMode(lexxos::app::ConsoleMirrorMode::Text);
					context.writeErrorLine("gbemu: failed to render PPU frame");
					return 1;
				}
			}
			++renderedFrames;

			if (watchBlank) {
				auto const nonWhitePixels = countFrameNonWhitePixels(framebuffer);
				if (nonWhitePixels != 0) {
					sawVisibleFrame = true;
					blankFrameStreak = 0;
				} else if (sawVisibleFrame && frame + 1U >= watchBlankAfter) {
					++blankFrameStreak;
					if (blankFrameStreak >= watchBlankStreakTarget) {
						debugStop = true;
						context.write("gbemu-debug: blank streak=");
						writeUnsigned(context, blankFrameStreak);
						context.writeLine("");
						writeGbEmuFaultSnapshot(context, "blank-frame", frame + 1U, cpu, bus, cartridge, framebuffer, bgMapMode, bgTileMode, emuTrace);
					}
				}
			}
		}

		if (syncRealtime && cyclesPerFrame != 0) {
			auto targetElapsedMs = static_cast<std::uint32_t>((static_cast<std::uint64_t>(totalCycles - syncStartCycles) * 1000ULL) / dmgCpuClockHz);
			auto actualElapsedMs = elapsedMs(syncStartMs, uptimeMs(context));
			while (targetElapsedMs > actualElapsedMs) {
				if (context.pollCancellationRequest()) {
					canceled = true;
					lexxos::app::console_mirror::setMode(lexxos::app::ConsoleMirrorMode::Text);
					context.writeLine("gbemu: canceled");
					break;
				}

				auto sleepDurationMs = targetElapsedMs - actualElapsedMs;
				if (sleepDurationMs > maxGbSyncSleepChunkMs) {
					sleepDurationMs = maxGbSyncSleepChunkMs;
				}
				sleepMs(context, sleepDurationMs);
				actualElapsedMs = elapsedMs(syncStartMs, uptimeMs(context));
			}
		}

		if (canceled) {
			break;
		}

		sleepMs(context, 0);

		if (profile) {
			auto const frameWallEndMs = uptimeMs(context);
			auto const frameWallMs = elapsedMs(frameWallStartMs, frameWallEndMs);
			totalWallMs += frameWallMs;

			auto const cacheFills = cartridge.cacheFillCount();
			auto const cacheRetries = cartridge.cacheRetryCount();
			auto const cacheFailures = cartridge.cacheFailureCount();
			auto const cacheReadBytes = cartridge.cacheReadBytes();
			auto const cacheFillMs = cartridge.cacheFillMs();
			GbEmuProfileWindow frameProfile{};
			frameProfile.frames = 1;
			frameProfile.renderedFrames = shouldRenderFrame ? 1U : 0U;
			frameProfile.steps = frameSteps;
			frameProfile.cycles = frameCycles;
			frameProfile.wallMs = frameWallMs;
			frameProfile.inputMs = frameInputMs;
			frameProfile.cpuMs = frameCpuMs;
			frameProfile.ppuMs = framePpuMs;
			frameProfile.blitMs = frameBlitMs;
			frameProfile.cacheFills = cacheFills - previousCacheFills;
			frameProfile.cacheRetries = cacheRetries - previousCacheRetries;
			frameProfile.cacheFailures = cacheFailures - previousCacheFailures;
			frameProfile.cacheReadBytes = cacheReadBytes - previousCacheReadBytes;
			frameProfile.cacheFillMs = cacheFillMs - previousCacheFillMs;

			previousCacheFills = cacheFills;
			previousCacheRetries = cacheRetries;
			previousCacheFailures = cacheFailures;
			previousCacheReadBytes = cacheReadBytes;
			previousCacheFillMs = cacheFillMs;

			profileWindow.frames += frameProfile.frames;
			profileWindow.renderedFrames += frameProfile.renderedFrames;
			profileWindow.steps += frameProfile.steps;
			profileWindow.cycles += frameProfile.cycles;
			profileWindow.wallMs += frameProfile.wallMs;
			profileWindow.inputMs += frameProfile.inputMs;
			profileWindow.cpuMs += frameProfile.cpuMs;
			profileWindow.ppuMs += frameProfile.ppuMs;
			profileWindow.blitMs += frameProfile.blitMs;
			profileWindow.cacheFills += frameProfile.cacheFills;
			profileWindow.cacheRetries += frameProfile.cacheRetries;
			profileWindow.cacheFailures += frameProfile.cacheFailures;
			profileWindow.cacheReadBytes += frameProfile.cacheReadBytes;
			profileWindow.cacheFillMs += frameProfile.cacheFillMs;
			profileTotal.frames += frameProfile.frames;
			profileTotal.renderedFrames += frameProfile.renderedFrames;
			profileTotal.steps += frameProfile.steps;
			profileTotal.cycles += frameProfile.cycles;
			profileTotal.wallMs += frameProfile.wallMs;
			profileTotal.inputMs += frameProfile.inputMs;
			profileTotal.cpuMs += frameProfile.cpuMs;
			profileTotal.ppuMs += frameProfile.ppuMs;
			profileTotal.blitMs += frameProfile.blitMs;
			profileTotal.cacheFills += frameProfile.cacheFills;
			profileTotal.cacheRetries += frameProfile.cacheRetries;
			profileTotal.cacheFailures += frameProfile.cacheFailures;
			profileTotal.cacheReadBytes += frameProfile.cacheReadBytes;
			profileTotal.cacheFillMs += frameProfile.cacheFillMs;

			if (profileWindow.frames >= profileEvery || lastRequestedFrame || frameRun.breakpointHit || !isContinuableEmulationStatus(lastStatus)) {
				writeGbEmuProfileLine(context, "window", frame + 1U, profileWindow, cyclesPerFrame);
				profileWindow.reset();
			}
		}

		if (debugStop || frameRun.breakpointHit || frameRun.pcRangeHit || !isContinuableEmulationStatus(lastStatus)) {
			break;
		}
	}

	releaseInteractiveLocks();
	lexxos::app::console_mirror::setMode(lexxos::app::ConsoleMirrorMode::Text);

	if (exitComboRequested) {
		context.writeLine("gbemu: exit combo");
	}

	if (audio != nullptr) {
		(void) flushGameBoyAudioFrames(context, *audio, bus);
		if (!audio->drainPcmOutput(1500)) {
			context.writeLine("gbemu: audio drain timed out; forced stop");
		}
		lexxos::devices::AudioPcmOutputStats audioStats{};
		if (profile && audio->pcmOutputStats(audioStats)) {
			context.write("gbemu: audio frames queued/written=");
			writeUnsigned64(context, audioStats.framesQueued);
			context.write("/");
			writeUnsigned64(context, audioStats.framesWritten);
			context.writeLine("");
		}
	}

	context.write("gbemu: rendered frames: ");
	writeUnsigned(context, renderedFrames);
	context.writeLine("");
	if (profile) {
		writeGbEmuProfileLine(context, "total", profileTotal.frames, profileTotal, cyclesPerFrame);
		context.write("gbemu: timing wall-ms=");
		writeUnsigned(context, totalWallMs);
		context.write(" input-ms=");
		writeUnsigned(context, totalInputMs);
		context.write(" cpu-ms=");
	} else {
		context.write("gbemu: timing cpu-ms=");
	}
	writeUnsigned(context, totalCpuMs);
	context.write(" ppu-ms=");
	writeUnsigned(context, totalPpuMs);
	context.write(" blit-ms=");
	writeUnsigned(context, totalBlitMs);
	if (profile) {
		context.write(" other-ms=");
		writeUnsigned(context, profileOtherMs(profileTotal));
	}
	context.writeLine("");
	context.write("gbemu: cpu steps=");
	writeUnsigned(context, totalSteps);
	context.write(" cycles=");
	writeUnsigned(context, totalCycles);
	context.write(" status=");
	context.write(lexxos::gameboy::toString(lastStatus));
	context.writeLine("");
	writeDisplayRenderInfo(context, renderInfo);
	writeGamepadState(context, lastState);
	if (trace || faultTrace) {
		writeRunTrace(context, "gbemu: ", emuTrace);
	}
	writeGbEmuFinalSummary(context, bus, cpu, cartridge);
	if (noPpu) {
		context.writeLine("gbemu: ppu-analysis skipped");
	} else {
		writeBackgroundUsageStats(context, bus, framebuffer, bgMapMode, bgTileMode, "gbemu: ");
		writeSpriteUsageStats(context, bus, "gbemu: ");
	}

	bus.attachCartridge(nullptr);
	if (canceled) {
		return 130;
	}
	if (lastStatus == lexxos::gameboy::CpuStepStatus::BusReadError) {
		return 1;
	}
	if (lastStatus == lexxos::gameboy::CpuStepStatus::UnsupportedOpcode) {
		return 2;
	}

	return 0;
}

} // namespace lexxos::apps::gameboy_programs
