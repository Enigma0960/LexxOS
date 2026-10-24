#include <lexxos_sim_api.h>

#include <new>

#include <sim_runtime.h>

using lexxos::emulator::SimRuntime;

namespace {

SimRuntime* runtimeFromHandle(LexxOsSim* sim) {
	return reinterpret_cast<SimRuntime*>(sim);
}

LexxOsSim* handleFromRuntime(SimRuntime* runtime) {
	return reinterpret_cast<LexxOsSim*>(runtime);
}

} // namespace

extern "C" LexxOsSim* lexxos_sim_create(LexxOsSimBoardApi const* api) {
	if (api == nullptr) {
		return nullptr;
	}

	try {
		return handleFromRuntime(new SimRuntime{*api});
	} catch (...) {
		return nullptr;
	}
}

extern "C" void lexxos_sim_destroy(LexxOsSim* sim) {
	delete runtimeFromHandle(sim);
}

extern "C" int lexxos_sim_boot(LexxOsSim* sim) {
	auto* runtime = runtimeFromHandle(sim);
	if (runtime == nullptr) {
		return LEXXOS_SIM_INVALID_ARGUMENT;
	}
	return runtime->boot();
}

extern "C" int lexxos_sim_request_shutdown(LexxOsSim* sim) {
	auto* runtime = runtimeFromHandle(sim);
	if (runtime == nullptr) {
		return LEXXOS_SIM_INVALID_ARGUMENT;
	}
	runtime->requestShutdown();
	return LEXXOS_SIM_OK;
}

extern "C" int lexxos_sim_is_running(LexxOsSim* sim) {
	auto* runtime = runtimeFromHandle(sim);
	if (runtime == nullptr) {
		return 0;
	}
	return runtime->isRunning() ? 1 : 0;
}

extern "C" int lexxos_sim_run_ms(LexxOsSim* sim, std::uint32_t milliseconds) {
	auto* runtime = runtimeFromHandle(sim);
	if (runtime == nullptr) {
		return LEXXOS_SIM_INVALID_ARGUMENT;
	}
	return runtime->runMs(milliseconds);
}

extern "C" int lexxos_sim_uart_push_rx(LexxOsSim* sim, std::uint8_t port, std::uint8_t const* data, std::size_t size) {
	auto* runtime = runtimeFromHandle(sim);
	if (runtime == nullptr) {
		return LEXXOS_SIM_INVALID_ARGUMENT;
	}
	return runtime->uartPushRx(port, data, size);
}

extern "C" std::size_t lexxos_sim_uart_pop_tx(LexxOsSim* sim, std::uint8_t port, std::uint8_t* dst, std::size_t maxSize) {
	auto* runtime = runtimeFromHandle(sim);
	if (runtime == nullptr) {
		return 0;
	}
	return runtime->uartPopTx(port, dst, maxSize);
}

extern "C" std::size_t lexxos_sim_uart_tx_size(LexxOsSim* sim, std::uint8_t port) {
	auto* runtime = runtimeFromHandle(sim);
	if (runtime == nullptr) {
		return 0;
	}
	return runtime->uartTxSize(port);
}

extern "C" int lexxos_sim_gpio_set_input(LexxOsSim* sim, std::uint8_t pin, int value) {
	auto* runtime = runtimeFromHandle(sim);
	if (runtime == nullptr) {
		return LEXXOS_SIM_INVALID_ARGUMENT;
	}
	return runtime->gpioSetInput(pin, value != 0);
}

extern "C" int lexxos_sim_gpio_get_output(LexxOsSim* sim, std::uint8_t pin, int* value) {
	auto* runtime = runtimeFromHandle(sim);
	if (runtime == nullptr || value == nullptr) {
		return LEXXOS_SIM_INVALID_ARGUMENT;
	}

	bool output = false;
	auto const result = runtime->gpioGetOutput(pin, output);
	if (result != LEXXOS_SIM_OK) {
		return result;
	}

	*value = output ? 1 : 0;
	return LEXXOS_SIM_OK;
}

extern "C" int lexxos_sim_input_set_key(LexxOsSim* sim, std::uint16_t key, int pressed) {
	auto* runtime = runtimeFromHandle(sim);
	if (runtime == nullptr) {
		return LEXXOS_SIM_INVALID_ARGUMENT;
	}
	return runtime->inputSetKey(key, pressed != 0);
}

extern "C" int lexxos_sim_input_push_event(
    LexxOsSim* sim, std::uint16_t key, int pressed, std::uint8_t character, std::uint8_t modifiers) {
	auto* runtime = runtimeFromHandle(sim);
	if (runtime == nullptr) {
		return LEXXOS_SIM_INVALID_ARGUMENT;
	}
	return runtime->inputPushEvent(key, pressed != 0, character, modifiers);
}

extern "C" int lexxos_sim_display_info(
    LexxOsSim* sim, int* width, int* height, int* format, std::uint32_t* frameCounter) {
	auto* runtime = runtimeFromHandle(sim);
	if (runtime == nullptr || width == nullptr || height == nullptr || format == nullptr || frameCounter == nullptr) {
		return LEXXOS_SIM_INVALID_ARGUMENT;
	}

	*width = static_cast<int>(runtime->displayWidth());
	*height = static_cast<int>(runtime->displayHeight());
	*format = 1;
	*frameCounter = runtime->displayFrameCounter();
	return LEXXOS_SIM_OK;
}

extern "C" std::size_t lexxos_sim_display_copy_rgb565(
    LexxOsSim* sim, std::uint16_t* dst, std::size_t maxPixels) {
	auto* runtime = runtimeFromHandle(sim);
	if (runtime == nullptr) {
		return 0;
	}
	return runtime->displayCopyRgb565(dst, maxPixels);
}
