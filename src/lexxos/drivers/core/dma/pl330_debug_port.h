#pragma once

#include <cstdint>

namespace lexxos::drivers::pl330 {

// Register offsets are relative to DMAC. Clock must advance during polling.
// enter/leave protect only the CPU-local ownership flag, never the busy wait.
struct DebugPortAccess {
	void* context{};
	std::uint32_t (*read)(void*, std::uint32_t){};
	void (*write)(void*, std::uint32_t, std::uint32_t){};
	void (*barrier)(void*){};
	std::uint32_t (*nowMs)(void*){};
	std::uintptr_t (*enter)(void*){};
	void (*leave)(void*, std::uintptr_t){};
};

// One instance per physical debug port, shared by all channel clients.
// Task context only. Contention fails without spinning on another task.
// CPU-local serialization; this is not an SMP lock or a DMA channel allocator.
class Pl330DebugPort {
public:
	explicit Pl330DebugPort(DebugPortAccess access);
	Pl330DebugPort(Pl330DebugPort const&) = delete;
	Pl330DebugPort& operator=(Pl330DebugPort const&) = delete;

	bool startNonsecure(std::uint32_t ownedChannels, unsigned channel, std::uint32_t programAddress);
	bool kill(std::uint32_t ownedChannels, unsigned channel);

private:
	bool execute(std::uint32_t ownedChannels, unsigned channel, std::uint32_t address, bool manager);
	DebugPortAccess _access;
	bool _busy{};
};

} // namespace lexxos::drivers::pl330
