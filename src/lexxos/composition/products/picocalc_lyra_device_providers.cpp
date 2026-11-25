#include <lexxos/composition/products/picocalc_lyra_device_providers.h>

#include <array>

#include <lexxos/composition/detail/picocalc_display_profile.h>
#include <lexxos/composition/products/lyra_lcd_dma.h>
#include <lexxos/composition/products/lyra_lcd_dma_workspace.h>
#include <lexxos/composition/products/lyra_spi_queues.h>
#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/display/double_buffered_display.h>
#include <lexxos/devices/display/legacy_ili9488_backend.h>
#include <lexxos/devices/input/keyboard_gamepad_adapter.h>
#include <lexxos/devices/input/picocalc_keyboard.h>
#include <lexxos/devices/picocalc/picocalc_southbridge_resource_policy.h>
#include <lexxos/devices/rk3506/lcd_dma_controller.h>
#include <lexxos/devices/rk3506/picocalc_lcd_dma_pl330.h>
#include <lexxos/devices/rk3506/picocalc_lcd_spi0.h>
#include <lexxos/feature.h>
#include <lexxos/hardware/legacy/lyra_display_pins.h>
#include <lexxos/hardware/products/picocalc_lyra_policy.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/platform/platform_services.h>

#if defined(LEXXOS_RK3506_FREERTOS_PERIPHERALS)
	#include <FreeRTOS.h>
	#include <task.h>
#else
	#include <lexxos/platform/rockchip/rk3506/cooperative_event.h>
#endif

#if FEATURE_ENABLED(DISPLAY)
	#include <lexxos/devices/display/ili9488_display.h>
#endif

#if FEATURE_ENABLED(DISPLAY)
extern "C" std::uint64_t rk3506_timer_counter();
#endif

namespace lexxos::composition::products {

namespace {

#if FEATURE_ENABLED(DISPLAY)
	devices::DoubleBufferedDisplay* bufferedDisplay{};
	devices::Ili9488Display* activeLcdPanel{};

	#if defined(LEXXOS_RK3506_FREERTOS_PERIPHERALS)
	TaskHandle_t presenterTaskHandle{};
	#else
	platform::rk3506::CooperativeEvent framePublished{};
	#endif

	devices::DisplayFrameSynchronization displaySynchronization() {
	#if defined(LEXXOS_RK3506_FREERTOS_PERIPHERALS)
		return {nullptr,
		    [](void*) -> std::uintptr_t { taskENTER_CRITICAL(); return 0; },
		    [](void*, std::uintptr_t) { taskEXIT_CRITICAL(); }};
	#else
		return {};
	#endif
	}

	bool displayTaskStarted{};

	// PIO presentation is CPU work. Share CPU0 with input and foreground
	// programs; producers wait with sleepMs when both buffers are occupied.
	constexpr std::uint32_t lcdPresenterTaskPriority = 1;

	void notifyDisplayTask() {
	#if defined(LEXXOS_RK3506_FREERTOS_PERIPHERALS)
		if (presenterTaskHandle != nullptr) {
			xTaskNotifyGive(presenterTaskHandle);
		}
	#else
		platform::rk3506::signalCooperativeEvent(framePublished);
		kernel::yield(); // targeted handoff to lcd-present if parked
	#endif
	}

	void enableDefaultRgb565Direct16() {
	#if defined(LEXXOS_RK3506_LCD_DIRECT16_DEFAULT)
		if (!bufferedDisplay || bufferedDisplay->wireFormat() != devices::DisplayWireFormat::Rgb565 || !lyraLcdDma().enabled()) {
			return;
		}

		devices::BusTransaction transaction{lyraSpi0Queue()};

		if (transaction) {
			(void) platform::rk3506::lcdSpi0SetRgb565Direct16(true);
		}
	#endif
	}

	void displayTask(void*) {
	#if defined(LEXXOS_RK3506_FREERTOS_PERIPHERALS)
		presenterTaskHandle = xTaskGetCurrentTaskHandle();
		#if defined(LEXXOS_RK3506_DMA_SAFE)
		// The GIC and FreeRTOS scheduler are alive at this point. Acquire
		// SPI0 before touching the shared PL330/SPI0 hardware.
		lyraLcdDma().setWaitFunction(kernel::sleepMs);
		{
			devices::BusTransaction transaction{lyraSpi0Queue()};
			if (transaction && lyraLcdDma().probe()) {
				(void) lyraLcdDma().setEnabled(true);
			}
		}
		#endif
	#else
		// Если IRQ ещё не был готов при boot, повторяем preflight из runtime.
		if (!lyraLcdDma().enabled()) {
			devices::BusTransaction transaction{lyraSpi0Queue()};
			if (transaction && lyraLcdDma().probe()) {
				(void) lyraLcdDma().setEnabled(true);
			}
		}
	#endif

	#if defined(LEXXOS_RK3506_FREERTOS_PERIPHERALS) && !defined(LEXXOS_RK3506_DMA_SAFE)
		platform::writeConsole("[lcd] FreeRTOS: SPI0 PIO; DMA disabled\n");
	#else
		platform::writeConsole(lyraLcdDma().enabled()
		        ? "[lcd] DMA enabled; two RGB565 framebuffers (400 KiB)\n"
		        : "[lcd] DMA unavailable; buffered PIO fallback: ");
		if (!lyraLcdDma().enabled()) {
			platform::writeConsole(lyraLcdDma().reason());
			platform::writeConsole("\n");
		}
	#endif

		lyraLcdDma().setWaitFunction(kernel::sleepMs);

		enableDefaultRgb565Direct16();
		if (bufferedDisplay->wireFormat() == devices::DisplayWireFormat::Rgb565) {
			platform::writeConsole(platform::rk3506::lcdSpi0Rgb565Direct16Enabled()
			        ? "[lcd] RGB565 direct16 active\n"
			        : "[lcd] RGB565 packed8 fallback\n");
		}

		bufferedDisplay->enableAsync();
		for (;;) {
			if (!bufferedDisplay->hasPendingFrame()) {
	#if defined(LEXXOS_RK3506_FREERTOS_PERIPHERALS)
				// A pending notification survives a publish/wait race.
				(void) ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
	#else
				platform::rk3506::waitCooperativeEvent(framePublished);
	#endif
				continue;
			}

			(void) bufferedDisplay->service();
		}
	}
#endif

} // namespace

devices::DoubleBufferedDisplayTiming picoCalcLyraDisplayTiming() {
#if FEATURE_ENABLED(DISPLAY)
	return bufferedDisplay ? bufferedDisplay->timing() : devices::DoubleBufferedDisplayTiming{};
#else
	return {};
#endif
}

devices::Ili9488DisplayTiming picoCalcLyraPanelTiming() {
#if FEATURE_ENABLED(DISPLAY)
	return activeLcdPanel ? activeLcdPanel->timing() : devices::Ili9488DisplayTiming{};
#else
	return {};
#endif
}

bool startPicoCalcLyraDisplayTask() {
#if FEATURE_ENABLED(DISPLAY)
	if (!bufferedDisplay || displayTaskStarted) {
		return true;
	}

	if (!kernel::createTask(displayTask, nullptr, {.name = "lcd-present", .stackSizeBytes = 8192, .priority = lcdPresenterTaskPriority, .executionClass = kernel::TaskExecutionClass::Driver})) {
		return false;
	}

	displayTaskStarted = true;
#endif
	return true;
}

void registerPicoCalcLyraInput(devices::DeviceManager& devices,
    platform::PlatformServices const& platformServices, devices::I2cBus& bus) {
	// Register even when the STM32 is absent at boot: the shared controller
	// retries initialization from the normal picocalc-input task.
	// Static lifetime is retained during this migration; bindings are boot-only.
	static lexxos::devices::PicoCalcKeyboard keyboard{bus, platformServices};
	keyboard.initialize();
	devices.registerInputDevice(keyboard.name(), "PicoCalc STM32 southbridge keyboard", keyboard);
	static lexxos::devices::KeyboardGamepadAdapter gamepad{
	    "picocalc-gamepad", keyboard,
	    {.up = lexxos::devices::KeyCode::Up, .down = lexxos::devices::KeyCode::Down, .left = lexxos::devices::KeyCode::Left, .right = lexxos::devices::KeyCode::Right, .a = lexxos::devices::KeyCode::I, .b = lexxos::devices::KeyCode::O, .x = lexxos::devices::KeyCode::K, .y = lexxos::devices::KeyCode::L, .start = lexxos::devices::KeyCode::Tab, .select = lexxos::devices::KeyCode::Backspace},
	    false
    };
	devices.registerGamepadDevice(gamepad.name(), "PicoCalc keyboard gamepad adapter", gamepad);
	devices.registerPicoCalcSouthbridgeService(
	    lexxos::devices::PicoCalcSouthbridgeResourcePolicy::serviceName,
	    "PicoCalc STM32 southbridge runtime service", keyboard.southbridgeRuntimeService());
}

void registerPicoCalcLyraDisplay(devices::DeviceManager& devices,
    platform::PlatformServices const& platformServices, hardware::products::LyraDisplayEndpoint const& endpoint, std::uint32_t frequencyHz) {
	auto const pins = hardware::legacy::lyraDisplayPins(endpoint, frequencyHz);
#if FEATURE_ENABLED(DISPLAY)
	#if FEATURE_ENABLED(DISPLAY_TEST_TRACE)
	lexxos::platform::writeConsole("[lcd-device] resolve endpoint pins\n");
	#endif

	if (!pins.isValid()) {
	#if FEATURE_ENABLED(DISPLAY_TEST_TRACE)
		lexxos::platform::writeConsole("[lcd-device] invalid display pins\n");
	#endif
		return;
	}

	#if FEATURE_ENABLED(DISPLAY_TEST_TRACE)
	lexxos::platform::writeConsole("[lcd-device] Ili9488Display::initialize begin\n");
	#endif
	lyraSpi0Queue().setWaitFunction(platformServices.clock.sleepMs);
	auto& dmaController = lyraLcdDma();
	static LyraLcdDmaWorkspace workspace;
	if (!platform::rk3506::lcdDmaBindController(dmaController)) {
		return;
	}

	static lexxos::devices::LegacyIli9488Backend backend{pins, platformServices};
	static lexxos::devices::Ili9488Display display{
	    {pins.width, pins.height, pins.columnOffset, pins.rowOffset, pins.invertColors,
	     lexxos::composition::detail::picoCalcDisplayWireFormat,
	     lexxos::composition::detail::picoCalcDisplayReset},
	    backend, workspace.buffers(), &lyraSpi0Queue()
    };

	display.setTimingCounter(rk3506_timer_counter);

	if (display.initialize()) {
	#if FEATURE_ENABLED(DISPLAY_TEST_TRACE)
		lexxos::platform::writeConsole("[lcd-device] initialize OK; register display\n");
	#endif
		// Два полных кадра принадлежат продукту, не универсальному драйверу.
		static std::array<std::uint16_t, 320 * 320> first{}, second{};
		static devices::DoubleBufferedDisplay buffered{display, first, second, kernel::sleepMs, rk3506_timer_counter, notifyDisplayTask,
		    hardware::products::picoCalcLyraHardwareScrollEnabled, displaySynchronization()};

	#if !defined(LEXXOS_RK3506_FREERTOS_PERIPHERALS)
		{
			devices::BusTransaction transaction{lyraSpi0Queue()};
			if (transaction && workspace.registerDmaSource(dmaController) && lyraLcdDma().probe()) {
				(void) lyraLcdDma().setEnabled(true);
			}
		}
	#endif

		if (!buffered.initialize()) {
			return;
		}

		activeLcdPanel = &display;
		bufferedDisplay = &buffered;
		enableDefaultRgb565Direct16();
		devices.registerDisplayDevice(buffered.name(), "PicoCalc ILI9488 double-buffered RGB display", buffered);
	} else {
	#if FEATURE_ENABLED(DISPLAY_TEST_TRACE)
		lexxos::platform::writeConsole("[lcd-device] initialize FAILED\n");
	#endif
	}
#else
	(void) devices;
	(void) platformServices;
	(void) pins;
#endif
}

} // namespace lexxos::composition::products
