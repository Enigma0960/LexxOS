#include <cstdint>
#include <cstdlib>
#include <functional>
#include <utility>
#include <vector>

#include <lexxos/drivers/core/dma/pl330_debug_port.h>

namespace {

void check(bool value) {
	if (!value) {
		std::abort();
	}
}

struct Fake {
	bool masked{};
	bool busy{};
	unsigned enters{}, leaves{};
	std::uint32_t now{};
	std::vector<std::pair<std::uint32_t, std::uint32_t>> trace;
	std::function<void()> nested;

	lexxos::drivers::pl330::DebugPortAccess access() {
		return {
		    this,
		    [](void* p, std::uint32_t offset) {
			    auto& self = *static_cast<Fake*>(p);
			    check(offset == 0xD00);
			    self.trace.emplace_back(offset, 0);
			    return self.busy ? 1U : 0U;
		    },
		    [](void* p, std::uint32_t offset, std::uint32_t value) {
			    auto& self = *static_cast<Fake*>(p);
			    self.trace.emplace_back(offset, value);
			    if (self.nested) {
				    auto callback = std::move(self.nested);
				    self.nested = {};
				    callback();
			    }
		    },
		    [](void* p) { static_cast<Fake*>(p)->trace.emplace_back(0, 0); },
		    [](void* p) { return static_cast<Fake*>(p)->now++; },
		    [](void* p) -> std::uintptr_t {
			    auto& self = *static_cast<Fake*>(p);
			    auto const previous = self.masked;
			    self.masked = true;
			    ++self.enters;
			    return previous;
		    },
		    [](void* p, std::uintptr_t state) {
			    auto& self = *static_cast<Fake*>(p);
			    check(self.masked);
			    self.masked = state != 0;
			    ++self.leaves;
		    },
		};
	}
};

} // namespace

int main() {
	using lexxos::drivers::pl330::Pl330DebugPort;
	Fake fake;
	Pl330DebugPort port{fake.access()};
	// No callbacks or MMIO for invalid channel ownership / program address.
	check(!port.startNonsecure(3, 2, 0x02001000));
	check(!port.kill(4, 0) && !port.kill(~0U, 8) && !port.kill(~0U, 32));
	check(!port.startNonsecure(4, 2, 0));
	check(fake.trace.empty() && fake.enters == 0);
	Pl330DebugPort unavailable{{}};
	check(!unavailable.kill(4, 2));

	// Reentrant SD submission cannot overwrite the LCD debug instruction pair.
	fake.nested = [&] {
		check(!fake.masked); // Hardware writes do not hold the IRQ mask.
		check(!port.startNonsecure(3, 0, 0x02002000));
	};
	check(port.startNonsecure(4, 2, 0x02001000));
	using Trace = std::vector<std::pair<std::uint32_t, std::uint32_t>>;
	check(fake.trace == Trace{
	          {0xD00,          0},
              {0xD08, 0x02A20000},
              {0xD0C, 0x02001000},
              {    0,          0},
              {0xD04,          0},
              {    0,          0}
    });
	check(!fake.masked && fake.enters == fake.leaves);

	fake.trace.clear();
	check(port.kill(3, 1));
	check(fake.trace == Trace{
	          {0xD00,          0},
              {0xD08, 0x00010101},
              {0xD0C,          0},
              {    0,          0},
              {0xD04,          0},
              {    0,          0}
    });

	// Timer wraps; timeout makes no writes and releases ownership for recovery.
	fake.trace.clear();
	fake.busy = true;
	fake.now = UINT32_MAX - 9U;
	check(!port.startNonsecure(3, 0, 0x02002000));
	check(fake.trace.size() == 20);
	for (auto const& entry : fake.trace) {
		check(entry.first == 0xD00);
	}

	check(!fake.masked && fake.enters == fake.leaves);
	fake.busy = false;
	fake.masked = true;
	check(port.kill(4, 2) && fake.masked); // Restore caller's IRQ mask.
	check(fake.enters == fake.leaves);

	// Separate controllers have independent ownership state.
	Fake other;
	Pl330DebugPort second{other.access()};
	fake.nested = [&] {
		check(second.kill(3, 0));
	};
	check(port.kill(4, 2));
	check(!other.trace.empty());
}
