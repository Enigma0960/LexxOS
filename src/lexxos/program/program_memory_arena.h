#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/program/program.h>

namespace lexxos::program {

enum class ProgramMemoryArenaStatus : std::uint8_t {
	Ready,
	ExternalUnavailable,
	InternalUnavailable,
};

std::string_view toString(ProgramMemoryArenaStatus status);

struct ProgramMemoryArenaSnapshot {
	ProgramMemoryClass memoryClass{ProgramMemoryClass::InternalOnly};
	std::uint32_t requestedExternalBytes{0};
	std::uint32_t reservedExternalBytes{0};
	std::uint32_t requestedInternalBytes{0};
	std::uint32_t reservedInternalBytes{0};
	ProgramMemoryArenaStatus status{ProgramMemoryArenaStatus::Ready};
	bool active{false};
};

class ProgramMemoryArena final {
public:
	ProgramMemoryArena() = default;
	ProgramMemoryArena(ProgramMemoryArena const&) = delete;
	ProgramMemoryArena& operator=(ProgramMemoryArena const&) = delete;
	~ProgramMemoryArena();

	[[nodiscard]] ProgramMemoryArenaStatus reserve(ProgramRuntimeProfile const& runtime);
	void release();

	[[nodiscard]] void* detachInternalStorage();
	static void freeDetachedInternalStorage(void* memory);

	[[nodiscard]] void* allocate(std::size_t size, std::size_t alignment);
	[[nodiscard]] ProgramMemoryArenaSnapshot snapshot() const;
	[[nodiscard]] bool active() const;

private:
	ProgramMemoryArenaSnapshot _snapshot{};
	void* _internalStorage{nullptr};
	std::uint32_t _internalStorageBytes{0};
	std::uint32_t _internalUsedBytes{0};
};

} // namespace lexxos::program
