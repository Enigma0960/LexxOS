#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string_view>
#include <thread>
#include <vector>

#include <lexxos_sim_api.h>

#include <lexxos/devices/audio_device.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/platform/platform.h>

namespace lexxos::emulator {

struct SimInputEvent {
	std::uint16_t key{0};
	bool pressed{false};
	std::uint32_t timestampMs{0};
	std::uint8_t character{0};
	std::uint8_t modifiers{0};
};

class SimRuntime {
public:
	static constexpr std::size_t gpioCount = 64;
	static constexpr std::size_t uartCount = 4;
	static constexpr std::size_t inputKeyCount = 128;
	static constexpr std::size_t inputEventCapacity = 64;
	static constexpr std::size_t defaultDisplayWidth = 320;
	static constexpr std::size_t defaultDisplayHeight = 320;
	static constexpr std::size_t maxDisplayDimension = 4096;
	static constexpr std::size_t maxDisplayPixelCount = 16U * 1024U * 1024U;
	static constexpr std::uint32_t sdBlockSize = 512;

	explicit SimRuntime(LexxOsSimBoardApi const& api);
	SimRuntime(SimRuntime const&) = delete;
	SimRuntime& operator=(SimRuntime const&) = delete;
	~SimRuntime();

	int boot();
	void requestShutdown();
	[[nodiscard]] bool isRunning() const;
	int runMs(std::uint32_t milliseconds);

	int uartPushRx(std::uint8_t port, std::uint8_t const* data, std::size_t size);
	[[nodiscard]] std::size_t uartPopTx(std::uint8_t port, std::uint8_t* dst, std::size_t maxSize);
	[[nodiscard]] std::size_t uartTxSize(std::uint8_t port) const;

	bool readConsoleChar(char& value);
	void writeConsole(std::string_view text);

	int gpioSetInput(std::uint8_t pin, bool value);
	bool gpioRead(std::uint8_t pin, bool& value) const;
	void gpioWrite(std::uint8_t pin, bool value);
	int gpioGetOutput(std::uint8_t pin, bool& value) const;

	int inputSetKey(std::uint16_t key, bool pressed);
	int inputPushEvent(std::uint16_t key, bool pressed, std::uint8_t character, std::uint8_t modifiers);
	bool inputPollEvent(SimInputEvent& event);
	[[nodiscard]] bool inputIsPressed(std::uint16_t key) const;

	bool displayClear(std::uint16_t rgb565);
	bool displayBlitRgb565(lexxos::devices::Rect const& rect, std::uint16_t const* pixels, int stridePixels);
	void displayCommitFrame();
	[[nodiscard]] std::size_t displayWidth() const;
	[[nodiscard]] std::size_t displayHeight() const;
	[[nodiscard]] std::uint32_t displayFrameCounter() const;
	[[nodiscard]] std::size_t displayCopyRgb565(std::uint16_t* dst, std::size_t maxPixels) const;

	[[nodiscard]] LexxOsSimAudioMode audioMode() const;
	bool audioPlayTone(lexxos::devices::AudioToneRequest const& request);
	bool audioPlayPcmBuffer(lexxos::devices::AudioPcmBufferRequest const& request);
	bool audioDrainPcmOutput(std::uint32_t timeoutMs);
	void audioStopPcmOutput();
	void audioResetPcmOutputStats();
	[[nodiscard]] bool audioPcmOutputStats(lexxos::devices::AudioPcmOutputStats& stats) const;

	bool configureGpioInterrupt(
	    std::uint8_t pin,
	    lexxos::platform::GpioInterruptEdge edge,
	    lexxos::platform::GpioInterruptHandler handler,
	    void* userData);
	void disableGpioInterrupt(std::uint8_t pin);

	[[nodiscard]] std::uint32_t uptimeMs() const;
	void sleepMs(std::uint32_t milliseconds) const;

	[[nodiscard]] bool blockIsInserted(std::uint8_t deviceId) const;
	[[nodiscard]] bool blockIsReadOnly(std::uint8_t deviceId) const;
	[[nodiscard]] std::uint64_t blockCount(std::uint8_t deviceId) const;
	bool blockRead(std::uint8_t deviceId, std::uint64_t firstBlock, void* dst, std::size_t blockCount) const;
	int blockWrite(std::uint8_t deviceId, std::uint64_t firstBlock, void const* src, std::size_t blockCount) const;
	int hostDirectorySync(int stage) const;

	bool i2cProbe(std::uint8_t bus, std::uint8_t address) const;
	bool i2cWrite(std::uint8_t bus, std::uint8_t address, std::uint8_t const* data, std::size_t size) const;

	void log(int level, std::string_view message) const;

private:
	struct UartState {
		std::deque<std::uint8_t> rx;
		std::deque<std::uint8_t> tx;
	};

	struct GpioInterruptState {
		bool enabled{false};
		lexxos::platform::GpioInterruptEdge edge{lexxos::platform::GpioInterruptEdge::Both};
		lexxos::platform::GpioInterruptHandler handler{nullptr};
		void* userData{nullptr};
	};

	struct AudioState {
		lexxos::devices::AudioPcmOutputStats pcmStats{};
		std::uint32_t toneCalls{0};
	};

	[[nodiscard]] bool validUart(std::uint8_t port) const;
	[[nodiscard]] bool validGpio(std::uint8_t pin) const;
	[[nodiscard]] bool validInputKey(std::uint16_t key) const;
	[[nodiscard]] std::size_t displayOffset(int x, int y) const;
	void joinAppThread();

	LexxOsSimBoardApi _api{};
	std::chrono::steady_clock::time_point _startTime{};
	mutable std::mutex _mutex;
	std::array<UartState, uartCount> _uart{};
	std::array<bool, gpioCount> _gpioInput{};
	std::array<bool, gpioCount> _gpioOutput{};
	std::array<GpioInterruptState, gpioCount> _gpioInterrupts{};
	std::array<bool, inputKeyCount> _inputKeys{};
	std::deque<SimInputEvent> _inputEvents{};
	std::size_t _displayWidth{defaultDisplayWidth};
	std::size_t _displayHeight{defaultDisplayHeight};
	std::vector<std::uint16_t> _displayPixels{};
	std::uint32_t _displayFrameCounter{0};
	LexxOsSimAudioMode _audioMode{LEXXOS_SIM_AUDIO_OFF};
	AudioState _audio{};
	std::thread _appThread;
	std::atomic_bool _running{false};
	std::atomic_bool _bootStarted{false};
};

void setActiveRuntime(SimRuntime* runtime);
[[nodiscard]] SimRuntime* activeRuntime();

} // namespace lexxos::emulator
