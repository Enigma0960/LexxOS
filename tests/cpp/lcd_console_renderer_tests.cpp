#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <lexxos/app/detail/picocalc_lcd_console_renderer.h>
#include <lexxos/platform/platform_services.h>

namespace {

std::uint32_t now{}, clockStep{};
unsigned criticalDepth{};

void expect(bool ok, char const* message) {
	if (!ok) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

std::uint32_t uptime() {
	auto const value = now;
	now += clockStep;
	return value;
}

class Panel final : public lexxos::devices::DisplayDevice {
public:
	unsigned frames{}, textStreams{}, statusStreams{}, rgb565Writes{}, rgb888Writes{};
	lexxos::devices::Rect stream{};
	std::size_t offset{};
	std::vector<std::uint16_t> pixels = std::vector<std::uint16_t>(320 * 320);

	std::string_view name() const override { return "ili9488-spi"; }

	bool isReady() const override { return true; }

	int width() const override { return 320; }

	int height() const override { return 320; }

	lexxos::devices::PixelFormat nativeFormat() const override { return lexxos::devices::PixelFormat::Rgb565; }

	bool clear(std::uint16_t color) override {
		std::fill(pixels.begin(), pixels.end(), color);
		return true;
	}

	bool prefersRgb565Stream() const override { return true; }

	bool beginFrame() override {
		expect(criticalDepth == 0, "frame inside critical section");
		++frames;
		return true;
	}

	bool endFrame() override {
		expect(criticalDepth == 0, "publish inside critical section");
		return true;
	}

	bool blitRgb565(lexxos::devices::Rect const&, std::uint16_t const*, int) override { return true; }

	bool beginRgb565Stream(lexxos::devices::Rect const& rect) override {
		expect(criticalDepth == 0, "pixel rendering inside critical section");
		if (rect.y < 24) {
			++statusStreams;
		} else {
			++textStreams;
		}

		stream = rect;
		offset = 0;
		return true;
	}

	bool writeRgb565Stream(std::uint16_t const* data, int count) override {
		++rgb565Writes;
		for (int i = 0; i < count; ++i, ++offset) {
			pixels[(stream.y + offset / stream.width) * 320 + stream.x + offset % stream.width] = data[i];
		}

		return true;
	}

	bool writeRgb888Stream(std::uint8_t const*, int) override {
		++rgb888Writes;
		return true;
	}

	bool endRgb565Stream() override { return true; }

	bool fillRgb565(lexxos::devices::Rect const& rect, std::uint16_t color) override {
		++textStreams;
		for (int y = rect.y; y < rect.y + rect.height; ++y) {
			std::fill_n(pixels.data() + y * 320 + rect.x, rect.width, color);
		}

		return true;
	}
};

} // namespace

void testEnterCritical() {
	++criticalDepth;
}

void testExitCritical() {
	expect(criticalDepth > 0, "unbalanced critical section");
	--criticalDepth;
}

// The renderer logs early LCD diagnostics; the test has no physical UART.
namespace lexxos::lifecycle {

EventBus& defaultEventBus() {
	static EventBus bus;
	return bus;
}

} // namespace lexxos::lifecycle

namespace lexxos::platform {

LegacyBoardDescriptor const& board() {
	static LegacyBoardDescriptor descriptor{};
	return descriptor;
}

void writeConsole(std::string_view) {
}

} // namespace lexxos::platform

// The renderer only needs these OS accessors; retain the actual DeviceManager.
namespace lexxos::app {

ProgramInputService::ProgramInputService(lifecycle::EventBus& events)
    : _events(events) {
}

} // namespace lexxos::app

namespace lexxos::os {

OperatingSystem::OperatingSystem(lexxos::platform::PlatformServices const& services)
    : _core(services)
    , _programInput([]() -> app::ProgramInputService& {
	    static app::ProgramInputService input{lifecycle::defaultEventBus()};
	    return input;
    }())
    , _consoleInput([]() -> app::ConsoleInputService& {
	    static app::ConsoleInputService input;
	    return input;
    }()) {
}

lexxos::devices::DeviceManager& OperatingSystem::devices() {
	return _core.devices();
}

lexxos::platform::PlatformServices const& OperatingSystem::platform() const {
	return _core.platform();
}

} // namespace lexxos::os

int main() {
	namespace lcd = lexxos::app::picocalc_lcd_console;
	lexxos::platform::PlatformServices services{.clock = {.uptimeMs = uptime}};
	lexxos::os::OperatingSystem os{services};
	Panel panel;
	os.devices().initialize();
	expect(os.devices().registerDisplayDevice(panel.name(), "test", panel), "register display");
	lcd::detail::LcdConsoleRenderer renderer;
	renderer.initialize(os);
	expect(renderer.isReady(), "renderer initialize");
	std::string report;
	for (unsigned i = 0; i < 100; ++i) {
		report += "row " + std::to_string(i) + " benchmark progress\r\n";
	}

	renderer.write(report);
	auto const before = panel.frames;
	renderer.refresh();
	expect(panel.frames == before, "refresh from shell rendered synchronously");
	clockStep = 1;
	renderer.flush();
	auto info = renderer.info();
	expect(info.queueCount > 0 && info.queueCount < report.size(), "flush must publish before consuming entire backlog");
	expect(panel.frames == before + 1 && panel.textStreams > 0, "no visible frame while backlog remains");
	expect(renderer.renderStats().charactersProcessed >= 1024 && renderer.renderStats().charactersProcessed <= 1280, "preemption must not reduce parser to single characters");
	auto const statusFrames = panel.statusStreams;
	renderer.setStatusBarInfo({.timeValid = true, .hour = 12, .minute = 30});
	auto const framesBeforeRefresh = panel.frames;
	renderer.refresh();
	expect(panel.frames == framesBeforeRefresh, "refresh bypassed mirror with dirty status/text");
	renderer.flush();
	expect(panel.statusStreams == statusFrames + 1 && renderer.info().queueCount > 0, "status waits for text backlog to empty");
	clockStep = 0;
	for (unsigned pass = 0; pass < 8 && renderer.info().queueCount; ++pass) {
		renderer.flush();
	}

	expect(renderer.info().queueCount == 0, "backlog stalled waiting for repaint strips");
	// Batched output must produce exactly the same final pixels as one snapshot.
	lexxos::os::OperatingSystem referenceOs{services};
	Panel referencePanel;
	referenceOs.devices().initialize();
	expect(referenceOs.devices().registerDisplayDevice(referencePanel.name(), "reference", referencePanel), "register reference");
	lcd::detail::LcdConsoleRenderer reference;
	reference.initialize(referenceOs);
	reference.setStatusBarInfo({.timeValid = true, .hour = 12, .minute = 30});
	reference.write(report);
	reference.flush();
	expect(reference.info().queueCount == 0 && panel.pixels == referencePanel.pixels, "batched tail snapshot contains stale/reordered rows");
	expect(panel.rgb565Writes > 0 && panel.rgb888Writes == 0, "RGB565 framebuffer forced an RGB888 round trip");
	// A resource conflict must not lose clear or the prompt written after it.
	auto lease = os.devices().lockDisplayDevice(panel.name());
	expect(bool(lease), "take display for conflict");
	renderer.clear();
	renderer.write("lexx:/> ");
	auto const clears = renderer.renderStats().clearCalls;
	renderer.flush();
	expect(renderer.info().queueCount == 8, "blocked clear discarded following prompt");
	lease.release();
	renderer.flush();
	expect(renderer.renderStats().clearCalls > clears && renderer.info().queueCount == 0, "clear retry failed to render prompt");
	for (unsigned i = 0; i < 15; ++i) {
		renderer.flush();
	}

	auto const textStreams = panel.textStreams;
	renderer.setTextModeEnabled(true); // focus reapplies the same mode
	renderer.refresh();
	renderer.flush();
	expect(panel.textStreams == textStreams, "unchanged focus forced a full repaint");
	expect(criticalDepth == 0, "critical section leaked");
}
