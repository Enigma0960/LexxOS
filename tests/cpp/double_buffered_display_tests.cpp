#include <array>
#include <cstdlib>
#include <functional>
#include <vector>

#include <lexxos/devices/display/double_buffered_display.h>
#include <lexxos/platform/rockchip/rk3506/dma_irq_clients.h>

namespace {

void check(bool condition) {
	if (!condition) {
		std::abort();
	}
}

unsigned criticalDepth{};
unsigned criticalEntries{}, criticalLeaves{};
std::function<void()> afterUnlock;

std::uintptr_t enter(void*) {
	++criticalEntries;
	return criticalDepth++;
}

void leave(void*, std::uintptr_t state) {
	check(criticalDepth > 0);
	criticalDepth = state;
	++criticalLeaves;

	if (!criticalDepth && afterUnlock) {
		auto callback = std::move(afterUnlock);
		afterUnlock = {};
		callback();
	}
}

class Panel final : public lexxos::devices::DisplayDevice {
public:
	std::vector<std::uint16_t> sent;
	std::vector<lexxos::devices::Rect> sentRects;
	std::function<void()> duringWrite;
	bool fail{};
	bool active{};
	int ends{};
	int scrollCalls{};
	int displayWidth{3};
	int displayHeight{2};

	bool configureVerticalScroll(int, int, int) override {
		++scrollCalls;
		return true;
	}

	bool setVerticalScrollStart(int) override {
		++scrollCalls;
		return true;
	}

	lexxos::devices::Rect rect{};

	std::string_view name() const override { return "fake"; }

	bool isReady() const override { return true; }

	int width() const override { return displayWidth; }

	int height() const override { return displayHeight; }

	lexxos::devices::PixelFormat nativeFormat() const override { return lexxos::devices::PixelFormat::Rgb565; }

	bool clear(std::uint16_t) override { return true; }

	bool beginFrame() override {
		check(criticalDepth == 0);
		check(!active);
		active = true;
		sent.clear();
		sentRects.clear();
		return true;
	}

	bool endFrame() override {
		check(criticalDepth == 0);
		check(active);
		active = false;
		++ends;
		return true;
	}

	bool blitRgb565(lexxos::devices::Rect const& r, std::uint16_t const* pixels, int stride) override {
		if (!beginRgb565Stream(r)) {
			return false;
		}

		for (int y = 0; y < r.height; ++y) {
			if (!writeRgb565Stream(pixels + y * stride, r.width)) {
				return false;
			}
		}

		return endRgb565Stream();
	}

	bool beginRgb565Stream(lexxos::devices::Rect const& r) override {
		rect = r;
		sentRects.push_back(r);
		return true;
	}

	bool writeRgb565Stream(std::uint16_t const* pixels, int count) override {
		auto before = std::vector<std::uint16_t>(pixels, pixels + count);
		if (duringWrite) {
			auto callback = std::move(duringWrite);
			duringWrite = {};
			callback();
		}

		// Пока другой кадр меняется, отправляемая память остаётся неизменной.
		check(before == std::vector<std::uint16_t>(pixels, pixels + count));
		sent.insert(sent.end(), pixels, pixels + count);
		return !fail;
	}

	bool endRgb565Stream() override { return true; }
};

lexxos::devices::DoubleBufferedDisplay* display{};
int waits{};

void wait(std::uint32_t) {
	check(criticalDepth == 0);
	++waits;
	display->service();
}

std::vector<bool> irqChanges;
bool reject{};

bool configure(bool enabled) {
	irqChanges.push_back(enabled);
	return !reject;
}

} // namespace

int main() {
	std::array<std::uint16_t, 6> a{}, b{};
	Panel panel;
	lexxos::devices::DoubleBufferedDisplay d{panel, a, b, wait};
	check(d.prefersRgb565Stream());
	display = &d;
	check(d.initialize());
	// Boot до запуска worker остаётся синхронным.
	check(d.beginFrame() && d.fillRgb565({0, 0, 3, 2}, 0) && d.endFrame());
	check(panel.sent == std::vector<std::uint16_t>(6, 0));
	panel.sent.clear();
	d.enableAsync();
	check(d.beginFrame() && d.fillRgb565({0, 0, 3, 2}, 0x1234) && d.endFrame());
	check(panel.sent.empty());
	panel.duringWrite = [&] {
		check(d.beginFrame());
		check(d.fillRgb565({1, 0, 1, 1}, 0xABCD));
	};

	check(d.service());
	check(panel.sent == std::vector<std::uint16_t>(6, 0x1234));
	check(d.endFrame() && d.waitIdle());
	check(panel.sent == std::vector<std::uint16_t>{0xABCD});
	check(panel.rect.x == 1 && panel.rect.width == 1);

	// Частичное обновление сохраняет остальной последний кадр.
	check(d.beginFrame());
	check(d.fillRgb565({0, 0, 1, 1}, 7));
	check(d.fillRgb565({2, 1, 1, 1}, 9));
	check(d.endFrame());

	// Третий кадр ждёт завершения второго, без перезаписи и потери кадров.
	check(d.beginFrame() && d.fillRgb565({0, 1, 1, 1}, 8));
	auto waited = waits;
	check(d.endFrame() && waits > waited);
	// Two disjoint updates remain separate; no enclosing 3x2 SPI transfer.
	check(panel.sent == std::vector<std::uint16_t>{7, 9});
	check(panel.sentRects.size() == 2);
	check(d.waitIdle() && panel.sent == std::vector<std::uint16_t>{8});
	check(d.beginFrame() && d.beginRgb565Stream({0, 0, 2, 1}));
	std::uint8_t rgb[]{255, 0, 0, 0, 255, 0};
	check(!d.writeRgb888Stream(rgb, 3));

	// A rejected write poisons the frame even if later writes fill the stream.
	auto const beforeRejectedFrame = panel.sent;
	check(d.writeRgb888Stream(rgb, 2) && d.endRgb565Stream());
	check(!d.endFrame() && !d.hasPendingFrame() && panel.sent == beforeRejectedFrame);
	check(d.beginFrame() && d.beginRgb565Stream({0, 0, 2, 1}));
	check(d.writeRgb888Stream(rgb, 2) && d.endRgb565Stream() && d.endFrame() && d.waitIdle());
	check(panel.sent == std::vector<std::uint16_t>{0xF800, 0x07E0});
	check(d.beginFrame() && d.beginRgb565Stream({0, 0, 2, 1}));
	check(d.writeRgb565BytesStream(rgb, 1));
	check(!d.endRgb565Stream());
	check(!d.endFrame()); // Неполный поток не публикуется.
	check(d.beginFrame() && d.fillRgb565({0, 0, 1, 1}, 5) && d.endFrame());
	panel.fail = true;
	check(!d.waitIdle() && !d.isReady() && !panel.active);
	check(!d.beginFrame());
	lexxos::devices::DoubleBufferedDisplay overlap{panel, a, a, wait};
	check(!overlap.initialize());
	lexxos::devices::DoubleBufferedDisplay small{panel, std::span{a}.first(5), b, wait};
	check(!small.initialize());

	// Product policy must reject scroll without draining a pending DMA frame.
	Panel redrawPanel;
	lexxos::devices::DoubleBufferedDisplay redraw{redrawPanel, a, b, wait, nullptr, nullptr, false};
	display = &redraw;
	check(redraw.initialize());
	redraw.enableAsync();
	check(redraw.beginFrame() && redraw.fillRgb565({0, 0, 3, 2}, 42) && redraw.endFrame());
	auto const waitsBeforeScroll = waits;
	check(!redraw.configureVerticalScroll(0, 2, 0) && !redraw.setVerticalScrollStart(1));
	check(redrawPanel.scrollCalls == 0 && waits == waitsBeforeScroll && redraw.hasPendingFrame());
	check(redraw.service() && redrawPanel.sent == std::vector<std::uint16_t>(6, 42));
	lexxos::devices::DoubleBufferedDisplay scrolling{redrawPanel, a, b, wait};
	check(scrolling.initialize());
	check(scrolling.configureVerticalScroll(0, 2, 0) && scrolling.setVerticalScrollStart(1));
	check(redrawPanel.scrollCalls == 2);

	// Force a presenter switch at the publication critical-section boundary,
	// before endFrame returns. The producer may already start its next frame.
	Panel synchronizedPanel;

	lexxos::devices::DoubleBufferedDisplay synchronized{
	    synchronizedPanel, a, b, wait, nullptr, nullptr, false, {nullptr, enter, leave}
    };

	display = &synchronized;
	check(synchronized.initialize());
	synchronized.enableAsync();
	check(synchronized.beginFrame() && synchronized.fillRgb565({0, 0, 3, 2}, 0x1234));

	synchronizedPanel.duringWrite = [&] {
		check(synchronized.beginFrame());
		check(synchronized.fillRgb565({1, 0, 1, 1}, 0xABCD));
	};

	std::function<void()> preempt = [&] {
		if (synchronized.hasPendingFrame()) {
			check(synchronized.service());
		} else {
			afterUnlock = preempt;
		}
	};

	afterUnlock = preempt;
	check(synchronized.endFrame());
	afterUnlock = {};
	check(synchronizedPanel.sent == std::vector<std::uint16_t>(6, 0x1234));
	check(synchronized.endFrame() && synchronized.waitIdle());
	check(synchronizedPanel.sent == std::vector<std::uint16_t>{0xABCD});
	check(!criticalDepth && criticalEntries == criticalLeaves);

	lexxos::devices::DoubleBufferedDisplay invalidSync{
	    synchronizedPanel, a, b, wait, nullptr, nullptr, false, {nullptr, enter, nullptr}
    };

	check(!invalidSync.initialize());

	// A HUD and an animated sprite must not force a 13x13 bounding transfer.
	std::array<std::uint16_t, 16 * 16> wideA{}, wideB{};
	Panel widePanel;
	widePanel.displayWidth = 16;
	widePanel.displayHeight = 16;
	lexxos::devices::DoubleBufferedDisplay wide{widePanel, wideA, wideB, wait};
	display = &wide;
	check(wide.initialize());
	check(wide.beginFrame());
	check(wide.fillRgb565({1, 1, 3, 3}, 0x1111));
	check(wide.fillRgb565({12, 12, 2, 2}, 0x2222));
	check(wide.endFrame());
	check(widePanel.sent.size() == 13);
	check(widePanel.sentRects.size() == 2);
	check(wide.timing().presentedPixels == 13);
	check(wide.timing().presentedRegions == 2);

	// The next back buffer must inherit BOTH regions from the last frame.
	// Five disjoint pixel writes exceed the fixed ROI limit; the widened
	// fifth ROI exposes any stale pixels from the previous frame.
	auto const copyStart = wide.timing().copiedBytes;
	check(wide.beginFrame());
	check(wide.fillRgb565({0, 0, 1, 1}, 0x3333));
	check(wide.fillRgb565({15, 15, 1, 1}, 0x4444));
	check(wide.fillRgb565({0, 15, 1, 1}, 0x5555));
	check(wide.fillRgb565({15, 0, 1, 1}, 0x6666));
	check(wide.fillRgb565({7, 7, 1, 1}, 0x7777));
	check(wide.endFrame());
	check(wide.timing().copiedBytes == copyStart + 26);
	check(widePanel.sentRects.size() <= lexxos::devices::DirtyRegionSet::maxRegions);

	// Transitive adjacent rows are coalesced into a single HUD rectangle.
	lexxos::devices::DirtyRegionSet rows;
	rows.add({2, 2, 6, 1});
	rows.add({2, 4, 6, 1});
	rows.add({2, 3, 6, 1});
	check(rows.size() == 1 && rows[0].width == 6 && rows[0].height == 3);

	// Full-screen alpha drawing must preserve the previous framebuffer.
	std::array<std::uint16_t, 6> alphaA{}, alphaB{};
	Panel alphaPanel;
	lexxos::devices::DoubleBufferedDisplay alphaDisplay{alphaPanel, alphaA, alphaB, wait};
	display = &alphaDisplay;
	check(alphaDisplay.initialize());
	check(alphaDisplay.beginFrame() && alphaDisplay.fillRgb565({0, 0, 3, 2}, 0x001F) && alphaDisplay.endFrame());
	std::uint16_t redPixels[6] = {0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800};
	std::uint8_t opacity[6] = {0, 128, 255, 255, 255, 255};
	check(alphaDisplay.beginFrame()
	    && alphaDisplay.blitRgb565Alpha({0, 0, 3, 2}, redPixels, opacity, 3)
	    && alphaDisplay.endFrame());
	check(alphaPanel.sent == std::vector<std::uint16_t>({0x001F, 0x800F, 0xF800, 0xF800, 0xF800, 0xF800}));

	lexxos::platform::rk3506::DmaIrqClients clients{configure};
	check(clients.setEnabled(0, true) && clients.setEnabled(1, true));
	check(clients.setEnabled(0, false) && irqChanges == std::vector<bool>{true});
	check(clients.setEnabled(1, true) && irqChanges.size() == 1);
	reject = true;
	check(!clients.setEnabled(1, false));
	reject = false;
	check(clients.setEnabled(1, false));
	check(irqChanges == std::vector<bool>{true, false, false});
	check(!clients.setEnabled(2, true));
	check(clients.setEnabled(1, true) && clients.setEnabled(0, true));
	check(clients.setEnabled(1, false));
	check(clients.setEnabled(0, false));
}
