#include <sim_runtime.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>
#include <thread>

#include <lexxos/app/app.h>
#include <lexxos/kernel/system.h>

namespace lexxos::emulator {
namespace {

	std::atomic<SimRuntime*> g_activeRuntime{nullptr};

	std::size_t sanitizeDisplayDimension(std::uint16_t value, std::size_t fallback) {
		if (value == 0 || value > SimRuntime::maxDisplayDimension) {
			return fallback;
		}
		return value;
	}

	LexxOsSimAudioMode sanitizeAudioMode(std::uint8_t mode) {
		switch (mode) {
			case LEXXOS_SIM_AUDIO_PWM:
				return LEXXOS_SIM_AUDIO_PWM;
			case LEXXOS_SIM_AUDIO_ADC:
				return LEXXOS_SIM_AUDIO_ADC;
			case LEXXOS_SIM_AUDIO_OFF:
			default:
				return LEXXOS_SIM_AUDIO_OFF;
		}
	}

} // namespace

SimRuntime::SimRuntime(LexxOsSimBoardApi const& api)
    : _api(api)
    , _startTime(std::chrono::steady_clock::now())
    , _displayWidth(sanitizeDisplayDimension(api.display_width, defaultDisplayWidth))
    , _displayHeight(sanitizeDisplayDimension(api.display_height, defaultDisplayHeight))
    , _audioMode(sanitizeAudioMode(api.audio_mode)) {
	if (_displayWidth * _displayHeight > maxDisplayPixelCount) {
		_displayWidth = defaultDisplayWidth;
		_displayHeight = defaultDisplayHeight;
	}
	_displayPixels.assign(_displayWidth * _displayHeight, 0);
}

SimRuntime::~SimRuntime() {
	requestShutdown();
	joinAppThread();
	if (activeRuntime() == this) {
		setActiveRuntime(nullptr);
	}
}

int SimRuntime::boot() {
	bool expected = false;
	if (!_bootStarted.compare_exchange_strong(expected, true)) {
		return LEXXOS_SIM_BUSY;
	}

	setActiveRuntime(this);
	_running.store(true, std::memory_order_release);
	_appThread = std::thread([this]() {
		setActiveRuntime(this);
		auto const result = lexxos::app::run();
		if (result != 0) {
			log(LEXXOS_SIM_LOG_ERROR, "LexxOS app returned a non-zero result");
		}
		_running.store(false, std::memory_order_release);
	});

	return LEXXOS_SIM_OK;
}

void SimRuntime::requestShutdown() {
	lexxos::kernel::requestShutdown();
}

bool SimRuntime::isRunning() const {
	return _running.load(std::memory_order_acquire);
}

int SimRuntime::runMs(std::uint32_t milliseconds) {
	if (!isRunning()) {
		return LEXXOS_SIM_NOT_RUNNING;
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
	return LEXXOS_SIM_OK;
}

int SimRuntime::uartPushRx(std::uint8_t port, std::uint8_t const* data, std::size_t size) {
	if (!validUart(port) || (data == nullptr && size != 0)) {
		return LEXXOS_SIM_INVALID_ARGUMENT;
	}

	std::lock_guard lock{_mutex};
	auto& queue = _uart[port].rx;
	for (std::size_t index = 0; index < size; ++index) {
		queue.push_back(data[index]);
	}
	return LEXXOS_SIM_OK;
}

std::size_t SimRuntime::uartPopTx(std::uint8_t port, std::uint8_t* dst, std::size_t maxSize) {
	if (!validUart(port) || dst == nullptr || maxSize == 0) {
		return 0;
	}

	std::lock_guard lock{_mutex};
	auto& queue = _uart[port].tx;
	std::size_t const count = std::min(maxSize, queue.size());
	for (std::size_t index = 0; index < count; ++index) {
		dst[index] = queue.front();
		queue.pop_front();
	}
	return count;
}

std::size_t SimRuntime::uartTxSize(std::uint8_t port) const {
	if (!validUart(port)) {
		return 0;
	}

	std::lock_guard lock{_mutex};
	return _uart[port].tx.size();
}

bool SimRuntime::readConsoleChar(char& value) {
	std::lock_guard lock{_mutex};
	auto& queue = _uart[0].rx;
	if (queue.empty()) {
		return false;
	}

	value = static_cast<char>(queue.front());
	queue.pop_front();
	return true;
}

void SimRuntime::writeConsole(std::string_view text) {
	{
		std::lock_guard lock{_mutex};
		auto& queue = _uart[0].tx;
		for (char const character : text) {
			queue.push_back(static_cast<std::uint8_t>(character));
		}
	}

	if (_api.log != nullptr) {
		std::string buffer{text};
		_api.log(_api.user, LEXXOS_SIM_LOG_INFO, buffer.c_str());
	}
}

int SimRuntime::gpioSetInput(std::uint8_t pin, bool value) {
	if (!validGpio(pin)) {
		return LEXXOS_SIM_INVALID_ARGUMENT;
	}

	lexxos::platform::GpioInterruptHandler handler = nullptr;
	void* userData = nullptr;
	bool triggerInterrupt = false;

	{
		std::lock_guard lock{_mutex};
		bool const oldValue = _gpioInput[pin];
		_gpioInput[pin] = value;

		auto const& interrupt = _gpioInterrupts[pin];
		if (interrupt.enabled && interrupt.handler != nullptr && oldValue != value) {
			switch (interrupt.edge) {
				case lexxos::platform::GpioInterruptEdge::Rising:
					triggerInterrupt = !oldValue && value;
					break;
				case lexxos::platform::GpioInterruptEdge::Falling:
					triggerInterrupt = oldValue && !value;
					break;
				case lexxos::platform::GpioInterruptEdge::Both:
					triggerInterrupt = true;
					break;
			}

			if (triggerInterrupt) {
				handler = interrupt.handler;
				userData = interrupt.userData;
			}
		}
	}

	if (handler != nullptr) {
		handler(lexxos::platform::GpioPin{.number = pin}, value, userData);
	}

	return LEXXOS_SIM_OK;
}

bool SimRuntime::gpioRead(std::uint8_t pin, bool& value) const {
	if (!validGpio(pin)) {
		return false;
	}

	std::lock_guard lock{_mutex};
	value = _gpioInput[pin];
	return true;
}

void SimRuntime::gpioWrite(std::uint8_t pin, bool value) {
	if (!validGpio(pin)) {
		return;
	}

	std::lock_guard lock{_mutex};
	_gpioOutput[pin] = value;
}

int SimRuntime::gpioGetOutput(std::uint8_t pin, bool& value) const {
	if (!validGpio(pin)) {
		return LEXXOS_SIM_INVALID_ARGUMENT;
	}

	std::lock_guard lock{_mutex};
	value = _gpioOutput[pin];
	return LEXXOS_SIM_OK;
}

int SimRuntime::inputSetKey(std::uint16_t key, bool pressed) {
	return inputPushEvent(key, pressed, 0, 0);
}

int SimRuntime::inputPushEvent(std::uint16_t key, bool pressed, std::uint8_t character, std::uint8_t modifiers) {
	if (!validInputKey(key)) {
		return LEXXOS_SIM_INVALID_ARGUMENT;
	}

	auto const timestampMs = uptimeMs();
	std::lock_guard lock{_mutex};
	if (_inputKeys[key] == pressed) {
		return LEXXOS_SIM_OK;
	}

	_inputKeys[key] = pressed;
	if (_inputEvents.size() >= inputEventCapacity) {
		_inputEvents.pop_front();
	}
	_inputEvents.push_back({
	    .key = key,
	    .pressed = pressed,
	    .timestampMs = timestampMs,
	    .character = character,
	    .modifiers = modifiers,
	});
	return LEXXOS_SIM_OK;
}

bool SimRuntime::inputPollEvent(SimInputEvent& event) {
	std::lock_guard lock{_mutex};
	if (_inputEvents.empty()) {
		return false;
	}

	event = _inputEvents.front();
	_inputEvents.pop_front();
	return true;
}

bool SimRuntime::inputIsPressed(std::uint16_t key) const {
	if (!validInputKey(key)) {
		return false;
	}

	std::lock_guard lock{_mutex};
	return _inputKeys[key];
}

bool SimRuntime::displayClear(std::uint16_t rgb565) {
	std::lock_guard lock{_mutex};
	std::fill(_displayPixels.begin(), _displayPixels.end(), rgb565);
	return true;
}

bool SimRuntime::displayBlitRgb565(lexxos::devices::Rect const& rect, std::uint16_t const* pixels, int stridePixels) {
	if (pixels == nullptr || stridePixels <= 0 || rect.width <= 0 || rect.height <= 0) {
		return false;
	}

	int const x0 = std::max(rect.x, 0);
	int const y0 = std::max(rect.y, 0);
	int const x1 = std::min(rect.x + rect.width, static_cast<int>(_displayWidth));
	int const y1 = std::min(rect.y + rect.height, static_cast<int>(_displayHeight));
	if (x0 >= x1 || y0 >= y1) {
		return true;
	}

	std::lock_guard lock{_mutex};
	for (int y = y0; y < y1; ++y) {
		auto const* src = pixels + (y - rect.y) * stridePixels + (x0 - rect.x);
		auto* dst = _displayPixels.data() + displayOffset(x0, y);
		std::copy(src, src + (x1 - x0), dst);
	}
	return true;
}

void SimRuntime::displayCommitFrame() {
	std::lock_guard lock{_mutex};
	++_displayFrameCounter;
}

std::size_t SimRuntime::displayWidth() const {
	return _displayWidth;
}

std::size_t SimRuntime::displayHeight() const {
	return _displayHeight;
}

std::uint32_t SimRuntime::displayFrameCounter() const {
	std::lock_guard lock{_mutex};
	return _displayFrameCounter;
}

std::size_t SimRuntime::displayCopyRgb565(std::uint16_t* dst, std::size_t maxPixels) const {
	if (dst == nullptr || maxPixels == 0) {
		return 0;
	}

	std::lock_guard lock{_mutex};
	auto const count = std::min(maxPixels, _displayPixels.size());
	std::copy(_displayPixels.begin(), _displayPixels.begin() + static_cast<std::ptrdiff_t>(count), dst);
	return count;
}

LexxOsSimAudioMode SimRuntime::audioMode() const {
	return _audioMode;
}

bool SimRuntime::audioPlayTone(lexxos::devices::AudioToneRequest const& request) {
	if (_audioMode == LEXXOS_SIM_AUDIO_OFF) {
		return false;
	}

	{
		std::lock_guard lock{_mutex};
		++_audio.toneCalls;
	}

	if (_api.audio_tone == nullptr) {
		return true;
	}
	return _api.audio_tone(
	           _api.user,
	           static_cast<std::uint8_t>(_audioMode),
	           static_cast<std::uint8_t>(request.channel),
	           request.frequencyHz,
	           request.durationMs,
	           request.volume)
	    == 0;
}

bool SimRuntime::audioPlayPcmBuffer(lexxos::devices::AudioPcmBufferRequest const& request) {
	if (_audioMode == LEXXOS_SIM_AUDIO_OFF || request.sampleRateHz == 0 || (request.frames == nullptr && request.frameCount != 0)) {
		return false;
	}

	{
		std::lock_guard lock{_mutex};
		auto& stats = _audio.pcmStats;
		stats.framesQueued += request.frameCount;
		stats.framesWritten += request.frameCount;
		++stats.bufferCalls;
		stats.sampleRateHz = request.sampleRateHz;
		stats.ringFill = 0;
		if (request.frameCount > stats.ringHighWatermark) {
			stats.ringHighWatermark =
			    static_cast<std::uint32_t>(std::min<std::size_t>(request.frameCount, std::numeric_limits<std::uint32_t>::max()));
		}
	}

	if (_api.audio_pcm == nullptr || request.frameCount == 0) {
		return true;
	}
	return _api.audio_pcm(
	           _api.user,
	           static_cast<std::uint8_t>(_audioMode),
	           static_cast<std::uint8_t>(request.channel),
	           reinterpret_cast<std::int16_t const*>(request.frames),
	           request.frameCount,
	           request.sampleRateHz)
	    == 0;
}

bool SimRuntime::audioDrainPcmOutput(std::uint32_t timeoutMs) {
	if (_audioMode == LEXXOS_SIM_AUDIO_OFF) {
		return false;
	}
	if (_api.audio_drain == nullptr) {
		return true;
	}
	return _api.audio_drain(_api.user, static_cast<std::uint8_t>(_audioMode), timeoutMs) == 0;
}

void SimRuntime::audioStopPcmOutput() {
	if (_audioMode == LEXXOS_SIM_AUDIO_OFF) {
		return;
	}

	{
		std::lock_guard lock{_mutex};
		_audio.pcmStats.ringFill = 0;
	}

	if (_api.audio_stop != nullptr) {
		_api.audio_stop(_api.user, static_cast<std::uint8_t>(_audioMode));
	}
}

void SimRuntime::audioResetPcmOutputStats() {
	std::lock_guard lock{_mutex};
	_audio.pcmStats = lexxos::devices::AudioPcmOutputStats{};
}

bool SimRuntime::audioPcmOutputStats(lexxos::devices::AudioPcmOutputStats& stats) const {
	if (_audioMode == LEXXOS_SIM_AUDIO_OFF) {
		stats = {};
		return false;
	}

	std::lock_guard lock{_mutex};
	stats = _audio.pcmStats;
	return true;
}

bool SimRuntime::configureGpioInterrupt(
    std::uint8_t pin,
    lexxos::platform::GpioInterruptEdge edge,
    lexxos::platform::GpioInterruptHandler handler,
    void* userData) {
	if (!validGpio(pin) || handler == nullptr) {
		return false;
	}

	std::lock_guard lock{_mutex};
	_gpioInterrupts[pin] = GpioInterruptState{
	    .enabled = true,
	    .edge = edge,
	    .handler = handler,
	    .userData = userData,
	};
	return true;
}

void SimRuntime::disableGpioInterrupt(std::uint8_t pin) {
	if (!validGpio(pin)) {
		return;
	}

	std::lock_guard lock{_mutex};
	_gpioInterrupts[pin] = GpioInterruptState{};
}

std::uint32_t SimRuntime::uptimeMs() const {
	if (_api.now_us != nullptr) {
		return static_cast<std::uint32_t>(_api.now_us(_api.user) / 1000U);
	}

	auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _startTime);
	return static_cast<std::uint32_t>(elapsed.count());
}

void SimRuntime::sleepMs(std::uint32_t milliseconds) const {
	std::uint32_t remaining = milliseconds;
	while (remaining > 0 && !lexxos::kernel::isShutdownRequested()) {
		auto const chunk = std::min<std::uint32_t>(remaining, 5);
		std::this_thread::sleep_for(std::chrono::milliseconds(chunk));
		remaining -= chunk;
	}
}

bool SimRuntime::blockIsInserted(std::uint8_t deviceId) const {
	if (_api.block_is_inserted == nullptr) {
		return false;
	}
	return (_api.block_is_inserted(_api.user, deviceId) & LEXXOS_SIM_BLOCK_INSERTED) != 0;
}

bool SimRuntime::blockIsReadOnly(std::uint8_t deviceId) const {
	return _api.block_is_inserted != nullptr
	    && (_api.block_is_inserted(_api.user, deviceId) & LEXXOS_SIM_BLOCK_READ_ONLY) != 0;
}

std::uint64_t SimRuntime::blockCount(std::uint8_t deviceId) const {
	if (_api.block_count == nullptr) {
		return 0;
	}
	return _api.block_count(_api.user, deviceId);
}

bool SimRuntime::blockRead(std::uint8_t deviceId, std::uint64_t firstBlock, void* dst, std::size_t count) const {
	if (_api.block_read == nullptr || dst == nullptr) {
		return false;
	}
	return _api.block_read(_api.user, deviceId, firstBlock, static_cast<std::uint8_t*>(dst), count) == 0;
}

int SimRuntime::blockWrite(std::uint8_t deviceId, std::uint64_t firstBlock, void const* src, std::size_t count) const {
	if (_api.block_write == nullptr || src == nullptr) {
		return LEXXOS_SIM_ERROR;
	}
	return _api.block_write(_api.user, deviceId, firstBlock, static_cast<std::uint8_t const*>(src), count);
}

int SimRuntime::hostDirectorySync(int stage) const {
	return _api.host_directory_sync != nullptr ? _api.host_directory_sync(_api.user, stage) : 0;
}

bool SimRuntime::i2cProbe(std::uint8_t bus, std::uint8_t address) const {
	if (_api.i2c_probe == nullptr) {
		return false;
	}
	return _api.i2c_probe(_api.user, bus, address) == 0;
}

bool SimRuntime::i2cWrite(std::uint8_t bus, std::uint8_t address, std::uint8_t const* data, std::size_t size) const {
	if (_api.i2c_write == nullptr) {
		return false;
	}
	return _api.i2c_write(_api.user, bus, address, data, size) == 0;
}

void SimRuntime::log(int level, std::string_view message) const {
	if (_api.log == nullptr) {
		return;
	}

	std::string buffer{message};
	_api.log(_api.user, level, buffer.c_str());
}

bool SimRuntime::validUart(std::uint8_t port) const {
	return port < _uart.size();
}

bool SimRuntime::validGpio(std::uint8_t pin) const {
	return pin < _gpioInput.size();
}

bool SimRuntime::validInputKey(std::uint16_t key) const {
	return key < _inputKeys.size();
}

std::size_t SimRuntime::displayOffset(int x, int y) const {
	return static_cast<std::size_t>(y) * _displayWidth + static_cast<std::size_t>(x);
}

void SimRuntime::joinAppThread() {
	if (_appThread.joinable()) {
		_appThread.join();
	}
}

void setActiveRuntime(SimRuntime* runtime) {
	g_activeRuntime.store(runtime, std::memory_order_release);
}

SimRuntime* activeRuntime() {
	return g_activeRuntime.load(std::memory_order_acquire);
}

} // namespace lexxos::emulator

extern "C" int lexxos_sim_host_directory_sync(int stage) {
	auto* runtime = lexxos::emulator::activeRuntime();
	return runtime != nullptr ? runtime->hostDirectorySync(stage) : 0;
}
