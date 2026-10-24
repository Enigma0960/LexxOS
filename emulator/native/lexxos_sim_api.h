#pragma once

#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
	#define LEXXOS_SIM_EXPORT __declspec(dllexport)
#else
	#define LEXXOS_SIM_EXPORT __attribute__((visibility("default")))
#endif

extern "C" {

struct LexxOsSim;

enum LexxOsSimResult : int {
	LEXXOS_SIM_OK = 0,
	LEXXOS_SIM_ERROR = -1,
	LEXXOS_SIM_INVALID_ARGUMENT = -2,
	LEXXOS_SIM_BUSY = -3,
	LEXXOS_SIM_NOT_RUNNING = -4,
	LEXXOS_SIM_WRITE_PROTECTED = -5,
};

enum LexxOsSimLogLevel : int {
	LEXXOS_SIM_LOG_DEBUG = 0,
	LEXXOS_SIM_LOG_INFO = 1,
	LEXXOS_SIM_LOG_WARNING = 2,
	LEXXOS_SIM_LOG_ERROR = 3,
};

enum LexxOsSimAudioMode : std::uint8_t {
	LEXXOS_SIM_AUDIO_OFF = 0,
	LEXXOS_SIM_AUDIO_PWM = 1,
	LEXXOS_SIM_AUDIO_ADC = 2,
};

enum LexxOsSimBlockMediaFlags : int {
	LEXXOS_SIM_BLOCK_INSERTED = 1,
	LEXXOS_SIM_BLOCK_READ_ONLY = 2,
};

struct LexxOsSimBoardApi {
	void* user;

	std::uint16_t display_width;
	std::uint16_t display_height;
	std::uint8_t audio_mode;
	std::uint8_t reserved[3];

	std::uint64_t (*now_us)(void* user);

	// Returns media flags; legacy callbacks returning 0/1 remain valid.
	int (*block_is_inserted)(void* user, std::uint8_t device_id);
	std::uint64_t (*block_count)(void* user, std::uint8_t device_id);
	int (*block_read)(void* user, std::uint8_t device_id, std::uint64_t first_block, std::uint8_t* dst, std::size_t block_count);
	// Return OK, WRITE_PROTECTED (media remains readable), or an error.
	int (*block_write)(void* user, std::uint8_t device_id, std::uint64_t first_block, std::uint8_t const* src, std::size_t block_count);

	int (*i2c_probe)(void* user, std::uint8_t bus, std::uint8_t address);
	int (*i2c_write)(void* user, std::uint8_t bus, std::uint8_t address, std::uint8_t const* data, std::size_t size);

	void (*log)(void* user, int level, char const* message);

	int (*audio_tone)(
	    void* user,
	    std::uint8_t mode,
	    std::uint8_t channel,
	    std::uint32_t frequency_hz,
	    std::uint32_t duration_ms,
	    std::uint8_t volume);
	int (*audio_pcm)(
	    void* user,
	    std::uint8_t mode,
	    std::uint8_t channel,
	    std::int16_t const* frames,
	    std::size_t frame_count,
	    std::uint32_t sample_rate_hz);
	int (*audio_drain)(void* user, std::uint8_t mode, std::uint32_t timeout_ms);
	void (*audio_stop)(void* user, std::uint8_t mode);

	// Host-only sync protocol: 0=preflight, 1=apply while unmounted, 2=cancel.
	int (*host_directory_sync)(void* user, int stage);
};

LEXXOS_SIM_EXPORT LexxOsSim* lexxos_sim_create(LexxOsSimBoardApi const* api);
LEXXOS_SIM_EXPORT void lexxos_sim_destroy(LexxOsSim* sim);

LEXXOS_SIM_EXPORT int lexxos_sim_boot(LexxOsSim* sim);
LEXXOS_SIM_EXPORT int lexxos_sim_request_shutdown(LexxOsSim* sim);
LEXXOS_SIM_EXPORT int lexxos_sim_is_running(LexxOsSim* sim);
LEXXOS_SIM_EXPORT int lexxos_sim_run_ms(LexxOsSim* sim, std::uint32_t milliseconds);

LEXXOS_SIM_EXPORT int lexxos_sim_uart_push_rx(LexxOsSim* sim, std::uint8_t port, std::uint8_t const* data, std::size_t size);
LEXXOS_SIM_EXPORT std::size_t lexxos_sim_uart_pop_tx(LexxOsSim* sim, std::uint8_t port, std::uint8_t* dst, std::size_t max_size);
LEXXOS_SIM_EXPORT std::size_t lexxos_sim_uart_tx_size(LexxOsSim* sim, std::uint8_t port);

LEXXOS_SIM_EXPORT int lexxos_sim_gpio_set_input(LexxOsSim* sim, std::uint8_t pin, int value);
LEXXOS_SIM_EXPORT int lexxos_sim_gpio_get_output(LexxOsSim* sim, std::uint8_t pin, int* value);

LEXXOS_SIM_EXPORT int lexxos_sim_input_set_key(LexxOsSim* sim, std::uint16_t key, int pressed);
LEXXOS_SIM_EXPORT int lexxos_sim_input_push_event(
    LexxOsSim* sim, std::uint16_t key, int pressed, std::uint8_t character, std::uint8_t modifiers);

LEXXOS_SIM_EXPORT int lexxos_sim_display_info(
    LexxOsSim* sim, int* width, int* height, int* format, std::uint32_t* frame_counter);
LEXXOS_SIM_EXPORT std::size_t lexxos_sim_display_copy_rgb565(
    LexxOsSim* sim, std::uint16_t* dst, std::size_t max_pixels);
}
