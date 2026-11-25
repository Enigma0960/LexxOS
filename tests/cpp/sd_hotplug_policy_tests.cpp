#include <array>
#include <cstdlib>
#include <iostream>

#include <lexxos/app/sd_hotplug_policy.h>

namespace {

void expect(bool condition, char const* message) {
	if (!condition) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

struct FakeMedia {
	std::array<bool, 8> samples{false, true, false, true, true, true, true, true};
	unsigned position{0};
	std::uint32_t elapsedMs{0};
	std::uint32_t stopAt{1000};
};

bool sample(void* context) {
	auto& media = *static_cast<FakeMedia*>(context);
	return media.samples[media.position < media.samples.size() ? media.position++ : media.samples.size() - 1];
}

void sleep(void* context, std::uint32_t ms) {
	static_cast<FakeMedia*>(context)->elapsedMs += ms;
}

bool stopped(void* context) {
	auto& media = *static_cast<FakeMedia*>(context);
	return media.elapsedMs >= media.stopAt;
}

} // namespace

int main() {
	lexxos::app::SdHotplugPolicy left, right;
	left.initialize(false);
	right.initialize(true);
	expect(!left.observe(true) && !left.observe(false), "bounce must not change stable media state");
	expect(!left.observe(true) && !left.observe(true) && left.observe(true), "three samples must produce insertion");
	expect(!left.observe(true) && !right.observe(true), "stable state must not produce duplicate events");
	expect(!right.observe(false) && !right.observe(false) && right.observe(false), "removal requires three samples");
	expect(!left.observe(true), "another media policy cannot affect insertion state");
	expect(left.acceptStable(false) && !left.acceptStable(false), "debounced IRQ changes state once");
	lexxos::app::SdHotplugPolicy fresh;
	expect(!fresh.acceptStable(true), "initial stable sample does not announce hotplug");
	FakeMedia media;
	bool inserted = false;
	lexxos::app::SdDetectSource source{&media, sample, sleep, stopped};
	expect(lexxos::app::SdHotplugPolicy::readStable(source, inserted) && inserted, "debounced read failed");
	expect(media.elapsedMs == 125, "debounce must use 25 ms intervals and restart on bounce");
	FakeMedia cancelled;
	cancelled.stopAt = 50;
	source.context = &cancelled;
	expect(!lexxos::app::SdHotplugPolicy::readStable(source, inserted), "shutdown must stop unstable read");
	expect(cancelled.elapsedMs == 50, "shutdown wait exceeded");
	expect(!lexxos::app::SdHotplugPolicy::readStable({}, inserted), "missing backend must be rejected");
	return 0;
}
